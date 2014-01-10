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

/**
 * pygi_wrapper_class_from_gtype:
 * @gtype: GType which may hold a Python wrapper class.
 *
 * Returns: A Python class associated with @gtype or NULL.
 */
PyObject *
pygi_wrapper_class_from_gtype (GType gtype)
{
    PyObject *pytype = NULL;

    pytype = g_type_get_qdata (gtype, pygi_wrapper_class_key);

    if (!pytype)
        pytype = pygi_type_import_by_g_type_real (gtype);

    return pytype;
}

/**
 * pygi_wrapper_class_from_object_info:
 * @info: GIObjectInfo which may have an associated Python wrapper class.
 *
 * Returns: A Python class associated with @info or NULL.
 */
PyObject *
pygi_wrapper_class_from_object_info (GIObjectInfo *info)
{
    PyObject *pytype = NULL;
    GType gtype = g_registered_type_info_get_g_type ((GIRegisteredTypeInfo *)info);

    if (gtype != G_TYPE_NONE) {
        pytype = pygi_wrapper_class_from_gtype (gtype);
    }

    if (pytype == NULL) {
        pytype = _pygi_type_import_by_name (g_base_info_get_namespace (info),
                                            g_base_info_get_name (info));
    }

    return pytype;
}

/**
 * pygi_wrapper_class_new_full:
 * @class_name: Python class name of wrapper class to be created.
 * @module_name: Python module name of wrapper class to be created.
 * @gtype: GType to associate the wrapper class with (allows G_TYPE_NONE).
 * @copy_func: (allow-none): A function for copying/reffing a pointer held by
 *   the new wrapper.
 * @free_func: (allow-none): A function for freeing/unreffing a pointer held by
 *   the new wrapper.
 *
 * Returns: A new Python class which allows wrapping a pointer with memory management.
 */
PyObject *
pygi_wrapper_class_new_full (const char *class_name,
                             const char *module_name,
                             GType gtype,
                             PyGIWrapper_CopyFunc copy_func,
                             PyGIWrapper_FreeFunc free_func)
{
    int res;
    PyObject *class_dict;
    PyObject *wrapper_class;
    PyObject *py_module_name;

    if (gtype != G_TYPE_NONE) {
        wrapper_class = pygi_wrapper_class_from_gtype (gtype);
        if (wrapper_class != NULL) {
            return wrapper_class;
        }
    }

    class_dict = PyDict_New();
    if (class_dict == NULL) {
        return NULL;
    }

    /* Create a new class derived from PyGIWrapper. This is the same as:
     * >>> wrapper_class = type(typename, (gi.Wrapper,), {})
     */
    wrapper_class = PyObject_CallFunction ((PyObject *)&PyType_Type, "s(O)O",
                                           class_name,
                                           (PyObject *)&PyGIWrapper_Type,
                                           class_dict);
    Py_DECREF (class_dict);
    if (wrapper_class == NULL) {
        return NULL;
    }

    py_module_name = PYGLIB_PyUnicode_FromString (module_name);
    res = PyObject_SetAttrString (wrapper_class, "__module__", py_module_name);
    Py_DECREF (py_module_name);
    if (res < 0) {
        Py_DECREF (wrapper_class);
        return NULL;
    }

    res = pygi_wrapper_funcs_attach (wrapper_class,
                                     copy_func,
                                     free_func);
    if (res < 0) {
        Py_DECREF (wrapper_class);
        return NULL;
    }

    /* If we have a valid GType, stash our new wrapper as qdata on it. */
    if (gtype != G_TYPE_NONE) {
        PyObject *py_gtype;
        g_type_set_qdata (gtype, pygi_wrapper_class_key, wrapper_class);

        py_gtype = pyg_type_wrapper_new (gtype);
        res = PyObject_SetAttrString (wrapper_class, "__gtype__", py_gtype);
        Py_DECREF (py_gtype);
        if (res < 0) {
            Py_DECREF (wrapper_class);
            return NULL;
        }
    }

    return wrapper_class;
}

/**
 * pygi_wrapper_type_new_from_object_info:
 * @info: GIObjectInfo to use as an information source of pygi_wrapper_class_new_full.
 *
 * Generate a new Python class from @info. The new class will be named according
 * to the GI info and will contain a "PyGIWrapperFuncs" setup with ref/unref
 * functions for the class instances wrapped pointer.
 */
PyObject *
pygi_wrapper_class_new_from_object_info (GIObjectInfo *info)
{
    PyObject *wrapper_class;

    wrapper_class = pygi_wrapper_class_from_object_info (info);
    if (wrapper_class != NULL) {
        return wrapper_class;
    }

    return pygi_wrapper_class_new_full (
               g_base_info_get_name ((GIBaseInfo *)info),
               g_base_info_get_namespace ((GIBaseInfo *)info),
               g_registered_type_info_get_g_type ((GIRegisteredTypeInfo *)info),
               g_object_info_get_ref_function_pointer (info),
               g_object_info_get_unref_function_pointer (info));
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

static int
_pygi_wrapper_init (PyGIWrapper *self, PyObject *args, PyObject *kwargs)
{
    if (!PyArg_ParseTuple(args, ":Wrapper.__init__")) {
        return -1;
    }

    self->wrapped = NULL;

    PyErr_Format (PyExc_NotImplementedError, "%s can not be constructed",
                  Py_TYPE(self)->tp_name);
    return -1;
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

void
pygi_wrapper_register_types(PyObject *d)
{
    pygi_wrapper_class_key = g_quark_from_static_string("PyGIWrapper::class");

    PyGIWrapper_Type.tp_dealloc = (destructor)_pygi_wrapper_dealloc;
    PyGIWrapper_Type.tp_richcompare = _pygi_wrapper_richcompare;
    PyGIWrapper_Type.tp_repr = (reprfunc)_pygi_wrapper_repr;
    PyGIWrapper_Type.tp_hash = (hashfunc)_pygi_wrapper_hash;
    PyGIWrapper_Type.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    PyGIWrapper_Type.tp_init = (initproc)_pygi_wrapper_init;

    if (!PyType_Ready (&PyGIWrapper_Type))
        return;
}
