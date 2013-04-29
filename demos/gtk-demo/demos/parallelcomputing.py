#!/usr/bin/env python
# -*- Mode: Python; py-indent-offset: 4 -*-
# vim: tabstop=4 shiftwidth=4 expandtab
#
# Copyright (C) 2013 Simon Feltman <s.feltman@gmail.com>
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

"""
Example script showing interaction of Python threads and
a Gtk.Application GUI. Threads can be created by pressing
one of the buttons to start a computation using various CPU
bound techniques.

Normal - Calls Python "sum" 100 times with an array of zero through
one million. This shows of poor responsiveness of Python threads
running CPU bound computation. The call to "sum" does not release
the GIL (Global Interpreter Lock) and also does not give time to
other Python threads, which blocks the GUI.

Interleaved - With this, the "sum" computation is broken down
into many calls. This allows the Python interpreter to interleave
the compute threads with other Python threads, making the app
more interactive but with some GUI sluggishness and at the expense
of the computation running slower.

Numpy - A sum computed using numpy.sum with a numpy.array.
This technique releaves the Python worker thread from blocking
other Python threads for the duration of the computation.
This allows full GUI interactivity with thread parallelism.

Multiprocessing - Uses the same "Normal" mode computation but within
a separate process by utilizing the Python "multiprocessing" module.
This allows full GUI interactivity with process parallelism.
"""

title = "Parallel Computing"
description = __doc__

import sys
import threading
import multiprocessing
from contextlib import contextmanager

# sys.setswitchinterval(0.0005)

from gi.repository import GLib
from gi.repository import Gtk
from gi.repository import Gdk


def compute_sum_normal(update_func):
    data = range(10000000)
    for i in range(100):
        sum(data)
        update_func(i + 1)


def compute_sum_interleaved(update_func):
    data_range = range(0, 10000000, 100)
    for i in range(100):
        total = 0
        for j in data_range:
            total += sum(range(j, j + 100))
        update_func(i + 1)


def compute_sum_numpy(update_func):
    import numpy
    data = numpy.arange(10000000)
    for i in range(100):
        numpy.sum(data)
        update_func(i + 1)


@contextmanager
def setswitchinterval(interval):
    old_interval = sys.getswitchinterval()
    sys.setswitchinterval(interval)
    yield
    sys.setswitchinterval(old_interval)


def ensure_thread_completion(interval=1.0):
    def ensure_decorator(func):
        def wrapper(*args, **kwargs):
            old_interval = sys.getswitchinterval()
            sys.setswitchinterval(interval)
            res = func(*args, **kwargs)
            sys.setswitchinterval(old_interval)
            return res
        return wrapper
    return ensure_decorator


class App(Gtk.Application):
    ITEM_NAME = 0
    ITEM_PROGRESS = 1
    ITEM_PROGRESS_TEXT = 2
    ITEM_COMPLETION_TIME = 3

    def __init__(self):
        Gtk.Application.__init__(self, application_id='PyGObject.GtkThreadingExample')

        self.counter = 0
        self.model = None

    def schedule_item_update(self, iterator, item_idx, value):
        """Schedule a GUI update from a worker thread."""
        def set_item(value):
            print('--set_item_begin--')
            with setswitchinterval(0.0001):
                self.model[iterator][item_idx] = value
            print('--set_item_end--')

        Gdk.threads_add_idle(GLib.PRIORITY_DEFAULT,
                             set_item,
                             value)

    def on_compute_with_thread(self, button, compute_func):
        iterator = self.model.append(('', 0, '', ''))

        def schedule_update(percent):
            self.schedule_item_update(iterator, self.ITEM_PROGRESS, percent)

        thread = threading.Thread(target=compute_func, args=(schedule_update,))
        thread.setDaemon(True)
        thread.start()

        self.model[iterator][self.ITEM_NAME] = '%s (%s)' % (thread.name,
                                                            button.get_label())

    def on_compute_with_multiprocessing(self, button, compute_func):
        iterator = self.model.append(('', 0, '', ''))

        def watch_func(connection, condition):
            """Receives data from the subprocess."""
            item_idx, value = connection.recv()
            self.schedule_item_update(iterator, item_idx, value)

            # Return False to remove the source
            if item_idx == self.ITEM_PROGRESS and value >= 100:
                connection.close()
                return False
            else:
                return True

        # Watch a Python connection with the GLib/Gtk main loop.
        parent_conn, child_conn = multiprocessing.Pipe()
        GLib.io_add_watch(parent_conn,
                          GLib.IOCondition.IN,
                          watch_func)

        def process_func(connection):
            """Processing function called in subprocess."""
            def update(percent):
                connection.send((self.ITEM_PROGRESS, percent))

            compute_func(update)
            connection.close()

        process = multiprocessing.Process(target=process_func,
                                          args=(child_conn,))
        process.start()

        self.model[iterator][self.ITEM_NAME] = '%s (%s)' % (process.name,
                                                            button.get_label())

    def create_tree_and_model(self):
        store = Gtk.ListStore(str, int, str, str)
        tree_view = Gtk.TreeView(model=store)

        # Name/Type
        renderer = Gtk.CellRendererText()
        column = Gtk.TreeViewColumn('Task', renderer,
                                    text=self.ITEM_NAME)
        tree_view.append_column(column)

        # Task Progress
        renderer = Gtk.CellRendererProgress()
        column = Gtk.TreeViewColumn('Progress', renderer,
                                    value=self.ITEM_PROGRESS,
                                    text=self.ITEM_PROGRESS_TEXT)
        tree_view.append_column(column)

        # Completion Time
        renderer = Gtk.CellRendererText()
        column = Gtk.TreeViewColumn('Time', renderer,
                                    text=self.ITEM_COMPLETION_TIME)
        #tree_view.append_column(column)

        return tree_view, store

    def on_activate(self, app):
        self.window = Gtk.ApplicationWindow.new(self)
        self.window.set_title(title)

        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.window.add(box)

        scrolled_window = Gtk.ScrolledWindow()
        box.pack_start(scrolled_window, expand=True, fill=True, padding=0)

        self.tree_view, self.model = self.create_tree_and_model()
        scrolled_window.add(self.tree_view)

        button_box = Gtk.HButtonBox()
        box.pack_start(button_box, expand=False, fill=False, padding=0)

        for name, callback, func in (('Normal', self.on_compute_with_thread, compute_sum_normal),
                                     ('Interleaved', self.on_compute_with_thread, compute_sum_interleaved),
                                     ('Numpy', self.on_compute_with_thread, compute_sum_numpy),
                                     ('Multiprocessing', self.on_compute_with_multiprocessing, compute_sum_normal)):
            button = Gtk.Button(name)
            button_box.add(button)
            button.connect('pressed', callback, func)

        self.window.show_all()

    def run(self, argv):
        GLib.threads_init()  # Needed with PyGObject < 3.9
        Gdk.threads_init()  # Needed with Gtk < 4.0

        self.connect('activate', self.on_activate)
        return super(App, self).run(argv)


def main(demoapp=None):
    """Entry point for demo manager"""
    app = App()
    raise SystemExit(app.run([]))


if __name__ == '__main__':
    app = App()
    raise SystemExit(app.run(sys.argv))
