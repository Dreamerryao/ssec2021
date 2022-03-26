#!/bin/sh

flash="./flash"
rootfs="./rootfs.img"
liteos="./liteos.bin"
qemu="qemu-system-arm"
config="./qemu-ifup"


if [ ! -f "$flash" ];then
    echo "There is no flash here"
    exit
fi

if [ ! -f "$rootfs" ];then
    echo "There is no rootfs.img here"
    exit
fi

if [ ! -f "$liteos" ];then
    echo "There is no liteos.bin here"
    exit
fi

if [ ! -f "$config" ];then
    echo "There is no qemu-ifup here"
    exit
fi

if [ ! -f "$qemu" ];then
    echo "There is no qemu-system-arm here"
    exit
fi

if [ ! -x "$config" ];then
    sudo chmod 777 $config
fi

if [ ! -x "$qemu" ];then
    sudo chmod 777 $qemu
fi

sudo ./qemu-ifup
sudo ./qemu-system-arm -M hi3518 -kernel liteos.bin -nographic -net nic,vlan=0 -net tap,vlan=0,ifname=tap100 
