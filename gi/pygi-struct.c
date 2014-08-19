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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "pygi-struct.h"
#include "pygi-foreign.h"
#include "pygi-info.h"
#include "pygi-type.h"
#include "pygtype.h"
#include "pygpointer.h"

#include <girepository.h>
#include <pyglib-python-compat.h>


static GIBaseInfo *
_struct_get_info (PyObject *self)
{
    PyObject *py_info;
    GIBaseInfo *info = NULL;

    py_info = PyObject_GetAttrString (self, "__info__");
    if (py_info == NULL) {
        return NULL;
    }
    if (!PyObject_TypeCheck (py_info, &PyGIStructInfo_Type) &&
            !PyObject_TypeCheck (py_info, &PyGIUnionInfo_Type)) {
        PyErr_Format (PyExc_TypeError, "attribute '__info__' must be %s or %s, not %s",
                      PyGIStructInfo_Type.tp_name,
                      PyGIUnionInfo_Type.tp_name,
                      Py_TYPE(py_info)->tp_name);
        goto out;
    }

    info = ( (PyGIBaseInfo *) py_info)->info;
    g_base_info_ref (info);

out:
    Py_DECREF (py_info);

    return info;
}

static void
_struct_dealloc (PyGIStruct *self)
{
    GIBaseInfo *info = _struct_get_info ( (PyObject *) self );

    if (info != NULL && g_struct_info_is_foreign ( (GIStructInfo *) info)) {
        pygi_struct_foreign_release (info, pyg_pointer_get_ptr (self));
    } else if (self->free_on_dealloc) {
        g_free (pyg_pointer_get_ptr (self));
    }

    if (info != NULL) {
        g_base_info_unref (info);
    }

    Py_TYPE (self)->tp_free ((PyObject *)self);
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

    info = _struct_get_info ( (PyObject *) type );
    if (info == NULL) {
        if (PyErr_ExceptionMatches (PyExc_AttributeError)) {
            PyErr_Format (PyExc_TypeError, "missing introspection information");
        }
        return NULL;
    }

    size = g_struct_info_get_size ( (GIStructInfo *) info);
    if (size == 0) {
        PyErr_Format (PyExc_TypeError,
            "struct cannot be created directly; try using a constructor, see: help(%s.%s)",
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
_pygi_struct_new_from_g_type (GType g_type,
                              gpointer      pointer,
                              gboolean      free_on_dealloc)
{
    PyGIStruct *self;
    PyTypeObject *type;

    type = (PyTypeObject *)pygi_type_import_by_g_type (g_type);

    if (!type)
        type = (PyTypeObject *)&PyGIStruct_Type; /* fallback */

    if (!PyType_IsSubtype (type, &PyGIStruct_Type)) {
        PyErr_SetString (PyExc_TypeError, "must be a subtype of gi.Struct");
        return NULL;
    }

    self = (PyGIStruct *) type->tp_alloc (type, 0);
    if (self == NULL) {
        return NULL;
    }

    pyg_pointer_set_ptr (self, pointer);
    ( (PyGPointer *) self)->gtype = g_type;
    self->free_on_dealloc = free_on_dealloc;

    return (PyObject *) self;
}


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

    pyg_pointer_set_ptr (self, pointer);
    ( (PyGPointer *) self)->gtype = g_type;
    self->free_on_dealloc = free_on_dealloc;

    return (PyObject *) self;
}

static PyObject *
_struct_repr(PyGIStruct *self)
{
    PyObject* repr;
    GIBaseInfo *info;
    PyGPointer *pointer = (PyGPointer *)self;

    info = _struct_get_info ((PyObject *)self);
    if (info == NULL)
        return NULL;

    repr = PYGLIB_PyUnicode_FromFormat ("<%s.%s object at %p (%s at %p)>",
                                        g_base_info_get_namespace (info),
                                        g_base_info_get_name (info),
                                        self, g_type_name (pointer->gtype),
                                        pointer->pointer);

    g_base_info_unref (info);

    return repr;
}

static GIFieldInfo *
struct_info_find_field (GIStructInfo *struct_info, const gchar *field_name)
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
struct_getbuffer (PyObject *self, Py_buffer *view, int flags)
{
    GIBaseInfo *info;
    PyObject *py_bufattr;
    PyObject *py_bufferinfo;
    PyObject *py_readonly;
    gchar *format;
    Py_ssize_t itemsize;
    Py_ssize_t len;
    gboolean readonly;
    void *buffer_ptr = NULL;

    if (!PyObject_HasAttrString (self, "_pygi_getbufferinfo_"))
        return -1;

    info = _pygi_object_get_gi_info ( (PyObject *) self, &PyGIStructInfo_Type);
    if (info == NULL) {
        if (PyErr_ExceptionMatches (PyExc_AttributeError)) {
            PyErr_Format (PyExc_TypeError, "missing introspection information");
        }
        return -1;
    }

    py_bufferinfo = PyObject_CallMethod (self, "_pygi_getbufferinfo_", NULL);
    if (py_bufferinfo == NULL)
        return -1;

    /* py_bufferinfo needs to match the struct defined as gi.BufferInfo */
    if (!PyArg_ParseTuple(py_bufferinfo, "Onn|O!s:_pygi_getbufferinfo_",
                         &py_bufattr, &len, &itemsize,
                         &PyBool_Type, &py_readonly, &format)) {
        return -1;
    }
    readonly = py_readonly == Py_True;

    if (PYGLIB_PyLong_Check (py_bufattr)) {
        view->buf = PyLong_AsVoidPtr (py_bufattr);
    } else if (PYGLIB_PyUnicode_Check (py_bufattr)) {
        char *field_name;
        GIArgument value;
        GITypeInfo *type_info;
        GIFieldInfo *field_info;

        field_name = PYGLIB_PyUnicode_AsString (py_bufattr);
        field_info = struct_info_find_field ((GIStructInfo *) info, field_name);
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
            buffer_ptr = value.v_pointer;
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
    view->readonly = readonly;
    view->ndim = 1;
    view->itemsize =  itemsize;
    view->len = len * view->itemsize;
    view->buf = buffer_ptr;
    view->format = NULL;
    if ((flags & PyBUF_FORMAT) == PyBUF_FORMAT) {
        view->format = format;
    }
    view->shape = NULL;
    if ((flags & PyBUF_ND) == PyBUF_ND) {
        view->shape = malloc (sizeof (Py_ssize_t));
        *view->shape = len;
    }
    view->strides = NULL;
    if ((flags & PyBUF_STRIDES) == PyBUF_STRIDES) {
        view->strides = &(view->itemsize);
    }
    view->suboffsets = NULL;
    view->internal = NULL;

    return 0;
}

static void
struct_releasebuffer (PyObject *obj, Py_buffer *view)
{
    g_free (view->shape);
}

#if PY_MAJOR_VERSION >= 3
PyBufferProcs pygi_struct_buffer_procs = {
    (getbufferproc)struct_getbuffer,
    (releasebufferproc)struct_releasebuffer,
};
#else
PyBufferProcs pygi_struct_buffer_procs = {
    (getreadbufferproc)0,
    (getwritebufferproc)0,
    (getsegcountproc)0,
    (getcharbufferproc)0
};
#endif

void
_pygi_struct_register_types (PyObject *m)
{
    Py_TYPE(&PyGIStruct_Type) = &PyType_Type;
    PyGIStruct_Type.tp_base = &PyGPointer_Type;
    PyGIStruct_Type.tp_new = (newfunc) _struct_new;
    PyGIStruct_Type.tp_init = (initproc) _struct_init;
    PyGIStruct_Type.tp_dealloc = (destructor) _struct_dealloc;
    PyGIStruct_Type.tp_flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE);
    PyGIStruct_Type.tp_repr = (reprfunc)_struct_repr;

#if PY_MAJOR_VERSION >= 3
    PyGIStruct_Type.tp_as_buffer = &pygi_struct_buffer_procs;
#endif

    if (PyType_Ready (&PyGIStruct_Type))
        return;
    if (PyModule_AddObject (m, "Struct", (PyObject *) &PyGIStruct_Type))
        return;
}
