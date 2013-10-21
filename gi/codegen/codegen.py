#!/usr/bin/env python3

import os
import sys
import argparse
import keyword
import textwrap
import re
import string
import types
from io import StringIO
from contextlib import contextmanager


from gi import _gi as GIRepository
from gi.repository import GObject

from gi._gi import TypeTag, InfoType

repo = GIRepository.Repository.get_default()
indentation = ' ' * 4

# Hold the currently generating module in this global that templates import
current_context = None


# form is key: (format_string, converter_func_name or None)
_typetag_to_pyparse = {TypeTag.VOID: ('O&', 'pygi_codegen_interface_from_py'),
                       TypeTag.BOOLEAN: ('O&', 'pygi_codegen_boolean_from_py'),
                       TypeTag.DOUBLE: ('d', None),
                       TypeTag.FLOAT: ('f', None),
                       TypeTag.INT8: ('b', None),
                       TypeTag.UINT8: ('B', None),
                       TypeTag.INT16: ('h', None),
                       TypeTag.UINT16: ('H', None),
                       TypeTag.UNICHAR: ('H', None),
                       TypeTag.INT32: ('i', None),
                       TypeTag.UINT32: ('I', None),
                       TypeTag.INT64: ('L', None),
                       TypeTag.UINT64: ('K', None),
                       TypeTag.UTF8: ('s', None),
                       TypeTag.FILENAME: ('s', None),
                       TypeTag.INTERFACE: ('O&', 'pygi_codegen_interface_converter'),
                       TypeTag.GTYPE: ('O&', None),
                       TypeTag.ARRAY: ('O&', None),
                       TypeTag.GHASH: ('O&', None),
                       TypeTag.ERROR: ('O&', None),
                       TypeTag.GLIST: ('O&', None),
                       TypeTag.GSLIST: ('O&', None),
                       }


def typetag_to_pyparse(type_tag):
    return _typetag_to_pyparse[type_tag]


def typeinfo_to_pyparse(type_info):
    tag = type_info.get_tag()
    if tag == TypeTag.INTERFACE:
        iface = type_info.get_interface()
        iface_info_type = iface.get_type()
        if iface_info_type in (InfoType.FLAGS, InfoType.ENUM):
            return _typetag_to_pyparse[iface.get_storage_type()]

        name = iface.get_name()
        if name in current_context.registered_structs:
            builder = current_context.registered_structs[name]
            return ('O&', builder.from_py_converter)
    return _typetag_to_pyparse[tag]


_typetag_to_buildvalue = {TypeTag.VOID: ('O&', 'pygi_codegen_interface_to_py'),
                          TypeTag.BOOLEAN: ('O&', 'pygi_codegen_boolean_to_py'),
                          TypeTag.DOUBLE: ('d', None),
                          TypeTag.FLOAT: ('f', None),
                          TypeTag.INT8: ('b', None),
                          TypeTag.UINT8: ('B', None),
                          TypeTag.INT16: ('h', None),
                          TypeTag.UINT16: ('H', None),
                          TypeTag.UNICHAR: ('H', None),
                          TypeTag.INT32: ('i', None),
                          TypeTag.UINT32: ('I', None),
                          TypeTag.INT64: ('L', None),
                          TypeTag.UINT64: ('K', None),
                          TypeTag.UTF8: ('z', None),
                          TypeTag.FILENAME: ('z', None),
                          TypeTag.INTERFACE: ('O&', None),
                          TypeTag.GTYPE: ('O&', None),
                          TypeTag.ARRAY: ('O&', None),
                          TypeTag.GHASH: ('O&', None),
                          TypeTag.ERROR: ('O&', None),
                          TypeTag.GLIST: ('O&', None),
                          TypeTag.GSLIST: ('O&', None),
                          }


def typetag_to_buildvalue(type_tag):
    return _typetag_to_buildvalue[type_tag]


def typeinfo_to_buildvalue(type_info):
    tag = type_info.get_tag()
    if tag == TypeTag.INTERFACE:
        iface = type_info.get_interface()
        iface_info_type = iface.get_type()
        if iface_info_type in (InfoType.FLAGS, InfoType.ENUM):
            return _typetag_to_buildvalue[iface.get_storage_type()]

        name = iface.get_name()
        if name in current_context.registered_structs:
            builder = current_context.registered_structs[name]
            return ('O&', builder.to_py_converter)
    return _typetag_to_buildvalue[tag]


_typetag_to_ctype = {TypeTag.BOOLEAN: 'int',
                     TypeTag.DOUBLE: 'double',
                     TypeTag.FLOAT: 'float',
                     TypeTag.INT8: 'signed char',
                     TypeTag.UINT8: 'unsigned char',
                     TypeTag.INT16: 'signed short',
                     TypeTag.UINT16: 'unsigned short',
                     TypeTag.UNICHAR: 'unsigned short',
                     TypeTag.INT32: 'signed int',
                     TypeTag.UINT32: 'unsigned int',
                     TypeTag.INT64: 'PY_LONG_LONG',
                     TypeTag.UINT64: 'unsigned PY_LONG_LONG',
                     TypeTag.UTF8: 'const char*',
                     TypeTag.FILENAME: 'const char*',
                     TypeTag.INTERFACE: 'void*',
                    }

"""                  TypeTag.VOID: 'void*',
                     TypeTag.GTYPE: 'GType',
                     TypeTag.ARRAY: 'void*',
                     TypeTag.GHASH: 'void*',
                     TypeTag.ERROR: 'void*',
                     TypeTag.GLIST: 'void*',
                     TypeTag.GSLIST: 'void*',
                    }
"""


def typetag_to_ctype(type_tag):
    return _typetag_to_ctype[type_tag]


def typeinfo_to_ctype(type_info):
    tag = type_info.get_tag()
    if tag == TypeTag.INTERFACE:
        iface = type_info.get_interface()
        iface_info_type = iface.get_type()
        if iface_info_type in (InfoType.FLAGS, InfoType.ENUM):
            return _typetag_to_ctype[iface.get_storage_type()]

        name = iface.get_name()
        if name in current_context.registered_structs:
            builder = current_context.registered_structs[name]
            return builder.ctype
    return _typetag_to_ctype[tag]


_typetag_to_gtype = {TypeTag.VOID: 'gpointer',
                     TypeTag.BOOLEAN: 'gboolean',
                     TypeTag.DOUBLE: 'gdouble',
                     TypeTag.FLOAT: 'gfloat',
                     TypeTag.INT8: 'gint8',
                     TypeTag.UINT8: 'guint8',
                     TypeTag.INT16: 'gint16',
                     TypeTag.UINT16: 'guint16',
                     TypeTag.INT32: 'gint32',
                     TypeTag.UINT32: 'guint32',
                     TypeTag.INT64: 'gint64',
                     TypeTag.UINT64: 'guint64',
                     TypeTag.UTF8: 'gchar*',
                     TypeTag.FILENAME: 'gchar*',
                     TypeTag.INTERFACE: 'gpointer',
                     TypeTag.GTYPE: 'GType',
                     }


def camel_down_under(value):
    return re.sub('(((?<=[a-z])[A-Z])|([A-Z](?![A-Z]|$)))', '_\\1', value).lower().strip('_')


@contextmanager
def capture_stdout():
    oldout, sys.stdout = sys.stdout, StringIO()
    yield sys.stdout
    sys.stdout = oldout


_magic_print_substitute_re = re.compile(r'^(?P<indent>[ \t]*)\$python{\s*\n(?P<block>.*?)\s*\}end[ \t]*\n|'
                                        r'(?:\${(?P<expr>.*?)})',
                                        re.MULTILINE | re.DOTALL)


def magic_print(*args, sep=' ', end='\n', file=None, flush=False):
    """
    Print function which runs strings through a template replacer.
    """
    if file is None:
        file = sys.stdout

    # pull copies of the locals and globals contexts out of the caller
    frame = sys._getframe(1)
    context_locals = frame.f_locals.copy()
    context_globals = frame.f_globals.copy()

    def run_embedded_code(match):
        print(match.groupdict(), file=sys.stderr)
        indent, exec_block, eval_expr = match.groups()
        if eval_expr:
            return str(eval(eval_expr, context_globals, context_locals))
        else:
            exec_block = textwrap.dedent(exec_block)
            code = compile(exec_block, frame.f_code.co_filename, mode='exec')
            with capture_stdout() as out:
                exec(code, context_globals, context_locals)
                text = textwrap.indent(out.getvalue(), indent)
            return text

    combined_args = sep.join(_magic_print_substitute_re.sub(run_embedded_code, textwrap.dedent(arg))
                             for arg in args)
    file.write(combined_args)
    file.write(end)
    if flush:
        file.flush()

function_wrapper_template = """
static PyObject *
${self.function_wrapper_name} (${self.function_self_object_struct} *self, PyObject *args, PyObject *kwargs)
{
    $python{
        self.print_wrapper_body()
    }end
}
"""

function_wrapper_template_no_args = """
static PyObject *
${self.function_wrapper_name} (${self.function_self_object_struct} *self)
{
    $python{
        self.print_wrapper_body()
    }end
}
"""


method_table_template = """
static PyMethodDef ${self.tp_methods}[] = {
    $python{
        for builder in self.method_builders:
            builder.print_wrapper_entry()
    }end
    #ifdef ${self.method_table_custom_entries_macro_name}
        ${self.method_table_custom_entries_macro_name}
    #endif
    { NULL, NULL, 0 }
};
"""


class Builder:
    def __getitem__(self, name):
        """Overridden so we can use an instance of this object as a mapping
        for Python formatting."""
        return getattr(self, name)


class FunctionBuilder(Builder):
    info_types = (InfoType.FUNCTION,
                  InfoType.SIGNAL,
                  InfoType.VFUNC)

    function_install_tmpl = ""

    function_not_implemented_body = """
        PyErr_SetString (PyExc_NotImplementedError, "Don't know how to marshal %s");
        return NULL;"""

    def __init__(self, info):
        self.info = info
        self.symbol_name = info.get_symbol()
        self.function_wrapper_name = 'py' + self.symbol_name
        self.function_self_object_struct = 'PyObject'

        self.name = info.get_name()
        if keyword.iskeyword(self.name):
            self.name = self.name + '_'

        self.arguments = self.info.get_arguments()
        if self.arguments:
            self.py_flags = 'METH_VARARGS | METH_KEYWORDS'
        else:
            self.py_flags = 'METH_NOARGS'

    def print_wrapper_body(self):
        return_type = self.info.get_return_type()
        return_type_tag = return_type.get_tag()

        # keep track of variable declaration lines for printing later.
        decls = []

        if return_type_tag != TypeTag.VOID:
            if return_type_tag not in _typetag_to_ctype:
                magic_print(self.function_not_implemented_body % return_type_tag)
                return
            else:
                decls.append('%s res;' % typeinfo_to_ctype(return_type))
                decls.append('PyObject *py_res = NULL;')

        if self.info.get_flags() & GIRepository.FunctionInfoFlags.THROWS:
            throws = True
            decls.append('GError *error = NULL;')
        else:
            throws = False

        arg_names = [arg.get_name() for arg in self.arguments]
        if arg_names:
            # insert the static kwlist so it is at the top of variable declarations
            decls.insert(0, "static char *kwlist[] = { %s, NULL };" %
                         ', '.join('"%s"' % n for n in arg_names))

            parser_format = ''
            parser_args = []
            for arg in self.arguments:
                name = arg.get_name()
                tp = arg.get_type()
                tag = tp.get_tag()
                if tag not in _typetag_to_ctype:
                    magic_print(self.function_not_implemented_body % tag)
                    return

                # print variable declaration
                decls.append('%s %s;' % (typeinfo_to_ctype(tp), name))

                fmt, converter = typeinfo_to_pyparse(tp)
                parser_format += fmt
                if converter is not None:
                    parser_args.append(converter)
                parser_args.append('&' + name)

        for decl in decls:
            magic_print(decl)

        if arg_names:
            magic_print("""
                if (!PyArg_ParseTupleAndKeywords (args, kwargs,
                                                  "${parser_format}:${self.name}", kwlist,
                                                  ${", ".join(parser_args)}))
                    return NULL;
                """)

        if self.info.is_method():
            callee_arg_list = ['self->obj']
        else:
            callee_arg_list = []
        callee_arg_list.extend(arg_names)

        if throws:
            callee_arg_list.append('&error')

        # Call the function
        if return_type_tag == TypeTag.VOID:
            magic_print('%s (%s);' % (self.symbol_name, ', '.join(callee_arg_list)))
        else:
            magic_print('res = %s (%s);' % (self.symbol_name, ', '.join(callee_arg_list)))

        # TODO: Cleanup argument marshaling

        if throws:
            magic_print("""
                if (error != NULL) {
                    PyErr_SetString (PyExc_ValueError, error->message);
                    g_error_free (error);
                    return NULL;
                }""")

        # Setup return
        if return_type_tag == TypeTag.VOID:
            magic_print('Py_RETURN_NONE;')
        else:

            # TODO: handle out and inout args
            return_format = ''
            return_args = []
            fmt, converter = typeinfo_to_buildvalue(return_type)
            return_format += fmt
            if converter is not None:
                return_args.append(converter)
                return_args.append('&res')
            else:
                return_args.append('res')

            magic_print('py_res = Py_BuildValue ("%s", %s);' %
                        (return_format, ', '.join(return_args)))
            # TODO: Cleanup return value marshal
            magic_print('return py_res;')

    def print_wrapper_def(self):
        if self.arguments:
            magic_print(function_wrapper_template)
        else:
            magic_print(function_wrapper_template_no_args)

    def print_wrapper_entry(self):
        magic_print('{ "${self.name}", (PyCFunction) ${self.function_wrapper_name}, ${self.py_flags} },')


class MethodBuilder(FunctionBuilder):
    def __init__(self, info, object_struct_name='PyObject'):
        super().__init__(info)
        self.function_self_object_struct = object_struct_name

        if info.is_constructor() or not info.is_method():
            self.py_flags += ' | METH_STATIC'


py_type_object = """
PyTypeObject ${self.py_type_object_name} = {
    PyVarObject_HEAD_INIT(NULL, 0)
    /* .tp_name = */ ${self.tp_name},
    /* .tp_size = */ ${self.tp_size},
};
"""


class ClassBuilder(Builder):
    """
    info_types = (GIRepository.RegisteredTypeInfo,
                  GIRepository.EnumInfo,
                  GIRepository.InterfaceInfo,
                  GIRepository.ObjectInfo,
                  GIRepository.StructInfo,
                  GIRepository.UnionInfo)
    """
    info_types = (InfoType.OBJECT,
                  InfoType.INTERFACE,
                  InfoType.BOXED,
                  InfoType.STRUCT,
                  InfoType.UNION,
                  InfoType.TYPE)


    py_type_register = """
        Py_TYPE(&${self.py_type_object_name}) = &PyType_Type;
        ${self.py_type_object_name}.tp_repr = (reprfunc)${self.tp_repr};
        ${self.py_type_object_name}.tp_flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE);
        ${self.py_type_object_name}.tp_methods = ${self.tp_methods};
        ${self.py_type_object_name}.tp_base = ${self.tp_base};
        #ifdef ${self.py_type_custom_setup_macro_name}
            ${self.py_type_custom_setup_macro_name}(${self.py_type_object_name})
        #endif
        if (PyType_Ready (&${self.py_type_object_name}))
            return -1;
        if (PyModule_AddObject (module, "${self.name}", (PyObject *)&${self.py_type_object_name}))
            return -1;
        """

    py_object_struct = """
        typedef struct {
            PyObject_HEAD
            $python{
                if self.wrapped_type is not None:
                    magic_print('${self.ctype} obj;')
                    magic_print('#define ${self.get_wrapped_func}(obj) (((${self.py_object_struct_name}*)obj)->obj)')
                if self.gtype != GObject.TYPE_NONE:
                    magic_print('GType gtype;')
            }end
        } ${self.py_object_struct_name};
        """

    py_converters = """
        static int
        ${self.from_py_converter} (PyObject *obj, ${self.ctype} *value)
        {
            *value = ${self.get_wrapped_func} (obj);
            return 1;
        }

        static PyObject *
        ${self.to_py_converter} (${self.ctype} value)
        {
            ${self.py_object_struct_name} *obj;
            obj = PyObject_New(${self.py_object_struct_name}, &${self.py_type_object_name});
            obj->obj = value;
            return (PyObject *) obj;
        }
        """

    def __init__(self, info):
        self.info = info
        self.namespace = info.get_namespace()
        self.namespace_lower = self.namespace.lower()
        self.name = info.get_name()
        self.full_name = '%s.%s' % (self.namespace, self.name)
        self.gtype = info.get_g_type()

        info_type = info.get_type()

        self.wrapped_type = info.get_type_name()
        if self.wrapped_type is None:
            self.wrapped_type = "void"

        self.ctype = self.wrapped_type + '*'

        self.py_object_struct_name = 'Py%s%s' % (self.namespace, self.name)
        self.py_type_object_name = '%s_Type' % self.py_object_struct_name
        self.py_type_custom_setup_macro_name = camel_down_under(self.py_type_object_name).upper() + '_CUSTOM_SETUP'

        self.tp_methods = '%s_methods' % self.py_object_struct_name
        self.tp_name = '"%s"' % self.full_name
        self.tp_base = '&PyType_Type'
        self.tp_size = 'sizeof(%s)' % self.py_object_struct_name
        self.tp_repr = 'NULL'

        self.method_table_custom_entries_macro_name = camel_down_under(self.tp_methods).upper() + '_CUSTOM_ENTRIES'

        self.method_builders = [MethodBuilder(method_info, self.py_object_struct_name)
                                for method_info in self.info.get_methods()]


        self.get_wrapped_func = self.py_object_struct_name + '_get'
        self.from_pointer_func = ''
        self.from_py_converter = self.name.lower() + '_from_py'
        self.to_py_converter = self.name.lower() + '_to_py'

        if info.get_type() == InfoType.OBJECT:
            self.ref_func = info.get_ref_function()
            self.unref_func = info.get_ref_function()
        else:
            self.ref_func = None
            self.unref_func = None

    def print_struct_def(self):
        magic_print(self.py_object_struct)

    def print_type_registration(self):
        magic_print(self.py_type_register)

    def print_converters(self):
        magic_print(self.py_converters)

    def print_methods(self):
        for builder in self.method_builders:
            builder.print_wrapper_def()

        magic_print(method_table_template)
        magic_print(py_type_object)


class EnumBuilder(ClassBuilder):
    info_types = (InfoType.ENUM,
                  InfoType.FLAGS)

    enum_values_template = """
        {
            PyObject *_enum_value_names = PyDict_New();
            $python{
                for value_info in self.info.get_values():
                    name = value_info.get_name().upper()
                    value = value_info.get_value()
                    magic_print('pygi_codegen_enum_add_value (&${self.py_type_object_name}, _enum_value_names, "%s", %s);' % (name, value))
            }end
            PyDict_SetItemString (${self.py_type_object_name}.tp_dict,
                                  PYGI_CODEGEN_ENUM_VALUE_NAMES, _enum_value_names);
            Py_DECREF (_enum_value_names);
        }
        """

    py_converters = ""

    def __init__(self, info):
        super().__init__(info)
        self.tp_base = '&PyLong_Type'
        self.tp_repr = 'pygi_codegen_enum_repr'
        # Enums don't actually hold anything
        self.wrapped_type = None
        self.ctype = 'long'

    def print_type_registration(self):
        super().print_type_registration()
        # Fill out the classes tp_dict after PyType_Ready has been
        # called because it will create the dictionary for us.
        magic_print(self.enum_values_template)


"""
INVALID
CALLBACK
CONSTANT
VALUE
PROPERTY
FIELD
ARG
UNRESOLVED) 
"""

class ModuleBuilder(Builder):
    def __init__(self, namespace, output='', prefix='py', command_line='', template=''):
        repo.require(namespace)

        self.prefix = prefix
        self.namespace = namespace
        self.command_line = command_line
        self.namespace_lower = prefix + self.namespace.lower()

        self.tp_methods = self.namespace_lower + '_functions'
        self.method_table_custom_entries_macro_name = camel_down_under(self.tp_methods).upper() + '_CUSTOM_ENTRIES'

        self.input_template = template

        if output:
            self.output_filename = output
            self.output_basename = os.path.splitext(os.path.basename(output))[0]
        else:
            self.output_basename = (self.prefix + self.namespace).lower()
            self.output_filename = self.output_basename + '.c'

        self.class_builders = []
        self.method_builders = []
        self.registered_structs = {}

        for info in repo.get_infos(self.namespace):
            info_type = info.get_type()
            if info_type in EnumBuilder.info_types:
                self.class_builders.append(EnumBuilder(info))

            elif info_type in ClassBuilder.info_types:
                builder = ClassBuilder(info)
                self.class_builders.append(builder)
                self.registered_structs[builder.name] = builder

            elif info_type in FunctionBuilder.info_types:
                self.method_builders.append(FunctionBuilder(info))

    def generate(self):
        global current_context
        current_context = self

        try:
            with open(self.input_template, 'rt') as infile, open(self.output_filename, 'wt') as out:
                magic_print(infile.read(), file=out)

        finally:
            current_context = None


def main(argv):
    parser = argparse.ArgumentParser(description='Python GObject Instrospection Static Binding Generator')
    parser.add_argument('namespace', help='Instrospection Namespace')
    parser.add_argument('-p', '--prefix', default='py', help='')
    parser.add_argument('-o', '--output', default='', help='Output file path, default: ./lowercase(<prefix><namespace>).c')
    parser.add_argument('-t', '--template', help='Template file path')

    # Let argparse handle pulling argv[0] off if we are simply using sys.argv
    if argv == sys.argv:
        options = parser.parse_args()
    else:
        options = parser.parse_args(argv)

    options.command_line = ' '.join(argv)
    codegen = ModuleBuilder(**vars(options))
    codegen.generate()
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
