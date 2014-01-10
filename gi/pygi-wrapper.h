/* -*- Mode: C; c-basic-offset: 4 -*-
 * pygobject - Python bindings for the GNOME platform.
 * Copyright (C) 2014  Simon Feltman
 *
 *   pygi-wrapper.h: Generic Python wrappers for pointers with
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

#ifndef __PYGI_WRAPPER_H__
#define __PYGI_WRAPPER_H__

#include <Python.h>

#include <girepository.h>

G_BEGIN_DECLS

typedef void * (*PyGIWrapper_CopyFunc)(void *data);
typedef void   (*PyGIWrapper_FreeFunc)(void *data);

/**
 * PyGIWrapperFuncs:
 * @copy: Function to copy data. This is also used as the "ref" function for
 *   reference counted types.
 * @free: Function to free data. This is also used as the "unref" function for
 *   reference counted types.
 *
 * The PyGIWrapperFuncs structure holds memory management functions for wrapping
 * a generic pointer with a Python. An instance of this structure is stored on
 * Python class objects or instances to allow generic management with Python GI
 * marshaling.
 */
typedef struct {
    PyGIWrapper_CopyFunc  copy;
    PyGIWrapper_FreeFunc  free;
} PyGIWrapperFuncs;

int
pygi_wrapper_funcs_attach (PyObject            *obj,
                           PyGIWrapper_CopyFunc copy_func,
                           PyGIWrapper_FreeFunc free_func);

int
pygi_wrapper_funcs_attach_static (PyObject         *obj,
                                  PyGIWrapperFuncs *funcs);

PyGIWrapperFuncs *
pygi_wrapper_funcs_get (PyObject *obj);

PyObject *
pygi_wrapper_new_py_type_from_info (GIObjectInfo *info);

PyGIWrapperFuncs *
pygi_wrapper_funcs_get (PyObject *obj);

PyObject *
pygi_wrapper_class_from_gtype (GType gtype);

PyObject *
pygi_wrapper_class_from_object_info (GIObjectInfo *info);

PyObject *
pygi_wrapper_class_new_full (const char *class_name,
                             const char *module_name,
                             GType gtype,
                             PyGIWrapper_CopyFunc copy_func,
                             PyGIWrapper_FreeFunc free_func);

PyObject *
pygi_wrapper_class_new_from_object_info (GIObjectInfo *info);

void
pygi_wrapper_register_types(PyObject *d);

G_END_DECLS

#endif /* __PYGI_WRAPPER_H__ */
