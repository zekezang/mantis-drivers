cmd_/sourcecode/arm-workspace/my-bus/my-bus.ko := arm-linux-ld -EL -r  -T /sourcecode/arm-workspace/linux-2.6.38-used/scripts/module-common.lds --build-id  -o /sourcecode/arm-workspace/my-bus/my-bus.ko /sourcecode/arm-workspace/my-bus/my-bus.o /sourcecode/arm-workspace/my-bus/my-bus.mod.o