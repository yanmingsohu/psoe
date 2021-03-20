# 可能是个 Playstation 1 模拟器

这个项目随时会因为架构错误而崩溃.

PS1 是我小时候最向往的游戏机, 我非常喜欢它. 我想要非常非常的深入这个游戏机中, 
深入它的灵魂. 当我阅读了 Nocash 文档后, 认为 ps1 的硬件是异构的, 允许 cpu
以更快的频率运行, gpu 以更快的速度填充像素, spu 不受速度改变的影响, 该项目
中每个硬件模块都在单独的线程中执行, 以验证该想法.


如果能让模拟器跑起来, 那么将验证以下假设:

* cpu 可以快一些或慢一些; 过慢的速度, 可能会对部分游戏有影响; 过快的速度通常没有影响, 甚至让优化不好的游戏变得正常.
* spu 绝对不会卡顿, 理论上也不受 cpu 速度的影响, 有可能会让音乐的节奏变快, 但音高不应该改变.
* gpu 如果宿主机速度慢, 在渲染时会缺失多边形, 否则和 ps1 的看起来差不多.
* cdrom 以接近宿主机 IO 的最快速度读取数据, 这可能会让游戏的加载界面快速略过.
* dma 接近宿主机 IO 的最快速度, 这会减低很多游戏的卡顿.
* 计时器必须接近原始硬件, 不论 pc 有多快, 以保证音乐有正确节拍.


# 对本项目有帮助的资料和网站

* [MIPS Assembler Online](http://www.kurtm.net/mipsasm/index.cgi)
* [MIPS Interpreter Online](https://dannyqiu.me/mips-interpreter/)
* [MIPS opcodes](https://opencores.org/projects/plasma/opcodes)
* [x86 Assembler/Disassembler Online](https://defuse.ca/online-x86-assembler.htm#disassembly)
* [x86 opcodes](http://www.mathemainzel.info/files/x86asmref.html)
* [PSX document 1](http://hitmen.c02.at/html/psx_docs.html)
* [PSX document 2](https://github.com/simias/psx-guide)
* [PSX Test](https://github.com/simias/psx-hardware-tests)
* [OpenGL Tutorial](http://www.opengl-tutorial.org/)
* [GUN libcdio](https://www.gnu.org/software/libcdio/libcdio.html)
* [MIPS lwl/lwr](https://stackoverflow.com/questions/57522055/what-do-the-mips-load-word-left-lwl-and-load-word-right-lwr-instructions-do)


# 依赖的项目

* [Sound ADSR](https://github.com/kylophone/libADSR) ?
* [Sound API](https://github.com/thestk/rtaudio)
* [Sound resampler](https://github.com/avaneev/r8brain-free-src)
* [Graphics Mathematics](http://eigen.tuxfamily.org/index.php?title=Main_Page)
* [Sound API](https://github.com/jarikomppa/soloud) %
* [OpenGL GUI](https://github.com/wjakob/nanogui) %

  ? 之后会移除
  % 没有安装