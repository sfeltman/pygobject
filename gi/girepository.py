
from gi._gi import _girepository
from gi._gi._girepository import *


if 'TypeInfo' not in _girepository.__dict__:
    # if _girepository is not exposing sub-classes simulate the class hierarchy in Python
    # See: foo

    def simulate_type(name, base, prefix):
        functions = {}
        for name, value in _girepository.__dict__.items():
            if name.startswith(prefix):
                func_name = name[len(prefix):]
                functions[func_name] = value
        return type(name, (base,), functions)

    ArgInfo = simulate_type(BaseInfo, 'ArgInfo', 'arg_info')
    TypeInfo = simulate_type(BaseInfo, 'TypeInfo', 'type_info')
    ConstantInfo = simulate_type(BaseInfo, 'ConstantInfo', 'constant_info')
    FieldInfo = simulate_type(BaseInfo, 'FieldInfo', 'field_info')
    PropertyInfo = simulate_type(BaseInfo, 'PropertyInfo', 'property_info')

    # callable info types
    CallableInfo = simulate_type(BaseInfo, 'CallableInfo', 'callable_info')
    FunctionInfo = simulate_type(BaseInfo, 'FunctionInfo', 'function_info')
    SignalInfo = simulate_type(BaseInfo, 'SignalInfo', 'signal_info')
    VFuncInfo = simulate_type(BaseInfo, 'VFuncInfo', 'vfunc_info')

    # registered type info sub-classes
    RegisteredTypeInfo = simulate_type(BaseInfo, 'RegisteredTypeInfo', 'registered_type_info')
    EnumInfo = simulate_type(RegisteredTypeInfo, 'EnumInfo', 'enum_info')
    InterfaceInfo = simulate_type(RegisteredTypeInfo, 'InterfaceInfo', 'interface_info')
    ObjectInfo = simulate_type(RegisteredTypeInfo, 'ObjectInfo', 'object_info')
    StructInfo = simulate_type(RegisteredTypeInfo, 'StructInfo', 'struct_info')
    UnionInfo = simulate_type(RegisteredTypeInfo, 'UnionInfo', 'union_info')
