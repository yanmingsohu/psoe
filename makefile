CC = gcc
CXX = g++

CPPFLAGS = -Isrc
CFLAGS = -O2 -Wall -Wextra 
CXXFLAGS = -O2 -Wall -Wextra -std=gnu++14
#LDFLAGS = -lgcc
LDFLAGS = 

BINARY = bin/ps1
OBJPATH = bin/obj
SOURCES =

# ===--- setting SOURCES ---=== #
include src/test/makefile
include src/makefile


OBJECTS = $(patsubst %.c, %.o, $(patsubst %.cpp, $(OBJPATH)/%.o, $(SOURCES)))
DEPS = $(OBJECTS:.o=.d)

all: init $(BINARY)
	@echo "\x1b[30;47mBuild over\x1b[0m\n"

-include $(DEPS)

$(BINARY): $(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(OBJPATH)/%.o: %.c
	@echo "\x1b[36;44m"Compile $< "\x1b[0m"
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MF $(patsubst %.o,%.d,$@) -MT $@ $<
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(OBJPATH)/%.o: %.cpp
	@echo "\x1b[36;44m"Compile $< "\x1b[0m"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MM -MF $(patsubst %.o,%.d,$@) -MT $@ $< 
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $< 


.PHONY : clean init all
clean:
	rm -rf bin/*

init:
	@-mkdir -p $(dir $(OBJECTS))