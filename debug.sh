#!/bin/bash

make debug &
gdb -ex "target remote localhost:1234" -ex "symbol-file kernel/kernel"
