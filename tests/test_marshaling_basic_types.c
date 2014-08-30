/* test_marshaling_basic_types.c
 * Copyright (C) 2013  Simon Feltman
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib.h>
#include <Python.h>

#include "pygi-marshal-from-py.h"
#include "pyglib-python-compat.h"

#define G_TEST_EXPECT_FAILURE(assertion) \
    ;

static gboolean
int_argument_from_pyobj (GIArgument *arg, GITypeTag type_tag, PyObject *obj)
{
    gboolean result;
    g_assert (obj);
    result = _pygi_marshal_from_py_basic_type (obj,
                                               arg,
                                               type_tag,
                                               GI_TRANSFER_NOTHING);
    Py_DECREF (obj);
    return result;
}

#define TEST_INT_OBJ(arg_member, type_tag, obj, value) { \
    GIArgument arg = {0,}; \
    gboolean result = int_argument_from_pyobj (&arg, type_tag, obj); \
    g_assert (result); \
    g_assert (PyErr_Occurred () == NULL); \
    g_assert_cmpint (arg.arg_member, ==, value); \
}

#define TEST_INT_OBJ_ERROR(arg_member, type_tag, obj, exc) { \
    GIArgument arg = {0,}; \
    gboolean result = int_argument_from_pyobj (&arg, type_tag, obj); \
    g_assert (!result); \
    g_assert (PyErr_Occurred () != NULL); \
    g_assert (PyErr_ExceptionMatches(exc)); \
    PyErr_Clear (); \
}

#define TEST_INT(arg_member, type_tag, value) \
    TEST_INT_OBJ(arg_member, type_tag, PyLong_FromLongLong (value), value)

#define TEST_INT_ERROR(arg_member, type_tag, value, exc) \
    TEST_INT_OBJ_ERROR(arg_member, type_tag, PyLong_FromLongLong (value), exc)

#define TEST_INT_STR(arg_member, type_tag, strvalue, value) \
    TEST_INT_OBJ(arg_member, type_tag, PyLong_FromString (strvalue, NULL, 0), value)

#define TEST_INT_STR_ERROR(arg_member, type_tag, strvalue, exc) \
    TEST_INT_OBJ_ERROR(arg_member, type_tag, PyLong_FromString (strvalue, NULL, 0), exc)


static void
test_int8_from_py (void)
{
    /* int8 */
    TEST_INT (v_int8, GI_TYPE_TAG_INT8, G_MININT8);
    TEST_INT (v_int8, GI_TYPE_TAG_INT8, G_MAXINT8);

    G_TEST_EXPECT_FAILURE(
        TEST_INT_ERROR (v_int8, GI_TYPE_TAG_INT8, G_MININT8-1, PyExc_OverflowError);
        TEST_INT_ERROR (v_int8, GI_TYPE_TAG_INT8, G_MAXINT8+1, PyExc_OverflowError);
        /* int8 and uint8 allow single byte PyBytes */
        TEST_INT_OBJ (v_int8,  GI_TYPE_TAG_INT8, PYGLIB_PyBytes_FromString ("0"), '0');
        /* more than a single byte raises type error */
        TEST_INT_OBJ_ERROR (v_int8,  GI_TYPE_TAG_INT8, PYGLIB_PyBytes_FromString ("10"), PyExc_TypeError);
    )

    /* uint8 */
    TEST_INT (v_uint8, GI_TYPE_TAG_UINT8, 0);
    TEST_INT (v_uint8, GI_TYPE_TAG_UINT8, G_MAXUINT8);

    G_TEST_EXPECT_FAILURE(
        TEST_INT_ERROR (v_uint8, GI_TYPE_TAG_UINT8, -1, PyExc_OverflowError);
        TEST_INT_ERROR (v_uint8, GI_TYPE_TAG_UINT8, G_MAXUINT8+1, PyExc_OverflowError);
        TEST_INT_OBJ (v_uint8, GI_TYPE_TAG_INT8, PYGLIB_PyBytes_FromString ("0"), '0');
        TEST_INT_OBJ_ERROR (v_uint8, GI_TYPE_TAG_INT8, PYGLIB_PyBytes_FromString ("10"), PyExc_TypeError);
    )
}

static void
test_int16_from_py (void)
{
    /* int16 */
    TEST_INT (v_int16, GI_TYPE_TAG_INT16, G_MININT16);
    TEST_INT (v_int16, GI_TYPE_TAG_INT16, G_MAXINT16);

    G_TEST_EXPECT_FAILURE(
        TEST_INT_ERROR (v_int16, GI_TYPE_TAG_INT16, G_MININT16-1, PyExc_OverflowError);
        TEST_INT_ERROR (v_int16, GI_TYPE_TAG_INT16, G_MAXINT16+1, PyExc_OverflowError);
    )

    /* uint16 */
    TEST_INT (v_uint16, GI_TYPE_TAG_UINT16, 0);
    TEST_INT (v_uint16, GI_TYPE_TAG_UINT16, G_MAXUINT16);

    G_TEST_EXPECT_FAILURE(
        TEST_INT_ERROR (v_uint16, GI_TYPE_TAG_UINT16, -1, PyExc_OverflowError);
        TEST_INT_ERROR (v_uint16, GI_TYPE_TAG_UINT16, G_MAXUINT16+1, PyExc_OverflowError);
    )
}

static void
test_int32_from_py (void)
{

    /* int32 */
    TEST_INT (v_int32, GI_TYPE_TAG_INT32, G_MININT32);
    TEST_INT (v_int32, GI_TYPE_TAG_INT32, G_MAXINT32);

    G_TEST_EXPECT_FAILURE(
        TEST_INT_ERROR (v_int32, GI_TYPE_TAG_INT32, G_MININT32-1, PyExc_OverflowError);
        TEST_INT_ERROR (v_int32, GI_TYPE_TAG_INT32, G_MAXINT32+1, PyExc_OverflowError);
    )

    /* uint32 */
    TEST_INT (v_uint32, GI_TYPE_TAG_UINT32, 0);
    TEST_INT (v_uint32, GI_TYPE_TAG_UINT32, G_MAXUINT32);

    G_TEST_EXPECT_FAILURE(
        TEST_INT_ERROR (v_uint32, GI_TYPE_TAG_UINT32, -1, PyExc_OverflowError);
        TEST_INT_ERROR (v_uint32, GI_TYPE_TAG_UINT32, G_MAXUINT32+1, PyExc_OverflowError);
    )
}

static void
test_int64_from_py (void)
{
    /* int64 */
    TEST_INT (v_int64, GI_TYPE_TAG_INT64, G_MININT64);
    TEST_INT (v_int64, GI_TYPE_TAG_INT64, G_MAXINT64);

    G_TEST_EXPECT_FAILURE(
        TEST_INT_ERROR (v_int64, GI_TYPE_TAG_INT64, G_MININT64-1, PyExc_OverflowError);
        TEST_INT_ERROR (v_int64, GI_TYPE_TAG_INT64, G_MAXINT64+1, PyExc_OverflowError);
    )

    /* uint64 */
    TEST_INT (v_uint64, GI_TYPE_TAG_UINT64, 0);
    TEST_INT_STR (v_uint64, GI_TYPE_TAG_UINT64, "0xffffffffffffffff", G_MAXUINT64);

    G_TEST_EXPECT_FAILURE(
        TEST_INT_STR_ERROR (v_uint64, GI_TYPE_TAG_UINT64, "-1", PyExc_OverflowError);
        TEST_INT_STR_ERROR (v_uint64,
                            GI_TYPE_TAG_UINT64,
                            "0x10000000000000000", /* G_MAXUINT+1 */
                            PyExc_OverflowError);
    )
}

int
main (int argc, char *argv[])
{
    int result = 0;

    g_test_init (&argc, &argv, NULL);
    g_test_add_func ("/test_marshaling_basic_types/test_int8_from_py", test_int8_from_py);
    g_test_add_func ("/test_marshaling_basic_types/test_int16_from_py", test_int16_from_py);
    g_test_add_func ("/test_marshaling_basic_types/test_int32_from_py", test_int32_from_py);
    g_test_add_func ("/test_marshaling_basic_types/test_int64_from_py", test_int64_from_py);

#if PY_MAJOR_VERSION >= 3
    Py_SetProgramName ((wchar_t *)argv[0]);
#else
    Py_SetProgramName (argv[0]);
#endif

    Py_Initialize ();

    result = g_test_run ();

    Py_Finalize ();
    return result;
}

