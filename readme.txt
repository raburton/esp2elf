esp2elf
-------
This tool allows you to convert an esp8266 ROM into an elf file, which you can
then easily load in IDA/Ghidra. It is not the first tool to do this, but I was
recently asked to help extract a ROM from a dump where rBoot was being used for
OTA updates, and the python based tool didn't support this. It was also quite
difficult to install its dependencies with recent versions of python. I needed
a little project so whipped this up in C. It should support regular single
application ROMs as well as SDK and rBoot based OTA ROMs.

Note: There is very little error checking in the code, but if you're able to
reverse engineer a ROM, you'll be able to fix any glitches that occur here.

Building
--------
Makefile included.
Linux: Requires xdd and libelf dev package (apt-get install libelf-dev xxd).
Windows: Tested with MSYS2, requires packages: make, mingw-w64-ucrt-x86_64-gcc,
         and mingw-w64-x86_64-libelf. Also need a windows version of xxd.

Usage
-----

usage: esp2elf <romfile> <elffile> [offset] [iromoff] [iromlen]
- binfile - input esp8266 rom dump
- elffile - output elf file
- offset  - offset into rom to start from, defaults to 0
- iromoff - irom start offset (non-ota only, defaults to 0x40000)
- iromlen - irom length (non-ota only, defaults to 0x3e000)

Examples
-------- 
standard single application rom:
  esp2elf <rom> <elf> 
 
single rom with an example non-standard irom (see note 1):
  esp2elf <rom> <elf> 0 0x20000 0x5e000 
 
sdk ota rom, first slot (see note 2):
  esp2elf <rom> <elf> 0x1000
 
rBoot ota rom, first slot (see note 2):
  esp2elf <rom> <elf> 0x2000
 
Notes 
----- 
1) For single rom, there is no way to know location or length of irom, just have
   to try sensible values.
2) For ota roms, need to specify offset to one of the user roms, else you'll 
   make an elf of the bootloader itself (this is fine if you want to, though the
   extracted .irom.text elf section will be junk). Default offset for first
   rBoot rom is 0x2000, but rBoot is open source so could have been customised
   with a different starting location (but this would be unusual). Later rom
   slot positions will vary according to flash size and other configuration
   options.
 
Credits 
------- 
Inspiration: https://github.com/jsandin/esp-bin2elf
bootrom.bin: https://github.com/jcmvbkbc/esp-elf-rom
Bootrom symbols: https://github.com/espressif/ESP8266_RTOS_SDK
Essential help: https://sourceforge.net/projects/elftoolchain/files/Documentation/libelf-by-example/
