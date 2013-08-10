/* -*- Mode: C; c-basic-offset: 4 -*-
 * vim: tabstop=4 shiftwidth=4 expandtab
 *
 * Copyright (C) 2009 Simon van der Linden <svdlinden@src.gnome.org>
 *
 *   pygi-struct.c: wrapper to handle non-registered structures.
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

#include "pygi-private.h"

#include <pygobject.h>
#include <girepository.h>
#include <pyglib-python-compat.h>

static void
_struct_dealloc (PyGIStruct *self)
{
    GIBaseInfo *info = _pygi_object_get_gi_info (
                           (PyObject *) self,
                           &PyGIStructInfo_Type);

    if (info != NULL && g_struct_info_is_foreign ( (GIStructInfo *) info)) {
        pygi_struct_foreign_release (info, ( (PyGPointer *) self)->pointer);
    } else if (self->free_on_dealloc) {
        g_free ( ( (PyGPointer *) self)->pointer);
    }

    g_base_info_unref (info);

    Py_TYPE( (PyGPointer *) self )->tp_free ( (PyObject *) self);
}

static PyObject *
_struct_new (PyTypeObject *type,
             PyObject     *args,
             PyObject     *kwargs)
{
    static char *kwlist[] = { NULL };

    GIBaseInfo *info;
    gsize size;
    gpointer pointer;
    PyObject *self = NULL;

    if (!PyArg_ParseTupleAndKeywords (args, kwargs, "", kwlist)) {
        return NULL;
    }

    info = _pygi_object_get_gi_info ( (PyObject *) type, &PyGIStructInfo_Type);
    if (info == NULL) {
        if (PyErr_ExceptionMatches (PyExc_AttributeError)) {
            PyErr_Format (PyExc_TypeError, "missing introspection information");
        }
        return NULL;
    }

    size = g_struct_info_get_size ( (GIStructInfo *) info);
    if (size == 0) {
        PyErr_Format (PyExc_TypeError,
            "cannot allocate disguised struct %s.%s; consider adding a constructor to the library or to the overrides",
            g_base_info_get_namespace (info),
            g_base_info_get_name (info));
        goto out;
    }
    pointer = g_try_malloc0 (size);
    if (pointer == NULL) {
        PyErr_NoMemory();
        goto out;
    }

    self = _pygi_struct_new (type, pointer, TRUE);
    if (self == NULL) {
        g_free (pointer);
    }

out:
    g_base_info_unref (info);

    return (PyObject *) self;
}

static int
_struct_init (PyObject *self,
              PyObject *args,
              PyObject *kwargs)
{
    /* Don't call PyGPointer's init, which raises an exception. */
    return 0;
}

PYGLIB_DEFINE_TYPE("gi.Struct", PyGIStruct_Type, PyGIStruct);

PyObject *
_pygi_struct_new (PyTypeObject *type,
                  gpointer      pointer,
                  gboolean      free_on_dealloc)
{
    PyGIStruct *self;
    GType g_type;

    if (!PyType_IsSubtype (type, &PyGIStruct_Type)) {
        PyErr_SetString (PyExc_TypeError, "must be a subtype of gi.Struct");
        return NULL;
    }

    self = (PyGIStruct *) type->tp_alloc (type, 0);
    if (self == NULL) {
        return NULL;
    }

    g_type = pyg_type_from_object ( (PyObject *) type);

    ( (PyGPointer *) self)->gtype = g_type;
    ( (PyGPointer *) self)->pointer = pointer;
    self->free_on_dealloc = free_on_dealloc;

    return (PyObject *) self;
}

static GIFieldInfo *
_pygi_struct_info_find_field (GIStructInfo *struct_info, const gchar *field_name)
{
    int i;
    GIFieldInfo *field_info;

    for (i = 0; i < g_struct_info_get_n_fields (struct_info); i++) {
        field_info = g_struct_info_get_field ( (GIStructInfo *) struct_info, i);
        if (strcmp (g_base_info_get_name ((GIBaseInfo *) field_info),
                    field_name) == 0)
            return field_info;
        g_base_info_unref ((GIBaseInfo *) field_info);
    }
    return NULL;
}

static int
_pygi_struct_getbuffer(PyObject *self, Py_buffer *view, int flags)
{
    GIBaseInfo *info;
    PyObject *py_bufattr;
    PyObject *py_bufferinfo;
    PyObject *py_readonly;
    gchar *format;
    Py_ssize_t itemsize;
    Py_ssize_t len;

    if (!PyObject_HasAttrString (self, "__pygi_getbufferinfo__"))
        return -1;

    info = _pygi_object_get_gi_info ( (PyObject *) self, &PyGIStructInfo_Type);
    if (info == NULL) {
        if (PyErr_ExceptionMatches (PyExc_AttributeError)) {
            PyErr_Format (PyExc_TypeError, "missing introspection information");
        }
        return -1;
    }

    py_bufferinfo = PyObject_CallMethod (self, "__pygi_getbufferinfo__", NULL);
    if (py_bufferinfo == NULL)
        return -1;

    /* py_bufferinfo needs to match the struct defined as gi.BufferInfo */
    if (PyArg_ParseTuple(py_bufferinfo, "Onn|O!s:__pygi_getbufferinfo__;error message",
                         &py_bufattr, &len, &itemsize,
                         &PyBool_Type, &py_readonly, &format)) {
        return -1;
    }

    if (PYGLIB_PyLong_Check (py_bufattr)) {
        view->buf = PyLong_AsVoidPtr (py_bufattr);
    } else if (PYGLIB_PyUnicode_Check (py_bufattr)) {
        char *field_name;
        GIArgument value;
        GITypeInfo *type_info;
        GIFieldInfo *field_info;

        field_name = PYGLIB_PyUnicode_AsString (py_bufattr);
        field_info = _pygi_struct_info_find_field ((GIStructInfo *) info, field_name);
        if (field_info == NULL) {
            PyErr_Format (PyExc_TypeError, "Field %s does not exist on %s.",
                                           field_name, g_base_info_get_name (info));
            return -1;
        }

        type_info = g_field_info_get_type (field_info);
        if (!g_type_info_is_pointer (type_info) ||
                !(g_type_info_get_tag (type_info) == GI_TYPE_TAG_VOID ||
                  g_type_info_get_array_type (type_info) == GI_ARRAY_TYPE_C)) {

            PyErr_Format (PyExc_TypeError, "Buffer field %s on %s must be a void pointer or C array.",
                                           field_name, g_base_info_get_name (info));
            g_base_info_unref ((GIBaseInfo *) type_info);
            g_base_info_unref ((GIBaseInfo *) field_info);
            return -1;
        }
        g_base_info_unref ((GIBaseInfo *) type_info);
        type_info = NULL;

        if (g_field_info_get_field (field_info, ((PyGPointer *)self)->pointer, &value)) {
            view->buf = value.v_pointer;
        } else {
            PyErr_Format (PyExc_TypeError, "Unable to read field %s on %s.",
                                           field_name, g_base_info_get_name (info));
            g_base_info_unref ((GIBaseInfo *) field_info);
            return -1;
        }

        g_base_info_unref ((GIBaseInfo *) field_info);

    } else {
        PyErr_Format (PyExc_TypeError, "The \"buf\" value must be a valid memory address "
                                       "of the buffer or a struct field name holding the address.");
        return -1;
    }

    Py_INCREF (self);
    view->obj = (PyObject *)self;
    view->readonly = py_readonly == Py_True ? 1 : 0;
    view->ndim = 1;
    view->itemsize =  itemsize; //g_array_get_element_size (self->array_);

    view->format = NULL;
    if ((flags & PyBUF_FORMAT) == PyBUF_FORMAT)
        view->format = format;

    view->shape = NULL;
    //if ((flags & PyBUF_ND) == PyBUF_ND)
    //    view->shape = &(view->len);
    view->strides = NULL;
    //if ((flags & PyBUF_STRIDES) == PyBUF_STRIDES)
    //    view->strides = &(view->itemsize);
    view->len = len * view->itemsize;
    view->suboffsets = NULL;
    view->internal = NULL;
    return 0;
}

static void
_pygi_struct_releasebuffer(PyGIArray *obj, Py_buffer *view)
{
}

static PyBufferProcs _pygi_struct_as_buffer = {
    (getbufferproc)_pygi_struct_getbuffer,
    (releasebufferproc)_pygi_struct_releasebuffer,
};

void
_pygi_struct_register_types (PyObject *m)
{
    Py_TYPE(&PyGIStruct_Type) = &PyType_Type;
    PyGIStruct_Type.tp_base = &PyGPointer_Type;
    PyGIStruct_Type.tp_new = (newfunc) _struct_new;
    PyGIStruct_Type.tp_init = (initproc) _struct_init;
    PyGIStruct_Type.tp_dealloc = (destructor) _struct_dealloc;
    PyGIStruct_Type.tp_as_buffer = &_pygi_struct_as_buffer;
    PyGIStruct_Type.tp_flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE);

    if (PyType_Ready (&PyGIStruct_Type))
        return;
    if (PyModule_AddObject (m, "Struct", (PyObject *) &PyGIStruct_Type))
        return;
}
