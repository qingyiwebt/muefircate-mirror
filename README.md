# μuᴇꜰɪrcate

_very experimental_ • _some [developer notes](doc/NOTES.asciidoc) available_

 1. &nbsp;`sudo apt-get install make gcc-mingw-w64-x86-64 lld musl-dev`
 2. &nbsp;`sudo apt-get install dosfstools mtools ccache meson ninja-build`
 3. &nbsp;`sudo apt-get install fdisk qemu-system-x86 qemu-utils zip ovmf`
 4. &nbsp;`./configure`
 5. &nbsp;`make -j4`
 6. &nbsp;`make run-macron`

This aims to run x86-16 or x86-32 code from an x86-64 UEFI environment.

Again, some [developer notes](doc/NOTES.asciidoc) are available.
