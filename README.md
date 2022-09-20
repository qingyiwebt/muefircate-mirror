# biᴇꜰɪrcate

_very experimental_ • _some [developer notes](NOTES.asciidoc) available_

 1. &nbsp;`sudo apt-get install gcc-mingw-w64-x86-64 gcc-multilib`
 2. &nbsp;`sudo apt-get install dosfstools mtools`
 3. &nbsp;`sudo apt-get install qemu-system-x86 qemu-utils ovmf`
 4. &nbsp;`./configure`
 5. &nbsp;`make -j4`
 6. &nbsp;`make run-qemu`

This aims to run x86-16 or x86-32 code from an x86-64 UEFI environment.

Again, some [developer notes](NOTES.asciidoc) are available.
