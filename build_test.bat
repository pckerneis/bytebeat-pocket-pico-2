@echo off
REM Build script for standalone RPN VM tests on Windows
REM Compiles test_main.c with the real src/rpn_vm.c

echo Building standalone RPN VM test executable...
echo Testing REAL rpn_vm.c implementation (not a copy)
echo.

REM Try MSVC (Visual Studio) compiler first
where cl.exe >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo Using MSVC compiler...
    cl.exe /W4 /O2 /I./src /Fe:test_standalone.exe test_main.c src/rpn_vm.c
    if %ERRORLEVEL% == 0 (
        echo.
        echo Build successful! Run with: test_standalone.exe
        goto :end
    )
)

REM Try GCC (MinGW/MSYS2/Cygwin)
where gcc.exe >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo Using GCC compiler...
    gcc -Wall -Wextra -O2 -I./src -o test_standalone.exe test_main.c src/rpn_vm.c
    if %ERRORLEVEL% == 0 (
        echo.
        echo Build successful! Run with: test_standalone.exe
        goto :end
    )
)

REM Try Clang
where clang.exe >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo Using Clang compiler...
    clang -Wall -Wextra -O2 -I./src -o test_standalone.exe test_main.c src/rpn_vm.c
    if %ERRORLEVEL% == 0 (
        echo.
        echo Build successful! Run with: test_standalone.exe
        goto :end
    )
)

echo ERROR: No C compiler found!
echo.
echo Please install one of the following:
echo   - Visual Studio (MSVC): https://visualstudio.microsoft.com/
echo   - MinGW-w64: https://www.mingw-w64.org/
echo   - MSYS2: https://www.msys2.org/
echo   - Clang: https://releases.llvm.org/
echo.
echo Or use WSL and run: gcc -I./src -o test_standalone test_main.c src/rpn_vm.c
exit /b 1

:end
