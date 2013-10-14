/* -*- Mode: C; c-basic-offset: 4 -*-
 * vim: tabstop=4 shiftwidth=4 expandtab
 *
 * Copyright (C) 2011 John (J5) Palmieri <johnp@redhat.com>
 * Copyright (C) 2013 Simon Feltman <sfeltman@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

#include "pygi-arg-callback.h"
#include "pygi-private.h"

#define USER_DATA_INDEX 0
#define DESTROY_NOTIFY_INDEX 1

typedef struct _PyGICallbackCache
{
    PyGIArgCache arg_cache;
    GIScopeType scope;
    GIInterfaceInfo *interface_info;
} PyGICallbackCache;


/* _pygi_destroy_notify_dummy:
 *
 * Dummy method used in the occasion when a method has a GDestroyNotify
 * argument without user data.
 */
static void
_pygi_destroy_notify_dummy (gpointer data) {
}

static PyGICClosure *global_destroy_notify;

static void
_pygi_destroy_notify_callback_closure (ffi_cif *cif,
                                       void *result,
                                       void **args,
                                       void *data)
{
    PyGICClosure *info = * (void**) (args[0]);

    g_assert (info);

    _pygi_invoke_closure_free (info);
}

/* _pygi_destroy_notify_create:
 *
 * Method used in the occasion when a method has a GDestroyNotify
 * argument with user data.
 */
static PyGICClosure*
_pygi_destroy_notify_create (void)
{
    if (!global_destroy_notify) {

        PyGICClosure *destroy_notify = g_slice_new0 (PyGICClosure);
        GIBaseInfo* glib_destroy_notify;

        g_assert (destroy_notify);

        glib_destroy_notify = g_irepository_find_by_name (NULL, "GLib", "DestroyNotify");
        g_assert (glib_destroy_notify != NULL);
        g_assert (g_base_info_get_type (glib_destroy_notify) == GI_INFO_TYPE_CALLBACK);

        destroy_notify->closure = g_callable_info_prepare_closure ( (GICallableInfo*) glib_destroy_notify,
                                                                    &destroy_notify->cif,
                                                                    _pygi_destroy_notify_callback_closure,
                                                                    NULL);

        global_destroy_notify = destroy_notify;
    }

    return global_destroy_notify;
}

static gboolean
_pygi_marshal_from_py_interface_callback (PyGIInvokeState   *state,
                                          PyGICallableCache *callable_cache,
                                          PyGIArgCache      *arg_cache,
                                          PyObject          *py_arg,
                                          GIArgument        *arg,
                                          gpointer          *cleanup_data)
{
    GICallableInfo *callable_info;
    PyGICClosure *closure;
    PyGIArgCache *user_data_cache = NULL;
    PyGIArgCache *destroy_cache = NULL;
    PyGICallbackCache *callback_cache;
    PyObject *py_user_data = NULL;

    callback_cache = (PyGICallbackCache *)arg_cache;

    user_data_cache = pygi_callable_cache_get_arg_child (callable_cache, arg_cache, USER_DATA_INDEX);
    if (user_data_cache != NULL) {
        if (user_data_cache->py_arg_index < state->n_py_in_args) {
            /* py_user_data is a borrowed reference. */
            py_user_data = PyTuple_GetItem (state->py_in_args, user_data_cache->py_arg_index);
            if (!py_user_data)
                return FALSE;
            /* NULL out user_data if it was not supplied and the default arg placeholder
             * was used instead.
             */
            if (py_user_data == _PyGIDefaultArgPlaceholder)
                py_user_data = NULL;
        }
    }

    if (py_arg == Py_None) {
        return TRUE;
    }

    if (!PyCallable_Check (py_arg)) {
        PyErr_Format (PyExc_TypeError,
                      "Callback needs to be a function or method not %s",
                      py_arg->ob_type->tp_name);

        return FALSE;
    }

    callable_info = (GICallableInfo *)callback_cache->interface_info;

    closure = _pygi_make_native_closure (callable_info, callback_cache->scope, py_arg, py_user_data);
    arg->v_pointer = closure->closure;

    /* The PyGICClosure instance is used as user data passed into the C function.
     * The return trip to python will marshal this back and pull the python user data out.
     */
    if (user_data_cache != NULL) {
        state->in_args[user_data_cache->c_arg_index].v_pointer = closure;
    }

    /* Setup a GDestroyNotify callback if this method supports it along with
     * a user data field. The user data field is a requirement in order
     * free resources and ref counts associated with this arguments closure.
     * In case a user data field is not available, show a warning giving
     * explicit information and setup a dummy notification to avoid a crash
     * later on in _pygi_destroy_notify_callback_closure.
     */
    destroy_cache = pygi_callable_cache_get_arg_child (callable_cache, arg_cache, DESTROY_NOTIFY_INDEX);
    if (destroy_cache) {
        if (user_data_cache != NULL) {
            PyGICClosure *destroy_notify = _pygi_destroy_notify_create ();
            state->in_args[destroy_cache->c_arg_index].v_pointer = destroy_notify->closure;
        } else {
            gchar *msg = g_strdup_printf("Callables passed to %s will leak references because "
                                         "the method does not support a user_data argument. "
                                         "See: https://bugzilla.gnome.org/show_bug.cgi?id=685598",
                                         callable_cache->name);
            if (PyErr_WarnEx(PyExc_RuntimeWarning, msg, 2)) {
                g_free(msg);
                _pygi_invoke_closure_free(closure);
                return FALSE;
            }
            g_free(msg);
            state->in_args[destroy_cache->c_arg_index].v_pointer = _pygi_destroy_notify_dummy;
        }
    }

    /* Use the PyGIClosure as data passed to cleanup for GI_SCOPE_TYPE_CALL. */
    *cleanup_data = closure;

    return TRUE;
}

static PyObject *
_pygi_marshal_to_py_interface_callback (PyGIInvokeState   *state,
                                        PyGICallableCache *callable_cache,
                                        PyGIArgCache      *arg_cache,
                                        GIArgument        *arg)
{
    PyObject *py_obj = NULL;

    PyErr_Format (PyExc_NotImplementedError,
                  "Marshalling a callback to PyObject is not supported");
    return py_obj;
}

static void
_callback_cache_free_func (PyGICallbackCache *cache)
{
    if (cache != NULL) {
        if (cache->interface_info != NULL)
            g_base_info_unref ( (GIBaseInfo *)cache->interface_info);

        g_slice_free (PyGICallbackCache, cache);
    }
}

static void
_pygi_marshal_cleanup_from_py_interface_callback (PyGIInvokeState *state,
                                                  PyGIArgCache    *arg_cache,
                                                  PyObject        *py_arg,
                                                  gpointer         data,
                                                  gboolean         was_processed)
{
    PyGICallbackCache *callback_cache = (PyGICallbackCache *)arg_cache;
    if (was_processed && callback_cache->scope == GI_SCOPE_TYPE_CALL) {
        _pygi_invoke_closure_free (data);
    }
}

void
pygi_arg_callback_setup_child_args (PyGIArgCache *arg_cache,
                                    PyGICallableCache *callable_cache)
{
    if (pygi_arg_base_has_child_arg (arg_cache, USER_DATA_INDEX)) {
        PyGIArgCache *user_data_arg_cache = _arg_cache_alloc ();
        user_data_arg_cache->meta_type = PYGI_META_ARG_TYPE_CHILD_WITH_PYARG;
        user_data_arg_cache->direction = PYGI_DIRECTION_FROM_PYTHON;
        user_data_arg_cache->has_default = TRUE; /* always allow user data with a NULL default. */
        _pygi_callable_cache_set_arg (callable_cache,
                                      pygi_arg_base_get_child_arg (arg_cache, USER_DATA_INDEX),
                                      user_data_arg_cache);
    }

    if (pygi_arg_base_has_child_arg (arg_cache, DESTROY_NOTIFY_INDEX)) {
        PyGIArgCache *destroy_arg_cache = _arg_cache_alloc ();
        destroy_arg_cache->meta_type = PYGI_META_ARG_TYPE_CHILD;
        destroy_arg_cache->direction = PYGI_DIRECTION_FROM_PYTHON;
        _pygi_callable_cache_set_arg (callable_cache,
                                      pygi_arg_base_get_child_arg (arg_cache, DESTROY_NOTIFY_INDEX),
                                      destroy_arg_cache);
    }
}

static gboolean
pygi_arg_callback_setup_from_info (PyGIArgCache       *arg_cache,
                                   GITypeInfo         *type_info,
                                   GIArgInfo          *arg_info,   /* may be null */
                                   GITransfer          transfer,
                                   PyGIDirection       direction,
                                   GIInterfaceInfo    *iface_info,
                                   PyGICallableCache  *callable_cache)
{
    if (!pygi_arg_base_setup (arg_cache,
                              type_info,
                              arg_info,
                              transfer,
                              direction)) {
        return FALSE;
    }

    arg_cache->supports_child_args = TRUE;

    if (direction & PYGI_DIRECTION_TO_PYTHON) {
        arg_cache->to_py_marshaller = _pygi_marshal_to_py_interface_callback;
    }

    arg_cache->destroy_notify = (GDestroyNotify)_callback_cache_free_func;

    pygi_arg_base_set_child_arg (arg_cache, USER_DATA_INDEX,
                                 g_arg_info_get_closure (arg_info));

    pygi_arg_base_set_child_arg (arg_cache, DESTROY_NOTIFY_INDEX,
                                 g_arg_info_get_destroy (arg_info));

    ((PyGICallbackCache *)arg_cache)->scope = g_arg_info_get_scope (arg_info);
    g_base_info_ref( (GIBaseInfo *)iface_info);
    ((PyGICallbackCache *)arg_cache)->interface_info = iface_info;

    if (direction & PYGI_DIRECTION_FROM_PYTHON) {
        arg_cache->from_py_marshaller = _pygi_marshal_from_py_interface_callback;
        arg_cache->from_py_cleanup = _pygi_marshal_cleanup_from_py_interface_callback;
    }

    return TRUE;
}

PyGIArgCache *
pygi_arg_callback_new_from_info  (GITypeInfo        *type_info,
                                  GIArgInfo         *arg_info,   /* may be null */
                                  GITransfer         transfer,
                                  PyGIDirection      direction,
                                  GIInterfaceInfo   *iface_info,
                                  PyGICallableCache *callable_cache)
{
    gboolean res = FALSE;
    PyGIArgCache *arg_cache;

    arg_cache = (PyGIArgCache *)g_slice_new0 (PyGICallbackCache);
    if (arg_cache == NULL)
        return NULL;

    res = pygi_arg_callback_setup_from_info (arg_cache,
                                             type_info,
                                             arg_info,
                                             transfer,
                                             direction,
                                             iface_info,
                                             callable_cache);
    if (res) {
        return arg_cache;
    } else {
        _pygi_arg_cache_free (arg_cache);
        return NULL;
    }
}
