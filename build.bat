@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

cd x:\
if not exist build mkdir build
pushd build

SET CompilerFlags=-GR- -EHsc -Od -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -Zi /std:c++latest /nologo
REM SET CompilerFlags=-GR- -EHsc -O2 -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 /std:c++latest /nologo

SET LinkerFlags=-opt:ref user32.lib gdi32.lib SDL2.lib SDL2_ttf.lib opengl32.lib glu32.lib /LIBPATH:C:\sdl\SDL2-2.0.4\lib\x64 /LIBPATH:C:\sdl\SDL2_ttf-2.0.14\lib\x64 /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\lib\10.0.10240.0\ucrt\x64" /SUBSYSTEM:CONSOLE

SET IncludeDirectories=/I "C:\sdl\SDL2-2.0.4\include" /I "C:\sdl\SDL2_ttf-2.0.14\include"

echo.
echo.

echo.
echo.
echo ========================== Building Win32 Executable ==========================
echo.

cl %CompilerFlags% %IncludeDirectories% ..\src\tetris.cpp /link %LinkerFlags%

echo ===============================================================================
	
popd
