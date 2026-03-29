Welcome to Newsflash Plus Readme file


Newsflash Plus is a Usenet binary reader. If you're reading this
readme I presume you're already well on your way to using the 
software. I hope you enjoy it.

If you find any bugs or problems or just feel like you have
a great idea for a new feature dont't hesitate to contact me.

Visit these online resources to find out more.

http://www.ensisoft.com/
http://www.ensisoft.com/forum

Shutting down PC on Linux machines:

There are several ways to attempt to shutdown the PC.

   1. "/sbin/shutdown" requires super user permissions. 

   To enable this, make a new group in /etc/group something like
    "shutdown:x:123:MYUSERNAME"
   Change /sbin/shutdown group to shutdown
    "chown root:shutdown /sbin/shutdown"
   Give execution permission for group
    "chmod 750 /sbin/shutdown"
   Set sudo bit on
    "chmod u+s /sbin/shutdown"

   Now any user in shutdown group can shutdown the PC using shutdown command. Note that 
   the new groups are not effective, untill log off/log on is done. newgrp can be used to change
   into new group immediately.

   2. use gnome-session-quit (or equivalent. Consult your Distribution docs)
   This command may not be available on the older Gnome/Ubuntu installations. Also if you run KDE you won't have it. 
   KDE probably has somethig similar.

   3. Use D-BUS with dbus-send
   dbus-send --system --print-reply --dest=org.freedesktop.ConsoleKit /org/freedesktop/ConsoleKit/Manager  org.freedesktop.ConsoleKit.Manager.Stop

   4. use systemctl
   systemctl poweroff

   Hopefully one of these works for you.


Shutting down PC on Windows machines:

  Newsflash needs to be run with adminstrator privileges


NZB DTD 1.1 Metadata Support:

  Newsflash Plus now parses the <head><meta> section defined in the
  NZB 1.1 DTD specification. The following meta types are supported:

    - password   Automatically passed to unrar during extraction
    - title      Parsed and stored for future UI use
    - category   Parsed and stored for future UI use
    - tag        Parsed and stored (multiple tags supported)

  Password-protected archives embedded in NZB files with
  <meta type="password"> will now extract without manual intervention.
  This works for both manually opened NZB files and auto-downloaded
  NZB files from watched folders.


External Process Control Improvements:

  The management of external tools (par2, unrar, 7za) has been
  improved in several ways:

  Graceful Shutdown
    Process termination now sends a polite terminate signal first
    (SIGTERM / WM_CLOSE) and waits up to 3 seconds for the process
    to exit cleanly. Only if the process ignores the request is it
    forcefully killed. This lets tools clean up temporary files.

  Exit Code Awareness
    Newsflash now inspects exit codes from each external tool:

    UnRAR:
      Exit 11 = Wrong password (reported to user as such)
      Exit 3  = CRC error / corrupt archive

    7-Zip (7za):
      Exit 2 with a password set = Wrong password hint

    Par2:
      Non-zero exit code cross-checked against the stdout
      state machine for reliable success/failure determination.

  7-Zip Password Support
    Password-protected 7z/zip archives now receive the -p flag
    when a password is available from the NZB metadata.
    Previously 7za had no password support at all.

  Safe Destructors
    The par2, unrar, and 7za engine objects no longer crash
    (assert failure) if destroyed while a process is still active.
    They now stop the process gracefully during destruction.


www.ensisoft.com
