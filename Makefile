# CHIP-8 emulator Makefile: Linux, macOS, and Windows (MinGW-w64 / MSYS2).
#
# Windows (MSYS2 MinGW64 shell or plain cmd with mingw32-make):
#   make test                      builds and runs the unit tests (no SDL2 needed)
#   make                           builds chip8.exe
#
# SDL2 is found in one of two ways:
#   1) SDL2_DIR set, e.g.  make SDL2_DIR=C:/SDL2/x86_64-w64-mingw32
#      (the folder that contains include/SDL2 and lib, from the SDL2-devel-*-mingw.zip)
#   2) otherwise sdl2-config is used (MSYS2: pacman -S mingw-w64-x86_64-SDL2)
# On Windows, copy SDL2.dll next to chip8.exe (from SDL2_DIR/bin or the MSYS2 mingw64/bin).

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

ifeq ($(OS),Windows_NT)
  EXE    := .exe
  # Static runtime so the exe does not depend on MinGW DLLs.
  LDSTATIC := -static-libgcc -static-libstdc++
  RM_CMD = -del /Q chip8.exe test_chip8.exe 2>nul
  ifneq ($(MSYSTEM),)
    RM_CMD = rm -f chip8$(EXE) test_chip8$(EXE)
  endif
else
  EXE    :=
  LDSTATIC :=
  RM_CMD = rm -f chip8 test_chip8
endif

ifdef SDL2_DIR
  SDL_CFLAGS := -I$(SDL2_DIR)/include/SDL2 -I$(SDL2_DIR)/include
  ifeq ($(OS),Windows_NT)
    SDL_LIBS := -L$(SDL2_DIR)/lib -lmingw32 -lSDL2main -lSDL2
  else
    SDL_LIBS := -L$(SDL2_DIR)/lib -lSDL2
  endif
else
  # Recursive (=) so sdl2-config only runs when the app is built, not for `make test`.
  SDL_CFLAGS = $(shell sdl2-config --cflags)
  SDL_LIBS   = $(shell sdl2-config --libs)
endif

APP  := chip8$(EXE)
TEST := test_chip8$(EXE)

all: $(APP)

$(APP): main.cpp chip8.cpp chip8.hpp
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) main.cpp chip8.cpp -o $(APP) $(SDL_LIBS) $(LDSTATIC)

$(TEST): test_chip8.cpp chip8.cpp chip8.hpp
	$(CXX) $(CXXFLAGS) test_chip8.cpp chip8.cpp -o $(TEST) $(LDSTATIC)

test: $(TEST)
	"$(CURDIR)/$(TEST)"

clean:
	$(RM_CMD)

.PHONY: all test clean
