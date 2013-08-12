# -*- Mode: Python; py-indent-offset: 4 -*-
# vim: tabstop=4 shiftwidth=4 expandtab
#
# Copyright (C) 2013 Simon Feltman <sfeltman@gnome.org>
#
#   docstring.py: documentation string generator for gi.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
# USA

from ._gi import \
    VFuncInfo, \
    FunctionInfo, \
    DIRECTION_IN, \
    DIRECTION_OUT, \
    DIRECTION_INOUT


#: Module storage for currently registered doc string generator function.
_generate_doc_string_func = None


def set_doc_string_generator(func):
    """Set doc string generator function

    :Parameters:
        func : callable
            Function which takes a GIInfoStruct and returns
            documentation for it.
    """
    global _generate_doc_string_func
    _generate_doc_string_func = func


def get_doc_string_generator():
    return _generate_doc_string_func


def generate_doc_string(info):
    """Generator a doc string given a GIInfoStruct

    This passes the info struct to the currently registered doc string
    generator and returns the result.
    """
    return _generate_doc_string_func(info)


def split_function_info_args(info):
    """Split a functions args into a tuple of two lists.

    Note that args marked as DIRECTION_INOUT will be in both lists.

    :Returns:
        Tuple of (in_args, out_args)
    """
    in_args = []
    out_args = []
    for arg in info.get_arguments():
        direction = arg.get_direction()
        if direction in (DIRECTION_IN, DIRECTION_INOUT):
            in_args.append(arg)
        if direction in (DIRECTION_OUT, DIRECTION_INOUT):
            out_args.append(arg)
    return (in_args, out_args)


def _generate_callable_info_function_signature(info):
    """Default doc string generator"""
    in_args, out_args = split_function_info_args(info)
    in_args_strs = []
    if isinstance(info, VFuncInfo):
        in_args_strs = ['self']
    elif isinstance(info, FunctionInfo):
        if info.is_method():
            in_args_strs = ['self']
        elif info.is_constructor():
            in_args_strs = ['cls']

    # Build a lists of indices prior to adding the docs because
    # because it is possible the index retrieved comes before in
    # argument being used.
    ignore_indices = set([arg.get_destroy() for arg in in_args])
    user_data_indices = set([arg.get_closure() for arg in in_args])

    for i, arg in enumerate(in_args):
        if i in ignore_indices:
            continue
        argstr = arg.get_name()
        hint = arg.get_pytype_hint()
        if hint not in ('void',):
            argstr += ':' + hint
        if arg.may_be_null() or i in user_data_indices:
            # allow-none or user_data from a closure
            argstr += '=None'
        elif arg.is_optional():
            argstr += '=<optional>'
        in_args_strs.append(argstr)
    in_args_str = ', '.join(in_args_strs)

    if out_args:
        out_args_str = ', '.join(arg.get_name() + ':' + arg.get_pytype_hint()
                                 for arg in out_args)
        return '%s(%s) -> %s' % (info.get_name(), in_args_str, out_args_str)
    else:
        return '%s(%s)' % (info.get_name(), in_args_str)


set_doc_string_generator(_generate_callable_info_function_signature)
