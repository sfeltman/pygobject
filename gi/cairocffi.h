/* -*- mode: C; c-basic-offset: 2 -*-
 *
 * Pycairo - Python bindings for cairo
 *
 * Copyright © 2013 Simon Feltman
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 */

#ifndef _CAIROCFFI_H_
#define _CAIROCFFI_H_

#include <Python.h>

#include <cairo.h>

static PyObject *_cairocffi = NULL;
static PyObject *_ffi = NULL;

/* There are no C structs defined in cairocffi, so fake an API using PyObjects */
#define PycairoContext        PyObject
#define PycairoFontFace       PyObject
#define PycairoToyFontFace    PycairoFontFace
#define PycairoFontOptions    PyObject
#define PycairoPath           PyObject
#define PycairoPattern        PyObject
#define PycairoSolidPattern   PycairoPattern
#define PycairoSurfacePattern PycairoPattern
#define PycairoGradient       PycairoPattern
#define PycairoLinearGradient PycairoPattern
#define PycairoRadialGradient PycairoPattern
#define PycairoScaledFont     PyObject
#define PycairoSurface        PyObject
#define PycairoImageSurface   PycairoSurface
#define PycairoPDFSurface     PycairoSurface
#define PycairoPSSurface      PycairoSurface
#define PycairoSVGSurface     PycairoSurface
#define PycairoWin32Surface   PycairoSurface
#define PycairoXlibSurface    PycairoSurface

static PyObject *PycairoContext_ClassPtr = NULL;
static PyObject *PycairoFontFace_ClassPtr = NULL;
static PyObject *PycairoToyFontFace_ClassPtr = NULL;
static PyObject *PycairoFontOptions_ClassPtr = NULL;
static PyObject *PycairoMatrix_ClassPtr = NULL;
static PyObject *PycairoPath_ClassPtr = NULL;
static PyObject *PycairoPattern_ClassPtr = NULL;
static PyObject *PycairoSolidPattern_ClassPtr = NULL;
static PyObject *PycairoSurfacePattern_ClassPtr = NULL;
static PyObject *PycairoGradient_ClassPtr = NULL;
static PyObject *PycairoLinearGradient_ClassPtr = NULL;
static PyObject *PycairoRadialGradient_ClassPtr = NULL;
static PyObject *PycairoScaledFont_ClassPtr = NULL;
static PyObject *PycairoSurface_ClassPtr = NULL;
static PyObject *PycairoImageSurface_ClassPtr = NULL;
static PyObject *PycairoRecordingSurface_ClassPtr = NULL;

#define PycairoContext_Type *(PycairoContext_ClassPtr->ob_type)
#define PycairoFontFace_Type *(PycairoFontFace_ClassPtr->ob_type)
#define PycairoToyFontFace_Type *(PycairoToyFontFace_ClassPtr->ob_type)
#define PycairoFontOptions_Type *(PycairoFontOptions_ClassPtr->ob_type)
#define PycairoMatrix_Type *(PycairoMatrix_ClassPtr->ob_type)
#define PycairoPath_Type *(PycairoPath_ClassPtr->ob_type)
#define PycairoPattern_Type *(PycairoPattern_ClassPtr->ob_type)
#define PycairoSolidPattern_Type *(PycairoSolidPattern_ClassPtr->ob_type)
#define PycairoSurfacePattern_Type *(PycairoSurfacePattern_ClassPtr->ob_type)
#define PycairoGradient_Type *(PycairoGradient_ClassPtr->ob_type)
#define PycairoLinearGradient_Type *(PycairoLinearGradient_ClassPtr->ob_type)
#define PycairoRadialGradient_Type *(PycairoRadialGradient_ClassPtr->ob_type)
#define PycairoScaledFont_Type *(PycairoScaledFont_ClassPtr->ob_type)
#define PycairoSurface_Type *(PycairoSurface_ClassPtr->ob_type)
#define PycairoImageSurface_Type *(PycairoImageSurface_ClassPtr->ob_type)
#define PycairoRecordingSurface_Type *(PycairoRecordingSurface_ClassPtr->ob_type)

#if CAIRO_HAS_PDF_SURFACE
static PyObject *PycairoPDFSurface_ClassPtr = NULL;
#define PycairoPDFSurface_Type *(PycairoPDFSurface_ClassPtr->ob_type)
#endif

#if CAIRO_HAS_PS_SURFACE
static PyObject *PycairoPSSurface_ClassPtr = NULL;
#define PycairoPSSurface_Type *(PycairoPSSurface_ClassPtr->ob_type)
#endif

#if CAIRO_HAS_SVG_SURFACE
static PyObject *PycairoSVGSurface_ClassPtr = NULL;
#define PycairoSVGSurface_Type *(PycairoSVGSurface_ClassPtr->ob_type)
#endif

#if CAIRO_HAS_WIN32_SURFACE
static PyObject *PycairoWin32Surface_ClassPtr = NULL;
#define PycairoWin32Surface_Type *(PycairoWin32Surface_ClassPtr->ob_type)
#endif

#if CAIRO_HAS_XLIB_SURFACE
static PyObject *PycairoXlibSurface_ClassPtr = NULL;
#define PycairoXlibSurface_Type *(PycairoXlibSurface_ClassPtr->ob_type)
#endif


static void *
_cairocffi_pyobject_as_ptr(PyObject *obj) {
    // int(ffi.cast('uintptr_t', obj._pointer))
    PyObject *pyptr = PyObject_GetAttrString (obj, "_pointer");
    PyObject *pylong = PyObject_CallMethod (_ffi, "cast",
                                           "sO", "uintptr_t", pyptr);
    Py_DECREF (pyptr);

    /* int(pylong) */
    pyptr = PyObject_CallFunction(&PyLong_Type, "O", pylong);
    Py_DECREF (pylong);
    void *ptr = PyLong_AsVoidPtr (pyptr);
    Py_DECREF (pyptr);
    return ptr;
}

static PyObject *
_cairocffi_pyobject_from_ptr(const void *ptr, PyObject *pyclass)
{
    // ffi.cast('void *', int(ptr))
    PyObject *pylong = PyLong_FromVoidPtr ((void *)ptr);
    PyObject *pyptr = PyObject_CallMethod (_ffi, "cast",
                                           "sO", "void *", pylong);
    Py_DECREF (pylong);
    if (pyptr == NULL)
        return NULL;

    PyObject *res = PyObject_CallMethod (pyclass, "_from_pointer",
                                         "OO", pyptr, Py_True);
    Py_DECREF (pyptr);
    return res;
}

/* get C object out of the Python wrapper */
#define PycairoContext_GET(obj)    ((cairo_t *)_cairocffi_pyobject_as_ptr (obj))

static PyObject *
PycairoContext_FromContext (cairo_t *ctx, PyTypeObject *type, PyObject *base) {
    if (base == NULL) {
        return _cairocffi_pyobject_from_ptr (ctx, PycairoContext_ClassPtr);
    } else {
        return _cairocffi_pyobject_from_ptr (ctx, base);
    }
}

static PyObject *
PycairoFontFace_FromFontFace (cairo_font_face_t *font_face) {
    return _cairocffi_pyobject_from_ptr (font_face, PycairoFontFace_ClassPtr);
}

static PyObject *
PycairoFontOptions_FromFontOptions (cairo_font_options_t *font_options) {
    return _cairocffi_pyobject_from_ptr (font_options, PycairoFontOptions_ClassPtr);
}

static PyObject *
PycairoMatrix_FromMatrix (const cairo_matrix_t *matrix) {
    return _cairocffi_pyobject_from_ptr (matrix, PycairoMatrix_ClassPtr);
}

static PyObject *
PycairoPath_FromPath (cairo_path_t *path) {
    return _cairocffi_pyobject_from_ptr (path, PycairoPath_ClassPtr);
}

static PyObject *
PycairoPattern_FromPattern (cairo_pattern_t *pattern, PyObject *base) {
    return _cairocffi_pyobject_from_ptr (pattern, base);
}

static PyObject *
PycairoScaledFont_FromScaledFont (cairo_scaled_font_t *scaled_font) {
    return _cairocffi_pyobject_from_ptr (scaled_font, PycairoScaledFont_ClassPtr);
}

static PyObject *
PycairoSurface_FromSurface (cairo_surface_t *surface, PyObject *base) {
    return _cairocffi_pyobject_from_ptr (surface, base);
}

static cairo_path_t *
PycairoPath_ToPath (PyObject *pypath) {
    return (cairo_path_t *)_cairocffi_pyobject_as_ptr (pypath);
}

static cairo_surface_t *
PycairoSurface_ToSurface (PyObject *pysurface) {
    return (cairo_surface_t *)_cairocffi_pyobject_as_ptr (pysurface);
}

static cairo_font_options_t *
PycairoFontOptions_ToFontOptions (PyObject *pyfontoptions) {
    return (cairo_font_options_t *)_cairocffi_pyobject_as_ptr (pyfontoptions);
}

static int
Pycairo_Check_Status (cairo_status_t status) {
    return 1;
}

static int
import_cairo(void)
{
    if (_cairocffi == NULL) {
        _cairocffi = PyImport_ImportModule("cairocffi");
        if (_cairocffi == NULL)
            return -1;

        PyObject *res = PyObject_CallMethod (_cairocffi, "install_as_pycairo", NULL);
        if (res == NULL) {
            Py_DECREF (_cairocffi);
            return -1;
        }
        Py_DECREF (res);

        _ffi = PyObject_GetAttrString (_cairocffi, "ffi");
        if (_ffi == NULL) {
            Py_DECREF (_ffi);
            return -1;
        }

    } else {
        /* already imported */
        return 0;
    }

    PycairoContext_ClassPtr        = PyObject_GetAttrString (_cairocffi, "Context");
    PycairoFontFace_ClassPtr       = PyObject_GetAttrString (_cairocffi, "FontFace");
    PycairoToyFontFace_ClassPtr    = PyObject_GetAttrString (_cairocffi, "ToyFontFace");
    PycairoFontOptions_ClassPtr    = PyObject_GetAttrString (_cairocffi, "FontOptions");
    PycairoMatrix_ClassPtr         = PyObject_GetAttrString (_cairocffi, "Matrix");
    PycairoPath_ClassPtr           = NULL; //PyObject_GetAttrString (_cairocffi, "Path");
    PycairoPattern_ClassPtr        = PyObject_GetAttrString (_cairocffi, "Pattern");
    PycairoSolidPattern_ClassPtr   = PyObject_GetAttrString (_cairocffi, "SolidPattern");
    PycairoSurfacePattern_ClassPtr = PyObject_GetAttrString (_cairocffi, "SurfacePattern");
    PycairoGradient_ClassPtr       = PyObject_GetAttrString (_cairocffi, "Gradient");
    PycairoLinearGradient_ClassPtr = PyObject_GetAttrString (_cairocffi, "LinearGradient");
    PycairoRadialGradient_ClassPtr = PyObject_GetAttrString (_cairocffi, "RadialGradient");
    PycairoScaledFont_ClassPtr     = PyObject_GetAttrString (_cairocffi, "ScaledFont");
    PycairoSurface_ClassPtr        = PyObject_GetAttrString (_cairocffi, "Surface");
    PycairoImageSurface_ClassPtr   = PyObject_GetAttrString (_cairocffi, "ImageSurface");
    PycairoRecordingSurface_ClassPtr = PyObject_GetAttrString (_cairocffi, "RecordingSurface");

#if CAIRO_HAS_PDF_SURFACE
    PycairoPDFSurface_ClassPtr     = PyObject_GetAttrString (_cairocffi, "PDFSurface");
#endif

#if CAIRO_HAS_PS_SURFACE
    PycairoPSSurface_ClassPtr      = PyObject_GetAttrString (_cairocffi, "PSSurface");
#endif

#if CAIRO_HAS_SVG_SURFACE
    PycairoSVGSurface_ClassPtr     = PyObject_GetAttrString (_cairocffi, "SVGSurface");
#endif

#if CAIRO_HAS_WIN32_SURFACE
    PycairoWin32Surface_ClassPtr   = PyObject_GetAttrString (_cairocffi, "Win32Surface");
#endif

#if CAIRO_HAS_XLIB_SURFACE
    PycairoXlibSurface_ClassPtr    = NULL; //PyObject_GetAttrString (_cairocffi, "XlibSurface");
#endif

    if (PyErr_Occurred())
        return -1;
    return 0;
}

#if PY_VERSION_HEX < 0x03000000
#    define Pycairo_IMPORT import_cairo
#endif


#endif /* ifndef _CAIROCFFI_H_ */

