@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0"
cl /nologo /O2 /MT /std:c++17 /EHsc /utf-8 /LD /I "..\UnknownPolymorphism\include" src\up_helpers.cpp src\psych_state.cpp src\registry.cpp src\hooks.cpp src\module.cpp /Fe:PsychiatryUnknown.dll
