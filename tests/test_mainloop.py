# -*- Mode: Python -*-

import os
import sys
import select
import signal
import time
import unittest

try:
    from _thread import start_new_thread
    start_new_thread  # pyflakes
except ImportError:
    # Python 2
    from thread import start_new_thread
from gi.repository import GLib

from compathelper import _bytes


class TestMainLoop(unittest.TestCase):
    def test_exception_handling(self):
        pipe_r, pipe_w = os.pipe()

        pid = os.fork()
        if pid == 0:
            os.close(pipe_w)
            select.select([pipe_r], [], [])
            os.close(pipe_r)
            os._exit(1)

        def child_died(pid, status, loop):
            loop.quit()
            raise Exception("deadbabe")

        loop = GLib.MainLoop()
        GLib.child_watch_add(GLib.PRIORITY_DEFAULT, pid, child_died, loop)

        os.close(pipe_r)
        os.write(pipe_w, _bytes("Y"))
        os.close(pipe_w)

        def excepthook(type, value, traceback):
            self.assertTrue(type is Exception)
            self.assertEqual(value.args[0], "deadbabe")
        sys.excepthook = excepthook
        try:
            got_exception = False
            try:
                loop.run()
            except:
                got_exception = True
        finally:
            sys.excepthook = sys.__excepthook__

        #
        # The exception should be handled (by printing it)
        # immediately on return from child_died() rather
        # than here. See bug #303573
        #
        self.assertFalse(got_exception)

    def test_concurrency(self):
        def on_usr1(signum, frame):
            pass

        try:
            # create a thread which will terminate upon SIGUSR1 by way of
            # interrupting sleep()
            orig_handler = signal.signal(signal.SIGUSR1, on_usr1)
            start_new_thread(time.sleep, (10,))

            # now create two main loops
            loop1 = GLib.MainLoop()
            loop2 = GLib.MainLoop()
            GLib.timeout_add(100, lambda: os.kill(os.getpid(), signal.SIGUSR1))
            GLib.timeout_add(500, loop1.quit)
            loop1.run()
            loop2.quit()
        finally:
            signal.signal(signal.SIGUSR1, orig_handler)

    def test_sigint(self):
        pid = os.fork()
        if pid == 0:
            time.sleep(0.5)
            os.kill(os.getppid(), signal.SIGINT)
            os._exit(0)

        loop = GLib.MainLoop()
        try:
            loop.run()
            self.fail('expected KeyboardInterrupt exception')
        except KeyboardInterrupt:
            pass
        self.assertFalse(loop.is_running())
        os.waitpid(pid, 0)

    def test_interruptible_loop_context_nesting(self):
        quits_called = []

        def quit1():
            quits_called.append(1)

        def quit2():
            quits_called.append(2)

        def quit3():
            quits_called.append(3)

        # Simulate using three nested context managers
        # with Context(loop1):         # Receives Ctrl+C
        #    with Context(loop2):      # Exits normally
        #        with Context(loop3):  # Receives Ctrl+C
        #             pass

        # creation of a context manager does nothing to the globals
        context1 = GLib.InterruptibleLoopContext(quit1)
        self.assertTrue(GLib.InterruptibleLoopContext._signal_source_id is None)
        self.assertEqual(len(GLib.InterruptibleLoopContext._loop_contexts), 0)

        # 1st context enter
        context1.__enter__()

        first_source_id = GLib.InterruptibleLoopContext._signal_source_id
        self.assertFalse(first_source_id is None)
        self.assertEqual(len(GLib.InterruptibleLoopContext._loop_contexts), 1)

        # 2nd context enter
        context2 = GLib.InterruptibleLoopContext(quit2)
        context2.__enter__()

        # Make sure the source does not change after entering the second context
        self.assertEqual(GLib.InterruptibleLoopContext._signal_source_id,
                         first_source_id)
        self.assertEqual(len(GLib.InterruptibleLoopContext._loop_contexts), 2)

        # 3rd context enter
        context3 = GLib.InterruptibleLoopContext(quit3)
        context3.__enter__()

        self.assertEqual(len(GLib.InterruptibleLoopContext._loop_contexts), 3)

        # Simulate a SIGINT, this will flag the context with _quit_by_sigint
        # and call the quit3 callback
        self.assertFalse(context3._quit_by_sigint)
        GLib.InterruptibleLoopContext._glib_sigint_handler(None)
        self.assertTrue(context3._quit_by_sigint)

        # Exiting the 3rd context will raise a KeyboardInterrupt and
        # pop itself off the global stack
        self.assertRaises(KeyboardInterrupt, context3.__exit__, None, None, None)
        self.assertEqual(len(GLib.InterruptibleLoopContext._loop_contexts), 2)

        # Exit the 2nd loop without calling _glib_sigint_handler to simulate Ctrl+C
        context2.__exit__(None, None, None)
        self.assertEqual(len(GLib.InterruptibleLoopContext._loop_contexts), 1)

        # Simulate Ctrl+C for 1st loop exit
        self.assertFalse(context1._quit_by_sigint)
        GLib.InterruptibleLoopContext._glib_sigint_handler(None)
        self.assertTrue(context1._quit_by_sigint)

        # Exiting the 1st loop after SIGINT, all contexts should be cleared and
        # the signal source should be cleared out.
        self.assertRaises(KeyboardInterrupt, context1.__exit__, None, None, None)
        self.assertEqual(len(GLib.InterruptibleLoopContext._loop_contexts), 0)
        self.assertEqual(GLib.InterruptibleLoopContext._signal_source_id,
                         None)

        # Finally test the results of quits_called
        self.assertEqual(len(quits_called), 2)
        self.assertTrue(1 in quits_called)
        self.assertTrue(3 in quits_called)
