cmd_/sourcecode/arm-workspace/uart-module/tiny_serial.ko := arm-linux-ld -EL -r  -T /sourcecode/arm-workspace/linux-2.6.38/scripts/module-common.lds --build-id  -o /sourcecode/arm-workspace/uart-module/tiny_serial.ko /sourcecode/arm-workspace/uart-module/tiny_serial.o /sourcecode/arm-workspace/uart-module/tiny_serial.mod.o