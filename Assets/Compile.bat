@PUSHD %~DP0
SET src=TetrisAI.c
SET exe=Tetris_release.exe
SET cflags=-O3 -Os -s -m32 -std=c11
gcc.exe %src% -c -o Tetris.o %cflags%
windres.exe -i Tetris.rc -F pe-i386 --input-format=rc -o Tetris.res -O coff
gcc.exe Tetris.o Tetris.res -o %exe% %cflags%
@DEL /F /Q Tetris.o
@DEL /F /Q Tetris.res
@POPD