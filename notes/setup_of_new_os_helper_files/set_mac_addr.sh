#!/bin/sh
cpuSerialNum() {
#output first 5 bytes of CPU Serial number in hex with a space between each
#nvmem on RK3308 does not handle multiple simultaneous readers :-(
  nvmem=/sys/bus/nvmem/devices/rockchip-otp0/nvmem
  serNumOffset=20
  /bin/flock -w2 $nvmem /bin/od -An -vtx1 -j $serNumOffset -N 5 $nvmem
}

Id=`cpuSerialNum` || exit #fail if Rockchip nvmem not available
/sbin/ip link set dev eth0 address 06:`echo $Id | tr ' ' :`