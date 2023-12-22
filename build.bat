@set sdl2_DIR=..\..\lib\SDL2-2.0.22

cmake -L -D sdl2_DIR=%sdl2_DIR% -B build .
cmake --build build --config Debug
REM cmake --build build --config Release

copy /B "%sdl2_DIR%\lib\x64\SDL2.dll" build\src\Debug
REM copy /B "%sdl2_DIR%\lib\x64\SDL2.dll" build\src\Release
pause

