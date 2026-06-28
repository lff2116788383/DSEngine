@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\Administrator\repos\DSEngine
cmake out\build\windows-x64-debug -DDSE_EDITOR_UI_TESTS=ON
cmake --build out\build\windows-x64-debug --target dse_editor_cpp
