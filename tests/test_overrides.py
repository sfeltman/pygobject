# -*- Mode: Python; py-indent-offset: 4 -*-
# vim: tabstop=4 shiftwidth=4 expandtab

import unittest

import gi.overrides
import gi.module

try:
    from gi.repository import Regress
    Regress  # pyflakes
except ImportError:
    Regress = None


class TestRegistry(unittest.TestCase):
    def test_non_gi_class(self):
        class MyClass:
            pass

        with self.assertRaisesRegexp(TypeError, 'Can not override type MyClass.*'):
            gi.overrides.override(MyClass)

    def test_non_gi_function(self):
        def my_non_gi_func():
            pass

        with self.assertRaisesRegexp(TypeError, 'Can not override function my_non_gi_func.*'):
            @gi.overrides.override(my_non_gi_func)
            def my_func():
                pass

    def test_override_gi_function(self):
        full_name = 'GIMarshallingTests.boolean_return_true'
        GIMarshallingTests = gi.module.get_introspection_module('GIMarshallingTests')
        introspected_boolean_return_true = GIMarshallingTests.boolean_return_true

        # Verify the registry does not hold boolean_return_true
        self.assertFalse(full_name in gi.overrides.registry)
        self.assertEqual(GIMarshallingTests.boolean_return_true(), True)

        # Make a mocked override of boolean_return_true which returns False
        def boolean_return_true():
            return False
        boolean_return_true.__module__ = 'gi.overrides.GIMarshallingTests'

        # Call the decorator (without using decorator syntax)
        res = gi.overrides.override(GIMarshallingTests.boolean_return_true)(boolean_return_true)
        # The return value is the same as the input override func.
        self.assertEqual(res, boolean_return_true)
        self.assertTrue(full_name in gi.overrides.registry)

        # In this case simply overriding a function will add it to the registry
        # but should not modify the original introspection module.
        self.assertEqual(GIMarshallingTests.boolean_return_true(), True)
        self.assertEqual(GIMarshallingTests.boolean_return_true,
                         introspected_boolean_return_true)

        del gi.overrides.registry[full_name]

    @unittest.skipUnless(Regress, 'built without cairo support')
    def test_separate_path(self):
        # Regress override is in tests/gi/overrides, separate from gi/overrides
        # https://bugzilla.gnome.org/show_bug.cgi?id=680913
        self.assertEqual(Regress.REGRESS_OVERRIDE, 42)


class TestModule(unittest.TestCase):
    # Tests for gi.module

    def test_get_introspection_module_caching(self):
        # This test attempts to minimize side effects by
        # using a DynamicModule directly instead of going though:
        # from gi.repository import Foo

        # Clear out introspection module cache before running this test.
        old_modules = gi.module._introspection_modules
        gi.module._introspection_modules = {}

        mod_name = 'GIMarshallingTests'
        mod1 = gi.module.get_introspection_module(mod_name)
        mod2 = gi.module.get_introspection_module(mod_name)
        self.assertTrue(mod1 is mod2)

        # Using a DynamicModule will use get_introspection_module internally
        # in its _load method.
        mod_overridden = gi.module.DynamicModule(mod_name)
        mod_overridden._load()
        self.assertTrue(mod1 is mod_overridden._introspection_module)

        # Restore the previous cache
        gi.module._introspection_modules = old_modules
