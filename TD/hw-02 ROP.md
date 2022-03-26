# 知识点

## Section I ret2Shellcode

> In hacking, a **shellcode** is a small piece of code used as the payload in the exploitation of a software vulnerability. 

shellcode是一个二进制代码片段，由于这个代码片段的功能经常是打开一个shell，因此称为shellcode。

当我们劫持了控制流，在绝大多数情况下，程序并不会贴心的为我们提供后门，我们需要自己决定跳转的目的地在哪里，第一个目的地就是shellcode。如果一个程序没有后门，我们就创造一个。

shellcode的攻击逻辑：将一个代码片段以data的形式，喂给程序，通常而言这个数据会被保存在栈(stack)/堆(heap)上。利用漏洞劫持控制流之后，跳转到存储shellcode的位置，即可以完成攻击。只要shellcode的功能质量足够，shellcode攻击就可以成功。

shellcode是最经典、常见的攻击方式之一。因此安全界也提出了相应的解决方案，在linux中，这种保护措施称为**NX**(No-eXecute)，“不可执行”，即一段拥有“写权限”的内存块，不可以同时拥有“执行权限”，一旦pc指针指向了受NX保护的内存块，CPU就会抛出段错误(segmentation fault)。由此避免了恶意代码的执行，达到保护目的。

新版本的gcc会默认开启NX保护，可以通过`-z execstack`来关闭NX功能。

善用pwntools的checksec功能，能够更好的帮助我们选择攻击方式：
```sh
  cat test1.c
int main(void)
{
	char buffer[256];
	gets(buffer);
}

  gcc -o test1 test1.c -fno-stack-protector -fno-pie -no-pie -z execstack
    ...
  checksec test1
[*] '/home/zjussec/zjusec/learning/test1'
    Arch:     amd64-64-little
    RELRO:    Partial RELRO
    Stack:    No canary found
    NX:       NX disabled               <--?!
    PIE:      No PIE (0x400000)         
    RWX:      Has RWX segments
  

```

此外，也可以通过gdb中的vmmap/libs（不同的gdb插件指令也有不同）功能，或者`cat /proc/$YOUR_PID/maps`来查看进程中不同段的rwx权限分配。

在windows下，类似的保护措施称为**DEP**(Data Execution Prevention)，数据执行保护。

当我们成功使pc指针直接指向了注入的shellcode，那么废话的说，接下来程序就要执行我们的恶意代码了，此时就要考虑恶意代码的质量与功能了。至少shellcode应该与被攻击服务器相兼容。需要考虑的是目标服务器的架构、系统等。

[shellcode-storm](http://shell-storm.org/shellcode/)这个网站，涵盖了大量平台、大量功能的shellcode，但是其中的部分shellcode并不一定是完全正确的，往年的实验中也会发现同学选择的部分shellcode本身有一定的问题，因此需要仔细甄别后使用。

信不过这种现成的，也可以自己动手编。以下为使用pwntools进行shellcode编写的demo：
```py
>>> from pwn import *
>>> context(arch = 'i386', os = 'linux')
>>> asm('xor eax, eax')
b'1\xc0'
>>> 
```
其中`context()`是为了明确汇编语言的标准，不同环境的机器码会有所不同。输出的`'1\xc0'`就是指令对应的机器码了。

pwntools作为pwn选手强大的工具之一，生成一些指定属性的shellcode当然也不在话下，感兴趣的可以到pwntools的官方doc去进一步学习。[pwntools](http://docs.pwntools.com/en/stable/)

## Section II Ret2libc/ROP
>  the C standard library ("libc") is commonly used to provide a standard runtime environment for programs written in the C programming language. 

程序调用库函数，之所以叫库函数就是因为这个函数在库里...
我们平时调用的`puts()`、`printf()`都是库函数，不需要我们手动编写，而是在`include`了相应的头文件后，直接调用就好了，函数的具体实现，由libc完成。在我们打开一个程序时，装载器会在本地帮程序找到一个适合的libc库，然后将**整个libc**同程序一起加载到内存中。
我们可以通过`ldd`命令查看一个程序所会加载的libc。
```sh
  ldd test1
	linux-vdso.so.1 (0x00007ffdf94a2000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007fdbbf3ee000)
	/lib64/ld-linux-x86-64.so.2 (0x00007fdbbf7df000)

```
我们可以使用`readelf`来查看libc中的一些信息，其中`-s`代表symbol信息：
```
  readelf -s /lib/x86_64-linux-gnu/libc-2.27.so | grep ' gets@'
    89: 00000000000800b0   432 FUNC    WEAK   DEFAULT   13 gets@@GLIBC_2.2.5
  readelf -s /lib/x86_64-linux-gnu/libc-2.27.so | grep ' system@'
  1403: 000000000004f440    45 FUNC    WEAK   DEFAULT   13 system@@GLIBC_2.2.5
  
```
这里`readelf`的对象为`libc-2.27.so`是因为之前ldd显示的`libc.so.6`是一个指向`libc-2.27.so`的符号链接。

我们可以看到，在libc中有很多函数，因此只需要让pc指针跳转到相应的函数就行了。在最常见的情况下，我们想让进程执行`system("/bin/sh");`因此我们只要利用bof漏洞，覆盖返回地址为`system()`函数的地址，我们的进程就可以去执行`system()`了，但仅仅如此，是不够的。我们需要为system函数提供参数即字符串`“/bin/sh”`，并使用获得的这些信息，构造一条ropchain。

ROP(*Return-oriented programming*)，返回导向型编程，如果bof漏洞中我们可以溢出的字节数足够多，rop攻击是图灵完备的。

ROP的攻击逻辑：当一个函数完成执行，函数将执行`ret`，相当于`pop $PC`，即从栈中pop出一个字长的值，赋给PC指针。即我们利用bof漏洞时，我们不单可以覆盖ret_addr为func_A()的地址，还可以控制funcA()的返回地址，例如指向func_B()，如此往复。

通过不断的伪造栈，使进程能够从func_A跳到B再跳到C，或者跳回到A……最后如此形成的payload，我们称为rop链条。

形如`pop eax; ret`的代码片段，称为gadget，可以将指向其的指针作为rop链条的一部分，像这个gadget的作用就是控制eax为栈中的内容，然后再ret。

可以使用`ROPgadget`工具搜索相关的gadget。感兴趣的同学可以自己探索一下。

这里以32bit为例，当进程执行到`func(int a, int b, int c)`函数内部。(尚未push bp时)栈：
```
            /------------------\  lower      
      sp->  |     func_ret     | 
            |------------------|
            |        a         |  
      ↑     |------------------|
    stack   |        b         |      
    grows   |------------------|
            |        c         |
            |------------------| 
            |       ...        | 
            \------------------/  higher
              Fig. 1 template  
```
为了成功执行system("/bin/sh"),覆盖布局：
```
            /------------------\  lower      
      sp->  |     ret_addr     |  <-- system_addr
            |------------------|
            |                  |  <-- system_ret = 0xdeadbeef 这个值随便的
      ↑     |------------------|
    stack   |                  |  <-- binsh_addr  (points to "/bin/sh")
    grows   |------------------| 
            |                  | 
            \------------------/  higher
            Fig. 2 stack overflow   
```
按照Fig.2，我们第一次返回到`system_addr`，那么sp下移一格，指向`system("/bin/sh")`函数的返回地址。由于我们的目标是执行`system("/bin/sh");`fork出子进程`"/bin/sh"`，所以父进程的返回值无关紧要，随便设置为0xdeedbeef。再往下的`binsh_addr`就是system的唯一参数。

当ropchain得到执行，我们就会获得一个子进程`"/bin/sh"`。如果是在本地实验的，你关闭当前shell之后还会报一个segmentation fault，可以思考一下为啥。

**64bit**下，ret2libc、rop的原理和32bit都是一样的。唯一不同的是，64bit下，函数的参数传递不再依靠栈，而是寄存器。
同样以`func(int a, int b, int c)`为例(尚未push bp时)：
```
            /------------------\  lower      
      sp->  |     func_ret     | 
            |------------------|     
            |       ...        | 
            \------------------/  higher
             Fig. 3 64bit  
```
此时栈中仅仅保存func_ret，不过此时`rdi=a, rsi=b, rdx=c`。

在64bit下，参数小于等于6个时，依次使用以下寄存器传递参数：
```
rdi     rsi     rdx     rcx     r8     r9
```
当大于6个参数时，例如共9个参数，`func(... arg7, arg8, arg9)`:
```
            /------------------\  lower      
      sp->  |     func_ret     | 
            |------------------|
            |       arg7       |  
      ↑     |------------------|
    stack   |       arg8       |      
    grows   |------------------|
            |       arg9       |
            |------------------| 
            |       ...        | 
            \------------------/  higher
              Fig. 4 64bit  
```
此时寄存器对应参数：
```
rdi     rsi     rdx     rcx     r8      r9
arg1    arg2    arg3    arg4    arg5    arg6
```

**WARNING**：在64bit下，由于glibc-2.27版本中的库函数使用了sse指令，可能会遇到即使寄存器参数正确、成功进入相应函数，也会报段错误的问题。其原因是进入函数时的栈的对齐问题。新的sse指令要求操作数16字节对齐，因此可以考虑在你的rop链中增加一个指向`ret`的gadget，多跳一次，使sp指针对齐，就可以避免段错误，成功获取shell。

## Section III BROP

在现实世界中，绝大多数情况下，作为攻击者的我们，往往是无法获得远程服务器上运行的程序的binary以及source code的，但是即便如此，只要远程的程序拥有漏洞，我们依然可以通过一定的技巧，来完成攻击。

BROP就是专门用于这种攻击环境的技术，即Blind ROP，指的就是在没有binary，没有source code的情况下，完成对于一个远程程序的攻击。

BROP的论文：[Hacking blind](http://www.scs.stanford.edu/brop/bittau-brop.pdf)
BROP的slide：[Hacking blind slide](http://www.scs.stanford.edu/brop/bittau-brop-slides.pdf)

要想实现BROP攻击，也有一些必要的条件：
#### 攻击条件：
- 存在栈溢出漏洞
- 服务器端的程序在崩溃之后，使用`fork()`对程序进行重启，而不是`execve()`
> 主流的服务器很多都是符合这个要求的，以socat为例，socat会首先设置一个父进程，每当获得一个链接请求时，并不使用这个父进程与请求进行交互，而是fork出一个子进程与请求交互，那么子进程的崩溃并不会影响父进程，下一次请求来临时，依然可以fork出一个子进程与请求交互。具体`fork()`与`execve()`的区别，还请同学们自行探究。

#### 攻击流程
基本分为三大环节：
- stack reading
- Blind ROP
- Build Exp with binary

即第一步泄漏栈信息，劫持控制流；第二步，使用BROP搜集程序的信息，然后泄漏整个binary；第三步，使用之前搜集的信息，结合泄漏的binary，获取足够的信息完成一次普通的ROP攻击。

##### stack reading
**崩溃点判断**
- 爆破，每次重启跑的程序都是一样，那么第一次喂1个字节，查看程序是否崩溃，然后喂2个字节...如此得到程序崩溃的位置，这个位置通常是canary，或者是返回地址。

**canary处理**

canary是用于防止栈溢出攻击的，具体不作过多介绍，同学们自行了解。需要注意的是，canary（32bit下）一定是`0xXXXXXX00`，即小端格式下的第一个字节一定是`\x00`，不惜牺牲8个bit的空间，也要将其设置为固定死的`\x00`的原因，还请同学们自行思考。可以联系`\x00`这个字符在程序中意味着什么。

一旦程序发现canary被修改了，就会立即停止程序，抛出错误。目前canary已经是gcc的默认选项了，如果想要关闭这一机制，可以使用参数：`-fno-stack-protector`

还有要注意的，就是同一个程序的canary是共享的。

![img](https://wiki.x10sec.org/pwn/linux/stackoverflow/figure/stack_reading.png)

**leak canary**可以采用单字节的匹配爆破，32bit下，canary共4个字节，每个字节有256种可能，且最后一个byte固定为`\x00`，则至多需要768次尝试，以下为细节：
- 假设程序在输入100个字节时可以正常运行，101个字节时崩溃，视为遇到了canary。
- 依然输入第101个字节，但是第101字节要0~256遍历，直到程序不崩溃，认为已经单字节与canary匹配
- 如此重复，直到4个字节都匹配完成。
- 获取了canary的信息！

##### Blind ROP：
- **目的**：使用`write()`泄漏binary到socket，之后就可以使用传统的ROP攻击了

问题就在于怎么找到`write()`函数了，BROP所提供的方法，依然是爆破，爆破程序的`.text`段，这里简单概括一下攻击的流程，如果想要具体了解，可以在仔细阅读paper或者网上自己找资料。
- 将返回地址设置为代码段的某地址，然后通过返回的信息判断当前程序的运行状况，一般而言首先需要寻找的是**stop** gadget，然后再利用**stop** gadget可以判断出**trap** gadget，然后使用这两种gadget可以好好探索addr = **Probe**处的程序的具体情况，在64bit下可以找到BROP gadget，是一个用于辅助参数传递的绝妙gadget（此处略去大量细节）
- 在爆破过程中，plt表位置的爆破得到的返回信息会呈现出一种特定的规律，据此我们可以确定plt表的位置；
- 再对plt表中的所有未知功能的函数进行逐一测试，即使用stack，传递参数为`arg1 = 1, arg2 = 0x8048000, arg3 = 0x1000`
- 如果这个函数是write@plt，那么就会执行`write(1, 0x8048000, 0x1000)`，打印从0x8048000开始的0x1000个字节
> 需要注意的是，0x8048000是32bit下程序的默认加载位置（没有PIE的情况下），此处会存着“\x7fELF”的信息，与ELF的文件头相对应。因此我们可以通过write执行后的打印信息是否以“\x7fELF”开头，来判断是否找到了`write()`函数，0x1000是一般的小程序默认的代码段空间。

将write打印的`.text`部分的内容存储，并且使用反编译工具，查看其中的内容，使用binary模式打开程序，搜索其中的字符串，查看相关代码，就能获得很多有用的信息。

##### Build Exp with binary

使用获得的binary中的信息，构建一个普通的ROP攻击。

---
# 作业

DDL: 4月21日 23:59

## Challenge I ret2Shellcode 64bit（30分）
提供一个64bit binary程序，及其源代码、Makefile，你可以在代码仓库找到。要求各位使用shellcode方式攻击。你可以选择自行编辑shellcode，也可以从之前提过的[shellcode-storm](http://shell-storm.org/shellcode/)网站复制。
#### Request
服务器ip地址为：**47.99.80.189**， 端口号是：**10011**
- 记录你探究shellcode攻击的过程及原理
- 要求包括对shellcode的分析，即对其中汇编代码的分析，侧重于其中syscall的实现，参数是如何传递的等
- 再寻找一段linux-i386的shellcode，并对其进行分析，着重分析异同
- 要求最后执行flag.exe时的截图


## Challenge II Ret2libc 64bit （30分）
提供一个64bit binary程序、其源代码以及该程序在server端所使用的libc。你可以在代码仓库中找到。

#### Request
服务器ip地址为：**47.99.80.189**， 端口号是：**10012**
- 记录你的解题过程，并讲清楚其中原理
- 总结一下自己使用到的工具及方法
- 要求最后执行flag.exe时的截图

## Challenge III BROP （40分）
不提供源代码和binary，但以下为一些辅助的信息：
- gcc编译命令：`gcc 03_brop.c -o 03_brop -m32 -fno-pie -no-pie`
- checksec的结果：
![enter image description here](https://images.gitee.com/uploads/images/2021/0330/153854_66cb344f_5738530.jpeg "sec.jpg")
- 然后是简单说下程序跑起来的样子：
![enter image description here](https://images.gitee.com/uploads/images/2021/0330/154536_8acc3ace_5738530.jpeg "ins.jpg")
输出内容以`[+]`开头的信息，是使用父进程打印获得的，如果是`[-]`则表示是子进程输出的。即子进程不崩溃时，则输入内容后，依然会是`[-]`开头的，而如果子进程崩溃了，则会返回到父进程，输出以`[+]`开头的内容。
 > 之所以使用用户可见的父子进程，是为了减少服务器的连接量，让同学们能够一次连接就getshell。

### 重要提示：
- 当你获取了（1)程序的崩溃点;(2)程序的canary;(3)程序的返回地址所在的相对位置【这三个都是得分点，可以自己用一样的编译选项做个demo观察一下，结构都是一样的】
**此时**需要开始扫描程序，搜集信息获取可用的gadget，以下为**强烈建议**的扫描范围：**0x80486a0~0x80489a0**，在这个范围，有许多有趣的函数，会打印奇奇怪怪的信息，使用这些信息，足以帮助你完成此次攻击。

- **强烈建议**在扫描地址时，将有用的信息都进行本地化存储，然后在本地对每个结果进行筛选。减少对于同一个地址的重复扫描。

- 每一次连接，都会有不同的canary，且由于撰写爆破脚本时，要处理好字符串的接收格式，这是需要一定的调试空间的，以确定最后如何接收服务器端的返回数据。
因此每次爆破canary后，若仅仅执行一次脚本，就结束连接，则会浪费相当的时间和计算资源，同时考虑到我们提供给了程序10分钟的连接时间，因此**建议**在调试接受数据格式时，采用交互式控制台。即在爆破获得了canary之后，打开python的交互模式，多试几次数据的接收，减少重复的连接与爆破。可以参考以下代码：
```py
import code
code.interact(local=locals())
```

#### Request
服务器ip地址为：
```
(校外通道) ip: 47.99.80.189, port: 10013
(校内通道) ip: 10.15.201.97, port: 8090
```
- 请详细记录你的攻击流程，遇到的困难，以及解决困难的方法
- 尤其记录你搜集信息的过程
- 要求最后执行flag.exe时的截图