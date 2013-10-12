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


typedef struct _PyGICallbackCache
{
    PyGIArgCache arg_cache;
    gssize user_data_index;
    gssize destroy_notify_index;
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

    if (callback_cache->user_data_index > 0) {
        user_data_cache = _pygi_callable_cache_get_arg (callable_cache, callback_cache->user_data_index);
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
    if (callback_cache->destroy_notify_index > 0) {
        destroy_cache = _pygi_callable_cache_get_arg (callable_cache, callback_cache->destroy_notify_index);
    }

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

static void
_arg_cache_from_py_interface_callback_setup (PyGIArgCache *arg_cache,
                                             PyGICallableCache *callable_cache)
{
    PyGICallbackCache *callback_cache = (PyGICallbackCache *)arg_cache;
    if (callback_cache->user_data_index >= 0) {
        PyGIArgCache *user_data_arg_cache = _arg_cache_alloc ();
        user_data_arg_cache->meta_type = PYGI_META_ARG_TYPE_CHILD_WITH_PYARG;
        user_data_arg_cache->direction = PYGI_DIRECTION_FROM_PYTHON;
        user_data_arg_cache->has_default = TRUE; /* always allow user data with a NULL default. */
        _pygi_callable_cache_set_arg (callable_cache, callback_cache->user_data_index,
                                      user_data_arg_cache);
    }

    if (callback_cache->destroy_notify_index >= 0) {
        PyGIArgCache *destroy_arg_cache = _arg_cache_alloc ();
        destroy_arg_cache->meta_type = PYGI_META_ARG_TYPE_CHILD;
        destroy_arg_cache->direction = PYGI_DIRECTION_FROM_PYTHON;
        _pygi_callable_cache_set_arg (callable_cache, callback_cache->destroy_notify_index,
                                      destroy_arg_cache);
    }
    arg_cache->from_py_marshaller = _pygi_marshal_from_py_interface_callback;
    arg_cache->from_py_cleanup = _pygi_marshal_cleanup_from_py_interface_callback;
}


static gboolean
pygi_arg_callback_setup_from_info (PyGICallbackCache  *arg_cache,
                                   GITypeInfo         *type_info,
                                   GIArgInfo          *arg_info,   /* may be null */
                                   GITransfer          transfer,
                                   PyGIDirection       direction,
                                   GIInterfaceInfo    *iface_info,
                                   PyGICallableCache  *callable_cache)
{
    gssize child_offset = 0;

    if (!pygi_arg_base_setup ((PyGIArgCache *)arg_cache,
                              type_info,
                              arg_info,
                              transfer,
                              direction)) {
        return FALSE;
    }

    if (direction & PYGI_DIRECTION_TO_PYTHON) {
        ((PyGIArgCache *)arg_cache)->to_py_marshaller = _pygi_marshal_to_py_interface_callback;
    }

    if (callable_cache != NULL)
        child_offset =
            (callable_cache->function_type == PYGI_FUNCTION_TYPE_METHOD ||
                 callable_cache->function_type == PYGI_FUNCTION_TYPE_VFUNC) ? 1: 0;

    ( (PyGIArgCache *)arg_cache)->destroy_notify = (GDestroyNotify)_callback_cache_free_func;

    arg_cache->user_data_index = g_arg_info_get_closure (arg_info);
    if (arg_cache->user_data_index != -1)
        arg_cache->user_data_index += child_offset;
    arg_cache->destroy_notify_index = g_arg_info_get_destroy (arg_info);
    if (arg_cache->destroy_notify_index != -1)
        arg_cache->destroy_notify_index += child_offset;
    arg_cache->scope = g_arg_info_get_scope (arg_info);
    g_base_info_ref( (GIBaseInfo *)iface_info);
    arg_cache->interface_info = iface_info;

    if (direction & PYGI_DIRECTION_FROM_PYTHON)
        _arg_cache_from_py_interface_callback_setup ((PyGIArgCache *)arg_cache, callable_cache);

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
    PyGICallbackCache *callback_cache;

    callback_cache = g_slice_new0 (PyGICallbackCache);
    if (callback_cache == NULL)
        return NULL;

    res = pygi_arg_callback_setup_from_info (callback_cache,
                                             type_info,
                                             arg_info,
                                             transfer,
                                             direction,
                                             iface_info,
                                             callable_cache);
    if (res) {
        return (PyGIArgCache *)callback_cache;
    } else {
        _pygi_arg_cache_free ((PyGIArgCache *)callback_cache);
        return NULL;
    }
}
