# ![Tock Bootloader](http://www.tockos.org/assets/img/tockbootloader.svg "Tockbootloader Logo")

Version of the Tock Bootloader for signpost that helps support runtime app and
kernel reprogramming.


Compiling
---------

```bash
make
```

and to flash:

```bash
tockloader flash --address 0 --jtag --arch cortex-m4 --board signpost --jtag-device ATSAM4LC8C build/signpost_bootloader.bin
```

Bootloader Operation
--------------------

The bootloader checks to see if there is an app or kernel at address 0x60000
and then copies it to the correct location in flash (0x10000 for a kernel,
0x30000 for an app).
