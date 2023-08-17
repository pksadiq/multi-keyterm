<div align="center">
  <a href="https://www.sadiqpk.org/projects/multi-keyterm.html">
    <img src="https://gitlab.com/sadiq/multi-keyterm/raw/main/data/icons/hicolor/256x256/apps/org.sadiqpk.multi-keyterm.png" width="150" />
  </a>
  <br>

  <a href="https://www.sadiqpk.org/projects/multi-keyterm.html"><b>Multi Key Term</b></a>
  <br>

    Map different keyboards to different pseudo terminals
  <br>

  <a href="https://gitlab.com/sadiq/multi-keyterm/pipelines"><img
       src="https://gitlab.com/sadiq/multi-keyterm/badges/gtk3/pipeline.svg" /></a>
  <a href="https://sadiq.gitlab.io/multi-keyterm/coverage"><img
       src="https://gitlab.com/sadiq/multi-keyterm/badges/gtk3/coverage.svg" /></a>
</div>

---

Multi Key Term is a terminal emulator that helps you create one
to one maping between several keyboards and terminal sessions.
This can help multiple users use the same system with only
additional keyboards required per user.  The display will be
split to create multiple terminal sessions each associated
with different keyboard.

If you find this software useful, you may:
* donate me via [librepay][librepay].
* Help [free software][free-software] by switching to any [GNU/Linux][gnu-linux].
* Regardless of the Operating System you use, always buy
  Debian main compatible hardware or at least ask for one.
* This can help free software, improve hardware lifetime
  and reduce [Planned obsolescence][soft-lockout].

Source Repository: [GitLab][gitlab], [GitHub][github]

Issues and Feature Requests: [GitLab][issues]

Please visit the [website][home] for more details.

## Building Multi Key Term

Install required dependencies.

On Debian and derivatives (like Ubuntu):
* `sudo apt install build-essential meson libgtk-4-dev xsltproc \
  appstream-util docbook-xsl libvte-2.91-dev libinput-dev \
  libxkbcommon-dev libudev-dev gettext appstream`libadwaita-1-dev

On Fedora and derivatives:
* `sudo dnf install @c-development @development-tools vte291-devel \
  gettext-devel gtk4-devel meson desktop-file-utils libinput-devel \
  libxkbcommon-devel systemd-devel docbook-style-xsl libxslt`

`meson` is used as the build system.  Run the following to build:
* `meson build --prefix=/usr`
* `cd build && ninja`
* `sudo ninja install # To install`

Here `build` is a non-existing directory name.  You may use any
name of your choice.

## Running Multi Key Term

If you have installed application:
* `sudo multi-keyterm`

If you want to run the application without installing:
* do `sudo ./run` from the build directory

You can add `-v` multiple times to get verbose logs:
* `sudo ./run -vvvv`

## Known Issues

* Works only as root
   - It’s better to not run GUI Applications as root.
     Help welcome if you can fix this, eg.: by using polkit.
   - Gtk/mutter seems to create one logical keyboard device and
     map all keyboard events to the one created.  Also, wayland
     doesn’t seems require one to one maping of keyboard devices
     to `wl_keyboard`.  So we can’t rely on them and we directly
     access input devices with libinput.
   - You shall be able to avoid this by adding yourself to `input`
     group.  But it may be better to allow one application to run
     as root than to allow every application to have access to all
     input devices.
* Won't stop users from pressing system wide keys
   - Which means that any one user can disrupt others work by key
     combinations like `Alt + Tab`, `Control + Alt + F3` or
     `Alt + Sysrq` keys.
* All keystrokes shall be processed as long as the window has focus state
   - Which means that if you are able to switch window in way
     that won’t effect the window focus state (as per what GTK reports)
     the keys will then be processed both in the focused window and
     Multi Key Term.  Eg.: pressing `Control + Alt + F3` when
     you have focus on Multi Key term may result in the following
     keys pressed to be processed by both tty and Multi Key Term.
     Or say, When you do lock screen and unlock, the password you
     type in unlock screen *may* be shown in Multi Key Term if the
     window was in focus when screen was locked.
* Slow downs the system when used in GNOME Shell with X11 backend
   - Likely this is an issue in mutter x11 backend
   - Use GNOME shell wayland
* keyboard input is limitted to US pc105
* The LEDs on keyboards may be in a limbo state when the app is closed
   - Since the LED states of keyboards are individually updated, each
     keyboard may have different LED states.  This can be fixed by
     pressing the corresponding keys that have inconsistent LED states
     after the application is closed (so that your system shall update
     the LED states right)


## License

Multi Key Term is licensed under the GNU GPLv3+. See COPYING for license.
This application is built on top of [My GTemplate][my-gtemplate] which 
is available as public domain.

The icons in `data/hicolor` are adapted from [GNOME Terminal][gnome-terminal]

<!-- Links referenced elsewhere -->
[librepay]: https://liberapay.com/sadiq/donate
[free-software]: https://www.gnu.org/philosophy/free-sw.en.html
[gnu-linux]: https://getgnulinux.org
[soft-lockout]: https://en.wikipedia.org/wiki/Planned_obsolescence#Software_lock-out

[home]: https://www.sadiqpk.org/projects/multi-keyterm.html
[coverage]: https://sadiq.gitlab.io/multi-keyterm/coverage
[gitlab]: https://gitlab.com/sadiq/multi-keyterm
[github]: https://github.com/pksadiq/multi-keyterm
[issues]: https://gitlab.com/sadiq/multi-keyterm/issues

[purism]: https://puri.sm
[librem5]: https://puri.sm/products/librem-5

[my-gtemplate]: https://www.sadiqpk.org/projects/my-gtemplate.html
[gnome-terminal]: https://gitlab.gnome.org/GNOME/gnome-terminal
