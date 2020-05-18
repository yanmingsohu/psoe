#include "term.h"

#include <iostream>

#if defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #include <Windows.h>
#else
  #include <sys/ioctl.h>
  #include <unistd.h>
#endif


namespace ps1e_term {

static const char* MOVE_LEFT  = "\r";
static const char* MOVE_UP    = "\x1b[1a";
static const char* CLEAR_LINE = "\x1b[2k";


Dim size() {
#if defined(__EMSCRIPTEN__)
  return Dim{80, 43};
#elif defined(_WIN32)
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  int columns, rows;

  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
  rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
  return Dim{columns, rows};
#else
  winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  return Dim{w.ws_col, w.ws_row};
#endif
}


TermScreen::TermScreen() : d(size()) {
}


void TermScreen::clean() {
  puts("\33[0;0H");
return;
  puts(MOVE_LEFT);
  puts(CLEAR_LINE);
  for (int i=0; i<d.y; ++i) {
    puts(MOVE_UP);
    puts(CLEAR_LINE);
  }
}


void TermScreen::render() {
  for (auto p = buf.begin(); p != buf.end(); ++p) {
    std::cout << *p;
  }
}


void TermScreen::add(char* str) {
  buf.push_back(str);
  for (int i= d.y << 1; buf.size() >= i;) {
    buf.pop_front();
  }
}


void TermScreen::code(char* str) {
  add(str);
  clean();
  render();
}


}


int main() {
  char buf[80];
  ps1e_term::TermScreen ts;

  for (int i=0; i<100000; ++i) {
    sprintf_s(buf, "%d\n", i);
    ts.code(buf);
  }
  return 0;
}