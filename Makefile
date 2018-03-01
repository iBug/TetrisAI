# Project: TetrisAI
# Makefile created by Dev-C++ 5.11

BUILD    = release

CC       = gcc.exe
WINDRES  = windres.exe
RES      = Tetris.res
OBJ      = TetrisAI.o
LINKOBJ  = $(OBJ) $(RES)
#LIBS     = -L"C:/Program Files (x86)/Dev-Cpp/MinGW64/lib" -L"C:/Program Files (x86)/Dev-Cpp/MinGW64/mingw32/lib" -static-libgcc
#INCS     = -I"C:/Program Files (x86)/Dev-Cpp/MinGW64/include" -I"C:/Program Files (x86)/Dev-Cpp/MinGW64/mingw32/include" -I"C:/Program Files (x86)/Dev-Cpp/MinGW64/lib/gcc/mingw32/6.3.0/include"
BIN      = Tetris_$(BUILD).exe
CFLAGS   = $(INCS) -Wall -Wextra -Wno-unused-parameter -O3 -Os -s -std=c99 -m32
RM      ?= rm.exe -f

.PHONY: all clean

all: $(BIN)

clean:
	${RM} $(OBJ) $(BIN) $(RES)

$(BIN): $(OBJ) $(RES)
	$(CC) $(LINKOBJ) -o $(BIN) $(LIBS) $(CFLAGS)

$(OBJ): TetrisAI.c
	$(CC) -c TetrisAI.c -o $@ $(CFLAGS)

$(RES): Tetris.rc
	$(WINDRES) -i Tetris.rc -F pe-i386 --input-format=rc -o $@ -O coff 

