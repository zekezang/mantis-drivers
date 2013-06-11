import sys
import gdb

# Update module path.
dir = '/home/build/bluez-20120928/build-out/share/glib-2.0/gdb'
if not dir in sys.path:
    sys.path.insert(0, dir)

from gobject import register
register (gdb.current_objfile ())
