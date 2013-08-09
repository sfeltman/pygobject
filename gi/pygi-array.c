/* -*- Mode: C; c-basic-offset: 4 -*-
 * vim: tabstop=4 shiftwidth=4 expandtab
 *
 * Copyright (C) 2013 Simon Feltman <sfeltman@src.gnome.org>
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
#include "pygi-array.h"

#include <pyglib-python-compat.h>

static const char *
_pygi_native_format_for_signed_int_size (guint item_size)
{
    if (item_size == sizeof(signed char))
        return "b";
    else if (item_size == sizeof(short))
        return "h";
    else if (item_size == sizeof(int))
        return "i";
    else if (item_size == sizeof(long))
        return "l";
#ifdef HAVE_LONG_LONG
    else if (item_size == sizeof(PY_LONG_LONG))
        return "q";
#endif
    else
        return NULL;
}

static const char *
_pygi_native_format_for_unsigned_int_size (guint item_size)
{
    if (item_size == sizeof(unsigned char))
        return "B";
    else if (item_size == sizeof(unsigned short))
        return "H";
    else if (item_size == sizeof(unsigned int))
        return "I";
    else if (item_size == sizeof(unsigned long))
        return "L";
#ifdef HAVE_LONG_LONG
    else if (item_size == sizeof(unsigned PY_LONG_LONG))
        return "Q";
#endif
    else
        return NULL;
}

static const char *
_pygi_type_tag_to_py_format (GITypeTag type_tag, guint item_size)
{
    /* The "" prefix means native byte order with standard size. See:
     * http://docs.python.org/2/library/struct.html#byte-order-size-and-alignment
     */
    switch (type_tag) {
        case GI_TYPE_TAG_INT8:
        case GI_TYPE_TAG_INT16:
        case GI_TYPE_TAG_INT32:
        case GI_TYPE_TAG_INT64:
            return _pygi_native_format_for_signed_int_size (item_size);

        case GI_TYPE_TAG_UINT8:
        case GI_TYPE_TAG_UINT16:
        case GI_TYPE_TAG_UINT32:
        case GI_TYPE_TAG_UINT64:
            return _pygi_native_format_for_unsigned_int_size (item_size);

        case GI_TYPE_TAG_FLOAT:
            return "f";
        case GI_TYPE_TAG_DOUBLE:
            return "D";
        default:
            return NULL;
    }
}

static PyObject *
_pygi_array_new (PyTypeObject *type,
                 PyObject     *args,
                 PyObject     *kwargs)
{
    PyGIArray *self = NULL;

    if (!PyType_IsSubtype (type, &PyGIArray_Type)) {
        PyErr_SetString (PyExc_TypeError, "must be a subtype of gi.Array");
        return NULL;
    }

    self = (PyGIArray *) type->tp_alloc (type, 0);
    if (self == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    return (PyObject *) self;
}


PyObject *
_pygi_array_new_from_garray (GArray *array_,
                             GITypeTag type_tag,
                             GITransfer transfer)
{
    PyGIArray *self = NULL;
    const char *fmt = _pygi_type_tag_to_py_format (type_tag,
                                                   g_array_get_element_size (array_));
    if (fmt == NULL)
        return NULL;

    self = (PyGIArray *) PyGIArray_Type.tp_alloc (&PyGIArray_Type, 0);
    if (self == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    // ((PyVarObject*)(self))->ob_size = array_->len;
    self->type_tag = type_tag;
    self->array_ = array_;

    if (transfer == GI_TRANSFER_NOTHING)
        g_array_ref (array_);

    return (PyObject *) self;
}

static void
_pygi_array_dealloc (PyGIArray *self)
{
    g_array_unref (self->array_);
    Py_TYPE( (PyGObject *) self)->tp_free ( (PyObject *) self);
}

static int
_pygi_array_getbuffer(PyGIArray *self, Py_buffer *view, int flags)
{
    Py_INCREF (self);
    view->obj = (PyObject *)self;
    view->buf = self->array_->data;
    view->readonly = 0;
    view->ndim = 1;
    view->itemsize =  g_array_get_element_size (self->array_);

    view->format = NULL;
    if ((flags & PyBUF_FORMAT) == PyBUF_FORMAT)
        view->format = _pygi_type_tag_to_py_format (self->type_tag, view->itemsize);

    view->shape = NULL;
    //if ((flags & PyBUF_ND) == PyBUF_ND)
    //    view->shape = &(view->len);
    view->strides = NULL;
    //if ((flags & PyBUF_STRIDES) == PyBUF_STRIDES)
    //    view->strides = &(view->itemsize);
    view->len = self->array_->len * view->itemsize;
    view->suboffsets = NULL;
    view->internal = NULL;
    return 0;
}

static void
_pygi_array_releasebuffer(PyGIArray *obj, Py_buffer *view)
{
}

static PyBufferProcs _pygi_array_as_buffer = {
    (getbufferproc)_pygi_array_getbuffer,
    (releasebufferproc)_pygi_array_releasebuffer,
};

PYGLIB_DEFINE_TYPE("gi._Array", PyGIArray_Type, PyGIArray);

void
_pygi_array_register_types (PyObject *m)
{
    Py_TYPE(&PyGIArray_Type) = &PyType_Type;
    //PyGIArray_Type.tp_base = &PyObject_Type;
    PyGIArray_Type.tp_new = (newfunc) PyType_GenericNew;
    PyGIArray_Type.tp_dealloc = (destructor) _pygi_array_dealloc;
    PyGIArray_Type.tp_as_buffer = &_pygi_array_as_buffer;
    PyGIArray_Type.tp_flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE);

    if (PyType_Ready (&PyGIArray_Type))
        return;
    if (PyModule_AddObject (m, "_Array", (PyObject *) &PyGIArray_Type))
        return;
}
