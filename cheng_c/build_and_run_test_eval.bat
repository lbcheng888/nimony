@echo off
rem Change directory to the script's location
cd /d %~dp0

echo Cleaning ALL potentially relevant object files...
del src\*.o tests\*.o test_eval.exe 2> nul

echo Compiling dependencies...
gcc -Wall -Wextra -g -Iinclude -std=c11 -c src/l0_arena.c -o src/l0_arena.o || exit /b 1
gcc -Wall -Wextra -g -Iinclude -std=c11 -c src/l0_types.c -o src/l0_types.o || exit /b 1
gcc -Wall -Wextra -g -Iinclude -std=c11 -c src/l0_parser.c -o src/l0_parser.o || exit /b 1
gcc -Wall -Wextra -g -Iinclude -std=c11 -c src/l0_env.c -o src/l0_env.o || exit /b 1
gcc -Wall -Wextra -g -Iinclude -std=c11 -c src/l0_primitives.c -o src/l0_primitives.o || exit /b 1
gcc -Wall -Wextra -g -Iinclude -std=c11 -c src/l0_eval.c -o src/l0_eval.o || exit /b 1

echo Compiling tests/test_eval.c...
gcc -Wall -Wextra -g -Iinclude -std=c11 -c tests/test_eval.c -o tests/test_eval.o
if errorlevel 1 (
    echo Failed to compile test_eval.c
    exit /b %errorlevel%
)

echo Linking test_eval.exe...
gcc -Wall -Wextra -g -Iinclude -std=c11 tests/test_eval.o src/l0_arena.o src/l0_types.o src/l0_parser.o src/l0_env.o src/l0_primitives.o src/l0_eval.o -o test_eval.exe
if errorlevel 1 (
    echo Failed to link test_eval.exe
    exit /b %errorlevel%
)

echo Running test_eval.exe...
.\test_eval.exe
