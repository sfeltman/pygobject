/* -*- Mode: C; c-basic-offset: 4 -*-
 * pygobject - Python bindings for the GNOME platform.
 * Copyright (C) 2014  Simon Feltman
 *
 *   pygi-wrapper.c: Generic Python wrappers for pointers with
 *                   optional memory management.
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <pyglib.h>

#include "pygobject-private.h"
#include "pygi-wrapper.h"
#include "pygi-type.h"
#include "pygi-info.h"

GQuark pygi_wrapper_class_key;

PYGLIB_DEFINE_TYPE("gi.Wrapper", PyGIWrapper_Type, PyGIWrapper);

#define PYGI_WRAPPER_FUNCS_ATTR_NAME "__pygi_wrapper_funcs__"

static void
_pygi_wrapper_funcs_free (PyObject *capsule)
{
    PyGIWrapperFuncs *funcs = PyCapsule_GetPointer (capsule, NULL);
    if (funcs != NULL) {
        g_free (funcs);
        PyCapsule_SetPointer (capsule, NULL);
    }
}

static int
_pygi_wrapper_funcs_attach_capsule (PyObject *obj, PyObject *capsule)
{
    return PyObject_SetAttrString (obj, PYGI_WRAPPER_FUNCS_ATTR_NAME, capsule);
}

static PyObject *
_pygi_wrapper_funcs_get_capsule (PyObject *obj)
{
    return PyObject_GetAttrString (obj, PYGI_WRAPPER_FUNCS_ATTR_NAME);
}

/**
 * pygi_wrapper_funcs_attach:
 * @obj: Python object to attach wrapper functions to. This may be a Python
 *   class or instance.
 * @copy_func: A function for copying/reffing a pointer held by the wrapper.
 * @free_func: A function for freeing/unreffing a pointer held by the wrapper.
 *
 * Create and attach a new #PyGIWrapperFuncs structure with the given functions.
 *
 * Returns: 0 on success and -1 on error
 */
int
pygi_wrapper_funcs_attach (PyObject            *obj,
                           PyGIWrapper_CopyFunc copy_func,
                           PyGIWrapper_FreeFunc free_func)
{
    PyObject *capsule;
    PyGIWrapperFuncs *funcs;

    funcs = g_try_malloc0(sizeof(PyGIWrapperFuncs));
    if (funcs == NULL) {
        PyErr_SetNone (PyExc_MemoryError);
        return -1;
    }

    capsule = PyCapsule_New (funcs, NULL, (PyCapsule_Destructor)_pygi_wrapper_funcs_free);
    if (capsule == NULL) {
        g_free (funcs);
        return -1;
    }

    if (_pygi_wrapper_funcs_attach_capsule (obj, capsule) < 0) {
        Py_DECREF (capsule);  /* handles freeing funcs with the PyCapsule_Destructor */
        return -1;
    }

    return 0;
}

/**
 * pygi_wrapper_funcs_attach_static:
 * @obj: Python object to attach wrapper functions to. This may be a Python
 *   class or instance.
 * @funcs: Wrapper functions structure already created and filled out.
 *   The memory of this structure will not be managed and is expected to be
 *   statically created.
 *
 * Attach a pre-created set of wrapper functions to the given Python object.
 *
 * Returns: -1 on failure
 */
int
pygi_wrapper_funcs_attach_static (PyObject         *obj,
                                  PyGIWrapperFuncs *funcs)
{
    PyObject *capsule = PyCapsule_New (funcs, NULL, NULL);
    if (capsule != NULL) {
        return _pygi_wrapper_funcs_attach_capsule (obj, capsule);
    }
    return -1;
}

/**
 * pygi_wrapper_funcs_get:
 * @obj: Python object to attach wrapper functions to. This may be a Python
 *   class or instance.
 *
 * Returns: A #PyGIWrapperFuncs instance associated with the @obj or its class.
 */
PyGIWrapperFuncs *
pygi_wrapper_funcs_get (PyObject *obj)
{
    PyObject *capsule = _pygi_wrapper_funcs_get_capsule (obj);
    if (capsule == NULL) {
        return NULL;
    }

    return (PyGIWrapperFuncs *)PyCapsule_GetPointer (capsule, NULL);
}

static PyObject*
_pygi_wrapper_richcompare (PyObject *self, PyObject *other, int op)
{
    if (Py_TYPE(self) == Py_TYPE(other))
        return _pyglib_generic_ptr_richcompare (pygi_wrapper_get (self, void),
                                                pygi_wrapper_get (other, void),
                                                op);
    else {
        Py_INCREF (Py_NotImplemented);
        return Py_NotImplemented;
    }
}

static long
_pygi_wrapper_hash (PyGIWrapper *self)
{
    return (long)pygi_wrapper_get (self, void);
}

static PyObject *
_pygi_wrapper_repr (PyGIWrapper *self)
{
    return PYGLIB_PyUnicode_FromFormat ("<%s at %p>",
                                        Py_TYPE(self)->tp_name,
                                        pygi_wrapper_get (self, void));
}

static void
_pygi_wrapper_dealloc (PyGIWrapper *self)
{
    PyGIWrapperFuncs *funcs = pygi_wrapper_funcs_get ((PyObject *)self);
    if (funcs && funcs->free && self->wrapped) {
        funcs->free (self->wrapped);
        self->wrapped = NULL;
    }
    Py_TYPE(self)->tp_free ((PyObject *)self);
}

static PyObject *
_pygi_wrapper_class_setup_memory_management_from_gi_info (PyObject *wrapper_class,
                                                          PyObject *unused)
{
    GIObjectInfo *info;

    info = (GIObjectInfo *)_pygi_object_get_gi_info (wrapper_class, &PyGIObjectInfo_Type);
    if (info == NULL) {
        return NULL;
    }

    if (pygi_wrapper_funcs_attach (wrapper_class,
                                   g_object_info_get_ref_function_pointer (info),
                                   g_object_info_get_unref_function_pointer (info))) {
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyMethodDef _pygi_wrapper_methods[] = {
    { "_setup_memory_management_from_gi_info",
        (PyCFunction)_pygi_wrapper_class_setup_memory_management_from_gi_info,
        METH_CLASS | METH_NOARGS },
    { NULL, NULL, 0 }
};

void
pygi_wrapper_register_types(PyObject *module)
{
    pygi_wrapper_class_key = g_quark_from_static_string("PyGIWrapper::class");

    Py_TYPE(&PyGIWrapper_Type) = &PyType_Type;
    PyGIWrapper_Type.tp_dealloc = (destructor)_pygi_wrapper_dealloc;
    PyGIWrapper_Type.tp_richcompare = _pygi_wrapper_richcompare;
    PyGIWrapper_Type.tp_repr = (reprfunc)_pygi_wrapper_repr;
    PyGIWrapper_Type.tp_hash = (hashfunc)_pygi_wrapper_hash;
    PyGIWrapper_Type.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    PyGIWrapper_Type.tp_methods = _pygi_wrapper_methods;

    if (PyType_Ready (&PyGIWrapper_Type))
        return;
    if (PyModule_AddObject (module, "Wrapper", (PyObject *) &PyGIWrapper_Type))
        return;
}
