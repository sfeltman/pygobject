import types
import warnings

from gi import _gobject, PyGIDeprecationWarning

# support overrides in different directories than our gi module
from pkgutil import extend_path
__path__ = extend_path(__path__, __name__)

registry = None


class _Registry(dict):
    def __setitem__(self, key, value):
        '''We do checks here to make sure only submodules of the override
        module are added.  Key and value should be the same object and come
        from the gi.override module.

        We add the override to the dict as "override_module.name".  For instance
        if we were overriding Gtk.Button you would retrive it as such:
        registry['Gtk.Button']
        '''
        if not key == value:
            raise KeyError('You have tried to modify the registry.  This should only be done by the override decorator')

        if not value.__module__.startswith('gi.overrides'):
            raise KeyError('You have tried to modify the registry outside of the overrides module.  This is not allowed')

        # strip gi.overrides from module name
        module = value.__module__[13:]
        key = "%s.%s" % (module, value.__name__)
        super(_Registry, self).__setitem__(key, value)

    def register(self, override_class):
        try:
            info = getattr(override_class, '__info__')
        except AttributeError:
            raise TypeError('Can not override type %s because it does not come from '
                            'an introspection typelib' % override_class.__name__)

        # TODO: should the code for applying the gtype from the
        # info live somewhere else like in the metaclass?
        g_type = info.get_g_type()
        assert g_type != _gobject.TYPE_NONE
        if g_type != _gobject.TYPE_INVALID:
            g_type.pytype = override_class
            self[override_class] = override_class

    def register_func(self, override_func, introspection_func):
        if not hasattr(introspection_func, '__info__'):
            raise TypeError('Can not override function %s because it does not come from '
                            'an introspection typelib' % introspection_func.__name__)

        self[override_func] = override_func


registry = _Registry()


def overridefunc(introspection_func):
    def decorator(override_func):
        registry.register_func(override_func, introspection_func)
        return override_func
    return decorator


def override(type_):
    '''Decorator for registering an override'''
    if isinstance(type_, types.FunctionType):
        return overridefunc(type_)
    else:
        registry.register(type_)
        return type_


def deprecated(fn, replacement):
    '''Decorator for marking methods and classes as deprecated'''
    def wrapped(*args, **kwargs):
        warnings.warn('%s is deprecated; use %s instead' % (fn.__name__, replacement),
                      PyGIDeprecationWarning, stacklevel=2)
        return fn(*args, **kwargs)
    wrapped.__name__ = fn.__name__
    wrapped.__doc__ = fn.__doc__
    wrapped.__dict__.update(fn.__dict__)
    return wrapped
