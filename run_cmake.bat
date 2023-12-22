@set sdl2_DIR=..\..\lib\SDL2-2.0.22

REM rd /S /Q build
@md build

cmake -L -D sdl2_DIR=%sdl2_DIR% -B build .
REM cmake --build build --config Debug

rem copy /B "%sdl2_DIR%\lib\x64\SDL2.dll" build\src\Debug
pause

