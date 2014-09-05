#!/usr/bin/env python
# -*- Mode: Python; py-indent-offset: 4 -*-
# vim: tabstop=4 shiftwidth=4 expandtab
#
# Copyright (C) 2010 Red Hat, Inc., John (J5) Palmieri <johnp@redhat.com>
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

title = "PyGI Demo App"
description = """
Python GObject-Introspection demo application showing various techniques for
programming the GNOME platform.
"""

import codecs
import os
import sys
import textwrap
import time
import re

from gi.repository import GLib, GObject, Pango, GdkPixbuf, Gtk, Gio

try:
    from gi.repository import GtkSource
    GtkSource  # PyFlakes
except ImportError:
    GtkSource = None


DEMOROOTDIR = os.path.abspath(os.path.dirname(__file__))
DEMOCODEDIR = os.path.join(DEMOROOTDIR, 'demos')
sys.path.insert(0, DEMOROOTDIR)


class Indexer(object):
    def __init__(self):
        self.index = {}
        self.pattern = re.compile('[^a-zA-Z0-9_]')

    def add_file(self, filename, user_data):
        with open(filename, 'rt') as file:
            data = file.read()
            for key in set(self.pattern.split(data)):
                items = self.index.setdefault(key.lower(), set())
                items.add(user_data)

    def find(self, key):
        return self.index.get(key.lower(), set())


class Demo(GObject.GObject):
    __gtype_name__ = 'GtkDemo'

    def __init__(self, title, module, filename):
        super(Demo, self).__init__()

        self.title = title
        self.module = module
        self.filename = filename

    @classmethod
    def new_from_file(cls, path):
        relpath = os.path.relpath(path, DEMOROOTDIR)
        packagename = os.path.dirname(relpath).replace(os.sep, '.')
        modulename = os.path.basename(relpath)[0:-3]

        package = __import__(packagename, globals(), locals(), [modulename], 0)
        module = getattr(package, modulename)

        try:
            return cls(module.title, module, path)
        except AttributeError as e:
            raise AttributeError('(%s): %s' % (path, e.message))


class DemoTreeStore(Gtk.TreeStore):
    __gtype_name__ = 'GtkDemoTreeStore'

    COL_TITLE = 0
    COL_DEMO = 1
    COL_FILENAME = 2

    def __init__(self, indexer):
        super(DemoTreeStore, self).__init__(str, Demo, Pango.Style)

        self._parent_nodes = {}

        # Add "self" module as the first demo program listed.
        module = sys.modules[__name__]
        demo = Demo(module.title, module, __file__)
        self.append(None, (demo.title, demo, Pango.Style.NORMAL))
        indexer.add_file(__file__, demo)

        for filename in self._list_dir(DEMOCODEDIR):
            fullpath = os.path.join(DEMOCODEDIR, filename)
            initfile = os.path.join(os.path.dirname(fullpath), '__init__.py')

            if fullpath != initfile and os.path.isfile(initfile) and fullpath.endswith('.py'):
                parentname = os.path.dirname(os.path.relpath(fullpath, DEMOCODEDIR))

                if parentname:
                    parent = self._get_parent_node(parentname)
                else:
                    parent = None

                demo = Demo.new_from_file(fullpath)
                self.append(parent, (demo.title, demo, Pango.Style.NORMAL))
                indexer.add_file(fullpath, demo)

    def _list_dir(self, path):
        demo_file_list = []

        for filename in os.listdir(path):
            fullpath = os.path.join(path, filename)

            if os.path.isdir(fullpath):
                demo_file_list.extend(self._list_dir(fullpath))
            elif os.path.isfile(fullpath):
                demo_file_list.append(fullpath)

        return sorted(demo_file_list, key=str.lower)

    def _get_parent_node(self, name):
        if name not in self._parent_nodes.keys():
            node = self.append(None, (name, None, Pango.Style.NORMAL))
            self._parent_nodes[name] = node

        return self._parent_nodes[name]


class SideBar(Gtk.Box):
    demo_changed = GObject.Signal(arg_types=[GObject.Object])

    def __init__(self):
        super(SideBar, self).__init__(orientation=Gtk.Orientation.VERTICAL)

        self.selected_demo = None
        self.indexer = Indexer()
        self.entry = Gtk.SearchEntry(placeholder_text='Search...')
        self.searchbar = Gtk.SearchBar(hexpand=False)

        self.tree_store = DemoTreeStore(self.indexer)
        self.filter_model = Gtk.TreeModelFilter(child_model=self.tree_store)
        self.filter_model.set_visible_func(self.on_filter)

        self.tree_view = Gtk.TreeView(model=self.filter_model,
                                      headers_visible=False)

        self.scrolled = Gtk.ScrolledWindow(hadjustment=None,
                                           vadjustment=None,
                                           hscrollbar_policy=Gtk.PolicyType.NEVER,
                                           vscrollbar_policy=Gtk.PolicyType.AUTOMATIC)

        self.selection = self.tree_view.get_selection()
        self.selection.set_mode(Gtk.SelectionMode.BROWSE)
        self.tree_view.set_size_request(200, -1)

        self.entry.connect('search-changed', lambda entry: self.filter_model.refilter())
        self.searchbar.add(self.entry)

        cell = Gtk.CellRendererText()
        column = Gtk.TreeViewColumn(title='Widget (double click for demo)',
                                    cell_renderer=cell,
                                    text=0,
                                    style=2)

        first_iter = self.filter_model.get_iter_first()
        if first_iter is not None:
            self.selection.select_iter(first_iter)

        self.selection.connect('changed', self.on_selection_changed, self.filter_model)
        self.tree_view.connect('row_activated', self.on_row_activated, self.filter_model)

        self.tree_view.append_column(column)
        self.tree_view.expand_all()
        self.tree_view.set_headers_visible(False)

        self.scrolled.add(self.tree_view)
        self.tree_view.grab_focus()

        self.pack_start(self.searchbar, False, False, 0)
        self.pack_start(self.scrolled, True, True, 0)

    def on_selection_changed(self, selection, model):
        sel = selection.get_selected()
        if sel == ():
            return

        treeiter = sel[1]
        if treeiter is None:
            return

        title = model.get_value(treeiter, DemoTreeStore.COL_TITLE)
        demo = model.get_value(treeiter, DemoTreeStore.COL_DEMO)

        if demo and demo != self.selected_demo:
            self.demo_changed.emit(demo)
            self.selected_demo = demo

    def on_row_activated(self, view, path, col, model):
        print('on_row_activated')
        iter = model.get_iter(path)
        demo = model.get_value(iter, 1)

        if demo is not None:
            model.set_value(iter, 2, Pango.Style.ITALIC)
            try:
                demo.module.main(self)
            finally:
                model.set_value(iter, 2, Pango.Style.NORMAL)

    def toggle_search(self):
         if self.searchbar.get_search_mode():
             self.searchbar.set_search_mode(False)
             self.entry.set_text('')
         else:
            self.searchbar.set_search_mode(True)
            self.entry.grab_focus()

    def on_filter(self, model, treeiter, data):
        if treeiter is None:
            return True

        filter_text = self.entry.get_text()
        if not filter_text:
            return True

        demo = model.get_value(treeiter, DemoTreeStore.COL_DEMO)
        for key in filter_text.split():
            if demo in self.indexer.find(key):
                return True
        return False


class App(Gtk.Application):
    app_count = 0  # Allow multiple launches of this app within the same process.

    def __init__(self):
        super(App, self).__init__(application_id='org.gnome.pygi.demo' + str(self.app_count))
        App.app_count += 1

        self.gtksettings = Gtk.Settings.get_default()

        # Use a GResource to hold the CSS files. Resource bundles are created by
        # the glib-compile-resources program shipped with Glib which takes an xml
        # file that describes the bundle, and a set of files that the xml
        # references. These are combined into a binary resource bundle.
        base_path = os.path.abspath(os.path.dirname(__file__))
        resource_path = os.path.join(base_path, 'demos/data/demo.gresource')
        resource = Gio.Resource.load(resource_path)

        # FIXME: method register() should be without the underscore
        # FIXME: see https://bugzilla.gnome.org/show_bug.cgi?id=684319
        # Once the resource has been globally registered it can be used
        # throughout the application.
        resource._register()

    def on_activate(self, app):
        self.window = Gtk.ApplicationWindow.new(self)
        self.window.set_title('PyGI Code Demos')
        self.window.set_default_size(600, 400)
        self.setup_default_icon()

        self.hsize_group = Gtk.SizeGroup(Gtk.SizeGroupMode.HORIZONTAL)

        header = Gtk.Box()
        sidebar = SideBar()
        sidebar.demo_changed.connect(self.on_demo_changed)

        left_header_bar = Gtk.HeaderBar(show_close_button=False,
                                        title='PyGI Demos',
                                        subtitle='(double click to launch)')
        icon = Gtk.Image()
        icon.set_from_icon_name('edit-find-symbolic', Gtk.IconSize.MENU)
        self.search_button = Gtk.ToggleButton()
        self.search_button.add(icon)
        self.search_button.connect('toggled', lambda btn: sidebar.toggle_search())
        self.search_button.props.valign = Gtk.Align.CENTER
        self.search_button.get_style_context().add_class('image-button')
        left_header_bar.pack_start(self.search_button)

        right_header_bar = Gtk.HeaderBar(show_close_button=True)
        header.pack_start(left_header_bar, False, False, 0)
        header.pack_start(right_header_bar, True, True, 0)

        self.window.set_titlebar(header)
        self.hsize_group.add_widget(left_header_bar)

        stack = Gtk.Stack(transition_type=Gtk.StackTransitionType.SLIDE_LEFT_RIGHT,
                          homogeneous=True)
        switcher = Gtk.StackSwitcher(stack=stack, halign=Gtk.Align.CENTER)

        right_header_bar.set_custom_title(switcher)

        hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL,
                       homogeneous=False,
                       spacing=0)
        self.window.add(hbox)

        hbox.pack_start(child=sidebar, expand=False, fill=False, padding=0)
        hbox.pack_start(child=stack, expand=True, fill=True, padding=0)

        text_widget, info_buffer = self.create_text_view()
        stack.add_titled(text_widget, name='info', title='Info')

        self.info_buffer = info_buffer
        self.info_buffer.create_tag('title', font='Sans 18')

        text_widget, self.source_buffer = self.create_source_view()
        stack.add_titled(text_widget, name='source', title='Source')

        self.window.show_all()

    def find_file(self, base=''):
        dir = os.path.join(DEMOCODEDIR, 'data')
        logo_file = os.path.join(dir, 'gtk-logo-rgb.gif')
        base_file = os.path.join(dir, base)

        if (GLib.file_test(logo_file, GLib.FileTest.EXISTS) and
                GLib.file_test(base_file, GLib.FileTest.EXISTS)):
            return base_file
        else:
            filename = os.path.join(DEMOCODEDIR, base)

            if GLib.file_test(filename, GLib.FileTest.EXISTS):
                return filename

            # can't find the file
            raise IOError('Cannot find demo data file "%s"' % base)

    def setup_default_icon(self):
        filename = self.find_file('gtk-logo-rgb.gif')
        pixbuf = GdkPixbuf.Pixbuf.new_from_file(filename)
        transparent = pixbuf.add_alpha(True, 0xff, 0xff, 0xff)
        list = []
        list.append(transparent)
        Gtk.Window.set_default_icon_list(list)

    def on_demo_changed(self, sidebar, demo):
        # Split into paragraphs based on double newlines and use
        # textwrap to strip out all other formatting whitespace
        description = ''
        for paragraph in demo.module.description.split('\n\n'):
            description += '\n'.join(textwrap.wrap(paragraph, 99999))
            description += '\n\n'  # Add paragraphs back in

        f = codecs.open(demo.filename, 'rU', 'utf-8')
        code = f.read()
        f.close()

        # output and style the title
        (start, end) = self.info_buffer.get_bounds()
        self.info_buffer.delete(start, end)
        (start, end) = self.source_buffer.get_bounds()
        self.source_buffer.delete(start, end)

        start = self.info_buffer.get_iter_at_offset(0)
        end = start.copy()
        self.info_buffer.insert(end, demo.title)
        start = end.copy()
        start.backward_chars(len(demo.title))
        self.info_buffer.apply_tag_by_name('title', start, end)
        self.info_buffer.insert(end, '\n')

        # output the description
        self.info_buffer.insert(end, description)

        # output the code
        start = self.source_buffer.get_iter_at_offset(0)
        end = start.copy()
        self.source_buffer.insert(end, code)

    def create_scrolled_window(self):
        scrolled_window = Gtk.ScrolledWindow(hadjustment=None,
                                             vadjustment=None)
        scrolled_window.set_policy(Gtk.PolicyType.AUTOMATIC,
                                   Gtk.PolicyType.AUTOMATIC)
        scrolled_window.set_shadow_type(Gtk.ShadowType.IN)
        return scrolled_window

    def create_text_view(self):
        text_view = Gtk.TextView()
        buffer = Gtk.TextBuffer()

        text_view.set_buffer(buffer)
        text_view.set_editable(False)
        text_view.set_cursor_visible(False)

        scrolled_window = self.create_scrolled_window()
        scrolled_window.add(text_view)

        text_view.set_wrap_mode(Gtk.WrapMode.WORD)
        text_view.set_pixels_above_lines(2)
        text_view.set_pixels_below_lines(2)

        return scrolled_window, buffer

    def create_source_view(self):
        font_desc = Pango.FontDescription('monospace 11')

        if GtkSource:
            lang_mgr = GtkSource.LanguageManager()
            lang = lang_mgr.get_language('python')

            # Choose a resonable syntax hightlighting color scheme.
            scheme_mgr = GtkSource.StyleSchemeManager.get_default()
            if self.gtksettings.get_property('gtk-application-prefer-dark-theme'):
                scheme = scheme_mgr.get_scheme('oblivion')

            buffer = GtkSource.Buffer()

            if scheme:
                buffer.set_style_scheme(scheme)

            buffer.set_language(lang)
            buffer.set_highlight_syntax(True)

            view = GtkSource.View()
            view.set_buffer(buffer)
            view.set_show_line_numbers(True)

            scrolled_window = self.create_scrolled_window()
            scrolled_window.add(view)

        else:
            scrolled_window, buffer = self.create_text_view()
            view = scrolled_window.get_child()

        view.modify_font(font_desc)
        view.set_wrap_mode(Gtk.WrapMode.NONE)
        return scrolled_window, buffer

    def run(self, argv):
        self.connect('activate', self.on_activate)
        return super(App, self).run(argv)


def main(demoapp=None):
    """Entry point for demo manager"""
    app = App()
    return app.run(sys.argv)


if __name__ == '__main__':
    app = App()
    raise SystemExit(app.run(sys.argv))
