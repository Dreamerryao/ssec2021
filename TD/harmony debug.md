本文档旨在简单介绍一下使用 OpenHarmony 模拟器针对目标用户态程序的调试方法，为后续的题目调试做一定的准备

调试的对象是之前release的harmony example (已更新版本

## 应用程序源代码

给出了简单的源代码，逻辑就是输入和输出

```c
#include <stdio.h>

int main(int argc, char* argv[])
{
	char name[16];
	printf("Input your name: ");
	scanf("%s", name);

	printf("Hello World! %s!", name);

	return 0;
}
```

## 直接运行

我们使用给定的脚本直接运行鸿蒙系统: `./start_qemu.sh`

会发现系统运行起来后有这样的输出

![直接运行](https://images.gitee.com/uploads/images/2021/0622/194343_0f827c31_5739219.png "屏幕截图.png")

我们试着输入后，也可以看到预期的回显

![直接输入](https://images.gitee.com/uploads/images/2021/0622/194656_829e95f3_5739219.png "屏幕截图.png")

之后系统便挂起了，我们通过 CTRL + A + C 的方式退出 QEMU 后，弄个恶意输入试一试

![溢出输入](https://images.gitee.com/uploads/images/2021/0622/194823_755256d5_5739219.png "屏幕截图.png")

可以看到，程序整个 crash 掉了，而且鸿蒙系统还输出了一系列的错误信息


## 分析和修改 init.cfg

在开始调试分析前最关键的是理清楚逻辑关系: 我们的题目是一个用户态的程序，QEMU跑起来的是整个harmony1.0的系统，用户态程序自然是依托在系统上的。按理来说，跑起来一个系统后应该是给用户一个可以交互的命令台的（当然，就是shell了）。而这里的程序却没有给shell而是直接运行了目标的用户态程序，这显然是出题人安排的。我们首先提取 rootfs 的文件并分析

```
$ jefferson rootfs.img -d outdir
```

提取的文件在 `outdir/fs_1` 下，而其中最关键的文件就是启动配置文件 `outdir/fs_1/etc/init.cfg`，通过读取该文件，我们发现两处重要的地方

首先是 service 的配置，如下代码内容配置了名为 `camera_app` 的服务

```
...
        }, {
	    "name" : "camera_app",
	    "path" : "/bin/camera_app",
            "uid" : 1000,
	    "gid" : 1000,
            "once": 1,
            "importance" : 0,
            "caps" : [0, 1]
	}
...
```

而如下的代码内容则是指定了系统运行时启动哪些服务

```
        }, {
            "name" : "init",
            "cmds" : [
                "start apphilogcat",
                "start foundation",
                "start bundle_daemon",
                "start media_server",
                "start appspawn",
		"start camera_app"
            ]
        }, {
```

我们发现这里的启动配置跑起来了目标程序: `start camera_app` 而没有运行命令台程序，那我们把它改一下

```
        }, {
            "name" : "init",
            "cmds" : [
                "start apphilogcat",
                "start foundation",
                "start bundle_daemon",
                "start media_server",
                "start appspawn",
		"start shell"
            ]
        }, {
```
> 注意这里`start shell`是因为看到后面有以`shell`命名的service

然后我们重新打包生成文件系统，并替换旧的 rootfs，这样启动后我们就能拿shell了

```
mkfs.jffs2 -d ./fs_1/ -o rootfs.img
```

再次启动可以看到有交互台

![交互台](https://images.gitee.com/uploads/images/2021/0622/195949_97a8ec76_5739219.png "屏幕截图.png")

而我们现在可以主动在交互台中，启动目标用户态程序了

![命令行启动](https://images.gitee.com/uploads/images/2021/0622/200045_8cd09e6b_5739219.png "屏幕截图.png")

## 分析内存布局

当然，单纯拿shell并不是目的，我们的目的是能用调试器去分析这个目标程序。之前的x86程序我们能在自己的主机上直接启动并挂起gdb进行调试，但这次的目标程序是运行在鸿蒙系统之上的，而现在运行起来的系统是没有gdb的，那么该怎么做呢？

实际上比较一般的做法是交叉编译一个调试器也塞进rootfs.img中，但出于简单考虑（而且现有模拟器支持的内存限制），这么做自然不是首选。

那么另外的做法就是系统级别调试，直接用QEMU支持的调试功能调试整个运行起来的鸿蒙系统，只要给定正确的断点。系统在调度并执行目标用户态程序时，调试器也可以断住，并给出我们想要的寄存器信息和内存信息。

所以问题落在了如何找到正确的位置给定断点：目标程序会被加载到什么内存位置呢？

不同于x86上的运行，鸿蒙平台加载一个用户态程序的方式颇有不同，如果多尝试一下之前overflow时你可以发现其返回的报错信息中的内存位置多有不一样，显然是有很大的随机性的。那么我们就必须要在目标程序运行起来后通过特定的命令来查看内存布局。

我们在原有的shell上启动目标程序并打开网络命令`telnet`

![输入图片说明](https://images.gitee.com/uploads/images/2021/0622/200717_7bf9a190_5739219.png "屏幕截图.png")

此时目标程序等待输入已经挂起了，我们在另外一处通过`telnet`命令连接上鸿蒙
> 默认IP地址是 192.168.1.10

![输入图片说明](https://images.gitee.com/uploads/images/2021/0622/200804_32ce7f51_5739219.png "屏幕截图.png")

然后在这个新拿到的shell上通过`vmm`命令就可以访问内存布局了，如下图，成功返回了目标程序的加载地址以及相关libc的加载地址

![输入图片说明](https://images.gitee.com/uploads/images/2021/0622/200853_08abd5e0_5739219.png "屏幕截图.png")

## 调试与总结

既然知道如何获取目标程序的加载地址了，那么调试的过程就比较清晰了，这里简单总结流程如下

1. 修改文件系统`init.cfg`来获取shell
2. 通过QEMU的调试方式启动，可以参考`debug.sh`
3. 进入shell后开启`telnet`并运行目标程序直至挂起
4. 通过telnet访问鸿蒙，运行`vmm`看挂起的目标程序的内存布局
5. 此时在host机器上通过gdb remote连接目标系统，通过偏移计算在想要调试的位置下断点即可
