# May be playstation 1 emulator

I am very curious, if there is no speed limit processing on the cpu and each chip is simulated in a separate thread, what will the game result be?

This project is to test these assumptions.


# TODO list

* [ ] Run a game
* [ ] Run bios
* [x] Run soming
* [ ] Complete the core
  * [ ] CPU
    * [ ] Interpreter
      * [x] looks like working
      * [x] boot bios
    * [ ] asmjit
  * [ ] DMA
    * [x] look like working
    * [ ] Working on Thread
  * [ ] GPU
    * [x] Display soming
    * [x] Display Bios Logo
    * [x] Working on Thread
    * [ ] Use AI to enhance textures ?!
  * [ ] GTE
    * [x] working
    * [ ] test
  * [ ] SPU
    * [ ] Speak out
    * [ ] Working on Thread
    * [ ] Tone quality tuning
    * [ ] Frame rate changes cause sound to freeze
    * [ ] Customized dedicated hardware sound card
  * [ ] CD-ROM
    * [ ] load CD image
    * [ ] support load CD image in zip file
    * [ ] support load CD image in 7z file
  * [ ] Other Device
    * [ ] PIO
    * [ ] Pad
    * [ ] SIO
    * [ ] Timer
  * [ ] BIOS
    * [x] Show logo
    * [ ] PS Native bios
    * [ ] Homemade bios
  * [ ] Multi-platform
    * [x] Windows
    * [ ] Mac
    * [ ] linux
    * [ ] x86-64
    * [ ] ARM


# Dependents

* [Sound ADSR](https://github.com/kylophone/libADSR)
* [Sound API](https://github.com/thestk/rtaudio)
* [Sound resampler](https://github.com/avaneev/r8brain-free-src)
* [Graphics Mathematics](http://eigen.tuxfamily.org/index.php?title=Main_Page)
* [Sound API](https://github.com/jarikomppa/soloud) %
* [OpenGL GUI](https://github.com/wjakob/nanogui) %

  ? May be removed  
  % not install


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