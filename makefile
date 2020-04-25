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

all: dir $(BINARY)
	@echo build over

$(BINARY): $(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(OBJPATH)/%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(OBJPATH)/%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<


.PHONY : clean dir all
clean:
	rm -rf bin/*

dir:
	@-mkdir -p $(dir $(OBJECTS))