"""
Example script showing interaction of Python threads with
a Gio.Applications main loop by logging interleaved messages.
"""
import sys
import threading
import logging

from gi.repository import GLib
from gi.repository import Gio

logging.basicConfig(level=logging.INFO)


class App(Gio.Application):
    def __init__(self):
        Gio.Application.__init__(self, application_id='PyGObject.ThreadingExample')

    def start_compute_thread(self):
        def compute():
            for i in range(100):
                sum(range(1000000))
                logging.info('Update %s: %i' % (threading.current_thread().name, i))

        thread = threading.Thread(target=compute)
        thread.setDaemon(True)
        thread.start()

    def on_quit_timeout(self):
        self.release()
        return False

    def on_main_thread_timeout(self):
        logging.info('MAIN THREAD')
        return True

    def on_activate(self, app):
        GLib.timeout_add(10, self.on_main_thread_timeout)
        GLib.timeout_add(1000, self.on_quit_timeout)

        for i in range(4):
            self.start_compute_thread()

    def run(self, argv):
        GLib.threads_init()
        self.hold()
        self.connect('activate', self.on_activate)
        return super(App, self).run(argv)

if __name__ == '__main__':
    app = App()
    raise SystemExit(app.run(sys.argv))
