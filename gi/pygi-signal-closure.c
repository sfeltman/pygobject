/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2011  Laszlo Pandy <lpandy@src.gnome.org>
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "pygi-private.h"
#include "pygi-value.h"

static GISignalInfo *
_pygi_lookup_signal_from_g_type (GType g_type,
                                 const gchar *signal_name)
{
    GIRepository *repository;
    GIBaseInfo *info;
    GISignalInfo *signal_info = NULL;

    repository = g_irepository_get_default();
    info = g_irepository_find_by_gtype (repository, g_type);
    if (info == NULL)
        return NULL;

    if (GI_IS_OBJECT_INFO (info))
        signal_info = g_object_info_find_signal ((GIObjectInfo *) info,
                                                 signal_name);
    else if (GI_IS_INTERFACE_INFO (info))
        signal_info = g_interface_info_find_signal ((GIInterfaceInfo *) info,
                                                    signal_name);

    g_base_info_unref (info);
    return signal_info;
}

static void
signal_closure_invalidate (gpointer data, GClosure *closure)
{
    _pygi_invoke_closure_free (data);
}

/*
 * signal_closure_call:
 *
 * Specialized call which always tacks user_data onto the tail of the
 * Python arguments and optionally replaces the first argument with swap_data.
 */
static PyObject *
signal_closure_call (PyGICClosure *closure, PyObject *args)
{
    PyObject *result = NULL;
    PyObject *new_args;

    if (closure->user_data) {
        new_args = PySequence_Concat (args, closure->user_data);
        if (new_args == NULL) {
            return NULL;
        }
    } else {
        Py_INCREF (args);
        new_args = args;
    }

    if (closure->swap_data) {
        PyObject *list = PySequence_List (new_args);
        if (list == NULL) {
            goto out;
        }

        Py_INCREF (closure->swap_data);
        PyList_SetItem (list, 0, closure->swap_data);

        Py_DECREF (new_args);
        new_args = list;
    }

    result = PyObject_CallObject ((PyObject *)closure->function, new_args);

out:
    Py_DECREF (new_args);
    return result;
}

GClosure *
pygi_signal_closure_new (PyGObject *instance,
                         GType g_type,
                         const gchar *signal_name,
                         PyObject *callback,
                         PyObject *extra_args,
                         PyObject *swap_data)
{
    GClosure *closure = NULL;
    PyGICClosure *pygi_closure = NULL;
    GISignalInfo *signal_info = NULL;

    g_return_val_if_fail(callback != NULL, NULL);

    signal_info = _pygi_lookup_signal_from_g_type (g_type, signal_name);
    if (signal_info == NULL)
        return NULL;

    pygi_closure = _pygi_make_native_closure (signal_info,
                                              GI_SCOPE_TYPE_NOTIFIED,
                                              callback,
                                              extra_args,
                                              swap_data);

    if (pygi_closure) {
        pygi_closure->call = signal_closure_call;

        closure = g_cclosure_new (G_CALLBACK (pygi_closure->closure),
                                  pygi_closure,
                                  signal_closure_invalidate);
    }

    g_base_info_unref (signal_info);
    return closure;
}
