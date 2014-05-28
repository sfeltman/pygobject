#!/usr/bin/env python
# -*- Mode: Python; py-indent-offset: 4 -*-
# vim: tabstop=4 shiftwidth=4 expandtab
#
# Copyright (C) 2014 Simon Feltman <sfeltman@gnome.org>
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
# License along with this library; if not, see <http://www.gnu.org/licenses/>.

title = "Composite Template"
description = """
Demonstrates an interface loaded from a Builder XML description which seemlessly
integrates into a Python sub-class.
"""

from gi.repository import Gtk
from gi.repository.Gtk import Template, TemplateChild, TemplateCallback
from gi.repository.GObject import Property, Signal, SignalFlags


@Template(ui='spin_selector.ui')
class PySpinSelector(Gtk.Box):
    adjustment = Property(type=Gtk.Adjustment)
    display_area = TemplateChild('display_area')
    up_button = TemplateChild('down_button')
    down_button = TemplateChild('down_button')

    def __init__(self, adjustment, **kwargs):
        super(PySpinSelector, self).__init__(**kwargs)
        self.adjustment = adjustment
        self.adjustment.connect('value-changed', self.on_value_changed)
        self.current_values_text = self.get_current_values_text()

    @classmethod
    def new_from_range(cls, value, lower, upper, step):
        adjustment = Gtk.Adjustment.new(value, lower, upper, step, 1.0, 1.0)
        return cls(adjustment)

    def get_current_values_text(self):
        value = self.adjustment.props.value
        increment = self.adjustment.props.step_increment

        first = self.output.emit(value + increment)
        second = self.output.emit(value)
        third = self.output.emit(value - increment)

        return (first, second, third)

    @Signal(return_type=str, arg_types=(int,), flags=SignalFlags.RUN_LAST)
    def output(self, value):
        return str(value)

    def on_value_changed(self, adjustment):
        self.current_values_text = self.get_current_values_text()
        self.display_area.invalidate_rect(None, False)

    @TemplateCallback
    def on_up_button_clicked(self, btn):
        self.adjustment.props.value += self.adjustment.props.step_increment

    @TemplateCallback
    def on_down_button_clicked(self, btn):
        self.adjustment.props.value -= self.adjustment.props.step_increment

    @TemplateCallback
    def on_display_area_draw(self, context, widget):
        print(self, self.current_values_text)


class App(object):
    def __init__(self):
        foo = PySpinSelector.new_from_range(value=0, lower=0, upper=10, step=1)
        window = Gtk.Window()
        window.add(foo)
        window.connect('destroy', Gtk.main_quit)
        window.show_all()

    def run(self):
        Gtk.main()


def main(demoapp=None):
    app = App()
    app.run()


if __name__ == '__main__':
    main()
