#!/usr/bin/env python
"""
scene.c from the Redbook examples.
Converted to Python by Jason Petrone(jp@demonseed.net) 8/00
Ported to use GtkGL by Simon Feltman <sfeltman@gnome.org>

/*
 * Copyright (c) 1993-1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * Permission to use, copy, modify, and distribute this software for
 * any purpose and without fee is hereby granted, provided that the above
 * copyright notice appear in all copies and that both the copyright notice
 * and this permission notice appear in supporting documentation, and that
 * the name of Silicon Graphics, Inc. not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission.
 *
 * THE MATERIAL EMBODIED ON THIS SOFTWARE IS PROVIDED TO YOU "AS-IS"
 * AND WITHOUT WARRANTY OF ANY KIND, EXPRESS, IMPLIED OR OTHERWISE,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY OR
 * FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL SILICON
 * GRAPHICS, INC.  BE LIABLE TO YOU OR ANYONE ELSE FOR ANY DIRECT,
 * SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY
 * KIND, OR ANY DAMAGES WHATSOEVER, INCLUDING WITHOUT LIMITATION,
 * LOSS OF PROFIT, LOSS OF USE, SAVINGS OR REVENUE, OR THE CLAIMS OF
 * THIRD PARTIES, WHETHER OR NOT SILICON GRAPHICS, INC.  HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH LOSS, HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE
 * POSSESSION, USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * US Government Users Restricted Rights
 * Use, duplication, or disclosure by the Government is subject to
 * restrictions set forth in FAR 52.227.19(c)(2) or subparagraph
 * (c)(1)(ii) of the Rights in Technical Data and Computer Software
 * clause at DFARS 252.227-7013 and/or in similar or successor
 * clauses in the FAR or the DOD or NASA FAR Supplement.
 * Unpublished-- rights reserved under the copyright laws of the
 * United States.  Contractor/manufacturer is Silicon Graphics,
 * Inc., 2011 N.  Shoreline Blvd., Mountain View, CA 94039-7311.
 *
 * OpenGL(R) is a registered trademark of Silicon Graphics, Inc.
 */
"""

title = "Scene"
description = """
This program demonstrates the use of the GL lighting model.
Objects are drawn using a grey material characteristic.
A single light source illuminates the objects.
"""

import os
import sys

from gi.repository import GLib
from gi.repository import Gtk
from gi.repository import GtkGL
ConfigAttr = GtkGL.ConfigAttr

from OpenGL.GLUT import *
from OpenGL.GL import *


class App(object):
    def __init__(self, demoapp):
        self.window = Gtk.Window()
        self.window.set_title('OpenGL Scene')
        self.window.set_default_size(400, 300)
        self.draw_id = None

        self.area = GtkGL.Area.new([ConfigAttr.RGBA,
                                    ConfigAttr.RED_SIZE, 8,
                                    ConfigAttr.GREEN_SIZE, 8,
                                    ConfigAttr.BLUE_SIZE, 8,
                                    ConfigAttr.DEPTH_SIZE, 8,
                                    ])

        self.area.connect('configure-event', self.on_reshape)
        self.window.connect('destroy', self.on_destroy)

        self.window.add(self.area)
        self.window.show_all()

    def on_init(self):
        glutInit(sys.argv)
        self.area.make_current()
        light_ambient =  [0.0, 0.0, 0.0, 1.0]
        light_diffuse =  [1.0, 1.0, 1.0, 1.0]
        light_specular =  [1.0, 1.0, 1.0, 1.0]
        #  light_position is NOT default value
        light_position =  [1.0, 1.0, 1.0, 0.0]

        glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient)
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse)
        glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular)
        glLightfv(GL_LIGHT0, GL_POSITION, light_position)

        glEnable(GL_LIGHTING)
        glEnable(GL_LIGHT0)
        glEnable(GL_DEPTH_TEST)

        self.draw_id = GLib.idle_add(self.on_draw)

    def on_destroy(self, *args):
        # We must remove the source from the main loop otherwise it will
        # keep running after the window is closed.
        if self.draw_id is not None:
            GLib.source_remove(self.draw_id)
        Gtk.main_quit()

    def on_reshape(self, widget, event):
        if self.draw_id is None:
            self.on_init()

        self.area.make_current()
        w, h = event.width, event.height
        glViewport(0, 0, w, h)
        glMatrixMode (GL_PROJECTION)
        glLoadIdentity()
        if w <= h:
           glOrtho(-2.5, 2.5, -2.5*h/w,
                    2.5*h/w, -10.0, 10.0)
        else:
           glOrtho(-2.5*w/h,
                    2.5*w/h, -2.5, 2.5, -10.0, 10.0)
        glMatrixMode(GL_MODELVIEW)
        glLoadIdentity()

    def on_draw(self):
        self.area.make_current()

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glPushMatrix()
        glRotatef(20.0, 1.0, 0.0, 0.0)
        glPushMatrix()
        glTranslatef(-0.75, 0.5, 0.0);
        glRotatef(90.0, 1.0, 0.0, 0.0)
        glutSolidTorus(0.275, 0.85, 15, 15)
        glPopMatrix()

        glPushMatrix()
        glTranslatef(-0.75, -0.5, 0.0);
        glRotatef (270.0, 1.0, 0.0, 0.0)
        glutSolidCone(1.0, 2.0, 15, 15)
        glPopMatrix()

        glPushMatrix()
        glTranslatef(0.75, 0.0, -1.0)
        glutSolidSphere(1.0, 15, 15)
        glPopMatrix()

        glPopMatrix()
        glFlush()

        return True  # Keep the idle callbacke running


def main(demoapp=None):
    App(demoapp)
    Gtk.main()


if __name__ == '__main__':
    main()
