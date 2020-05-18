#pragma once

#include "../util.h"
#include <list>
#include <string>

namespace ps1e_term {


struct Dim {
  int x;
  int y;
};


class TermScreen {
private:
  
  Dim d;
  std::list<std::string> buf;

  void add(char *str);
  void clean();
  void render();

public:
  TermScreen();

  void code(char *str);
  void info(char *str);
  void reg(char *str);
};


Dim size();


}