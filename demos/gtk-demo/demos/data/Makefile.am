RESOURCES =             \
    css_accordion.css   \
    reset.css           \
    css_basics.css      \
    css_multiplebgs.css \
    brick.png           \
    brick2.png          \
    cssview.css

demo.gresource: demo.gresource.xml $(RESOURCES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) $(srcdir)/demo.gresource.xml

dist_pkgdata_DATA =                \
    alphatest.png           \
    apple-red.png           \
    background.jpg          \
    demo.ui                 \
    floppybuddy.gif         \
    gnome-applets.png       \
    gnome-calendar.png      \
    gnome-foot.png          \
    gnome-fs-directory.png  \
    gnome-fs-regular.png    \
    gnome-gimp.png          \
    gnome-gmush.png         \
    gnome-gsame.png         \
    gnu-keys.png            \
    gtk-logo-rgb.gif        \
    demo.gresource.xml      \
    demo.gresource          \
    $(RESOURCES)
