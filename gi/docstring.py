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
    CallableInfo, \
    ObjectInfo, \
    StructInfo, \
    Direction, \
    TypeTag


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

    Note that args marked as Direction.INOUT will be in both lists.

    :Returns:
        Tuple of (in_args, out_args)
    """
    in_args = []
    out_args = []
    for arg in info.get_arguments():
        direction = arg.get_direction()
        if direction in (Direction.IN, Direction.INOUT):
            in_args.append(arg)
        if direction in (Direction.OUT, Direction.INOUT):
            out_args.append(arg)
    return (in_args, out_args)


_type_tag_to_py_type = {TypeTag.BOOLEAN: bool,
                        TypeTag.INT8: int,
                        TypeTag.UINT8: int,
                        TypeTag.INT16: int,
                        TypeTag.UINT16: int,
                        TypeTag.INT32: int,
                        TypeTag.UINT32: int,
                        TypeTag.INT64: int,
                        TypeTag.UINT64: int,
                        TypeTag.FLOAT: float,
                        TypeTag.DOUBLE: float,
                        TypeTag.GLIST: list,
                        TypeTag.GSLIST: list,
                        TypeTag.ARRAY: list,
                        TypeTag.GHASH: dict,
                        TypeTag.UTF8: str,
                        TypeTag.FILENAME: str,
                        TypeTag.UNICHAR: str,
                        TypeTag.INTERFACE: None,
                        TypeTag.GTYPE: None,
                        TypeTag.ERROR: None,
                        TypeTag.VOID: None,
                        }


def _get_pytype_hint(gi_type):
    type_tag = gi_type.get_tag()
    py_type = _type_tag_to_py_type.get(type_tag, None)

    if py_type and hasattr(py_type, '__name__'):
        return py_type.__name__
    elif type_tag == TypeTag.INTERFACE:
        iface = gi_type.get_interface()

        info_name = iface.get_name()
        if not info_name:
            return gi_type.get_tag_as_string()

        return '%s.%s' % (iface.get_namespace(), info_name)

    return gi_type.get_tag_as_string()


def _generate_callable_info_sig(info):
    in_args, out_args = split_function_info_args(info)
    in_args_strs = []
    if isinstance(info, VFuncInfo):
        in_args_strs = ['self']
    elif isinstance(info, FunctionInfo):
        if info.is_method():
            in_args_strs = ['self']

    hint_blacklist = ('void',)

    # Build a lists of indices prior to adding the docs because
    # because it is possible the index retrieved comes before in
    # argument being used.
    ignore_indices = set()
    user_data_indices = set()
    for arg in in_args:
        ignore_indices.add(arg.get_destroy())
        ignore_indices.add(arg.get_type().get_array_length())
        user_data_indices.add(arg.get_closure())

    for i, arg in enumerate(in_args):
        if i in ignore_indices:
            continue
        argstr = arg.get_name()
        hint = _get_pytype_hint(arg.get_type())
        if hint not in hint_blacklist:
            argstr += ':' + hint
        if arg.may_be_null() or i in user_data_indices:
            # allow-none or user_data from a closure
            argstr += '=None'
        elif arg.is_optional():
            argstr += '=<optional>'
        in_args_strs.append(argstr)
    in_args_str = ', '.join(in_args_strs)

    out_args_strs = []
    return_hint = _get_pytype_hint(info.get_return_type())
    if not info.skip_return and return_hint and return_hint not in hint_blacklist:
        if info.may_return_null():
            argstr += ' or None'
        out_args_strs.append(return_hint)

    for i, arg in enumerate(out_args):
        if i in ignore_indices:
            continue
        argstr = arg.get_name()
        hint = _get_pytype_hint(arg.get_type())
        if hint not in hint_blacklist:
            argstr += ':' + hint
        out_args_strs.append(argstr)

    if out_args_strs:
        return '(%s) -> %s' % (in_args_str, ', '.join(out_args_strs))
    else:
        return '(%s)' % in_args_str


def _generate_callable_info_doc(info):
    return info.__name__ + _generate_callable_info_sig(info)


def _get_constructor_overloads(constructors):
    blacklist = set()
    overloads = {}

    for method_info in constructors:
        key = frozenset(arg.get_name() for arg in method_info.get_arguments())
        if key in overloads:
            blacklist.add(key)
            del overloads[key]
        elif key not in blacklist:
            overloads[key] = method_info

    return overloads.values()


def _generate_class_info_doc(info):
    doc = '\n:Constructors:\n'  # start with \n to avoid auto indent of other lines
    info_name = info.__name__

    if isinstance(info, StructInfo):
        # Don't show default constructor for disguised (0 length) structs
        if info.get_size() > 0:
            doc += '    ' + info_name + '()\n'
    else:
        doc += '    ' + info_name + '(**properties)\n'

    constructors = [method for method in info.get_methods() if method.is_constructor()]
    overloads = _get_constructor_overloads(constructors)

    # Add overloaded constructors
    for constructor in sorted(overloads, key=lambda x: len(x.__name__)):
        doc += '    %s%s\n' % (info_name, _generate_callable_info_sig(constructor))

    # Add named constructors blacklisted from overloads due to argument ambiguity
    for constructor in constructors:
        if constructor not in overloads:
            doc += '    %s.%s%s\n' % (info_name, constructor.__name__,
                                      _generate_callable_info_sig(constructor))

    return doc


def _generate_doc_dispatch(info):
    if isinstance(info, (ObjectInfo, StructInfo)):
        return _generate_class_info_doc(info)

    elif isinstance(info, CallableInfo):
        return _generate_callable_info_doc(info)

    return ''


set_doc_string_generator(_generate_doc_dispatch)
