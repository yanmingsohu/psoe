# May be Playstation 1 emulator

This project will collapse at any time due to architectural errors.

PS1 is my favorite game console when I was a kid, I like it very much. 
I want to go deep into this game console very, very deeply,
Go deep into its soul. When I read the Nocash document, 
I think the hardware of ps1 is heterogeneous, allowing cpu
Run at a faster frequency, the GPU fills the pixels at a faster speed, 
and the spu is not affected by the speed change, the project
Each hardware module is executed in a separate thread to verify the idea.


If the simulator can be run, the following assumptions will be verified:

* The cpu can be faster or slower; too slow speed may affect some games; 
  too fast speed usually has no effect, and even makes poorly optimized games normal.
* The spu will never freeze, and theoretically it is not affected by the cpu speed.
  It may make the rhythm of the music faster, but the pitch should not be changed.
* GPU If the host is slow, polygons will be missing when rendering,
  otherwise it will look similar to ps1.
* cdrom reads data at the fastest speed close to the host's IO, 
  which may quickly skip the loading interface of the game.
* dma is close to the fastest host IO speed, which will reduce the lag of many games.
* The timer must be close to the original hardware, 
  no matter how fast the pc is, to ensure that the music has the correct beat.


# Helpful website

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


# Dependents

* [Sound API](https://github.com/thestk/rtaudio)
* [Sound resampler](https://github.com/avaneev/r8brain-free-src)
* [Graphics Mathematics](http://eigen.tuxfamily.org/index.php?title=Main_Page)
* [Sound API](https://github.com/jarikomppa/soloud) %
* [OpenGL GUI](https://github.com/wjakob/nanogui) %

  ? May be removed  
  % not install