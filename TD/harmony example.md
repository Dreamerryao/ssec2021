本 doc 旨在介绍如何利用 openHarmony 的模拟器运行一个 helloworld 程序以及如何获取到目标的 binary 进行分析

## 依赖

对于18.04的虚拟机环境需要如下依赖
```
sudo apt install libvte-dev qemu-utils ipxe-qemu libvdeplug-dev libfdt-dev vim git gettext g++ autoconf automake libtool libaio1 libasound2 libbluetooth3 libc6 libcacard0 libfdt1 libgcc1 libglib2.0-0 libgnutls30 libjpeg8 libncurses5 libnuma1 libpixman-1-0 libpulse0 libsasl2-2 libsdl1.2debian libseccomp2 libspice-server1 libstdc++6 libtinfo5 libusb-1.0-0 libusbredirparser1 libuuid1 libx11-6 libxenstore3.0 zlib1g qemu-system-common libvdeplug2 bridge-utils uml-utilities

sudo apt install libvdeplug2 

wget -q -O /tmp/libpng12.deb http://mirrors.kernel.org/ubuntu/pool/main/libp/libpng/libpng12-0_1.2.54-1ubuntu1_amd64.deb
sudo dpkg -i /tmp/libpng12.deb
```

## 运行
文件说明以及启动脚本在 `harmony-example` 目录

类似启动成功的效果为

```
$ ./start_qemu.sh

(process:8477): GLib-WARNING **: 13:13:57.120: ../../../../glib/gmem.c:489: custom memory allocation vtable not supported
W: /etc/qemu-ifup: no bridge for guest interface found
   ____  _                   _            ____            _          _
  / ___|(_)_ __   __ _ _   _| | __ _ _ __/ ___|  ___  ___| |    __ _| |__
  \___ \| | '_ \ / _` | | | | |/ _` | '__\___ \ / _ \/ __| |   / _` | '_ \
   ___) | | | | | (_| | |_| | | (_| | |   ___) |  __/ (__| |__| (_| | |_) |
  |____/|_|_| |_|\__, |\__,_|_|\__,_|_|  |____/ \___|\___|_____\__,_|_.__/
                 |___/
						twitter @SingularSecLab
******************Welcome******************
Processor   : unknown
Run Mode    : UP
GIC Rev     : unknown
build time  : Oct 29 2020 15:00:51
Kernel      : Huawei LiteOS 2.0.0.35/debug

*******************************************

...

[Init] start service camera_app succeed, pid 8.

************************************************

	Hello HarmonyOS from SSEC2021 :)

************************************************

[Init] SigHandler, SIGCHLD received.
[Init] stop service foundation, pid 4.
[Init] stop service appspawn, pid 7.
[Init] stop service bundle_daemon, pid 5.
[Init] SigHandler, SIGCHLD received.
[Init] DoChown, failed for 0 99 /dev/hdfwifi, err 2.
[Init] main, entering wait.
```

可以看到，模拟器跑起来后最后输出了一个 helloworld


## 文件系统提取

参考 [README](https://gitee.com/zjuicsr/ssec21spring-stu/blob/master/harmony-example/README.md) 完成额外工具安装，然后可以用 jefferson 去提取出 rootfs.img 文件系统内容

```
jefferson rootfs.img -d outdir
```

这里会生成新的 outdir 的文件目录，上面最后运行到的 hello world 目标代码位于

```
./outdir/fs_1/bin/camera_app
```

查看文件类型为
```
file camera_app
camera_app: ELF 32-bit LSB shared object, ARM, EABI5 version 1 (SYSV), dynamically linked, interpreter /lib/ld-musl-arm.so.1, stripped
```

因为是 arm32 的binary，其代码可以通过 `arm-none-eabi-objdump` 工具分析，交叉工具链下载安装可以参考该[博客](https://blog.csdn.net/xingqingly/article/details/78112125)

> 注: 目录下已经给出的 camera_app 是包含 debug 符号的版本