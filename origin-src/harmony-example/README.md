# README

环境原型: 奇点实验室openHarmony模拟器

关于文件的说明:
* `qemu-system-arm` qemu模拟器的主程序
* `qemu-ifup`和`qemu-ifdown`是qemu用来在宿主机上配置网卡的脚本文件 (可以不用
* `liteos.bin` 是 OpenHarmony 的 debug 版本的内核镜像
* `rootfs.img` 是 OpenHarmony 的 debug 版本的文件系统镜像
* `flash` 是模拟OpenHarmony的flash外设
* `start_qemu.sh` 一键运行脚本
* `camera_app` 带调试符号的最后运行的hello world binary

需要的额外工具:
1. jefferson
用于提取rootfs.img文件内容
link: https://github.com/sviehb/jefferson
安装方法参考其 readme 即可

2. mkfs.jffs2
安装方法: sudo apt-get install mtd-utils


