@echo off

REM me is in the 4coder/custom-tree-sitter
set me=%~dp0
set custom_root=%cd%
set includes=-I"%custom_root%" -I"%custom_root%\custom_include"

if not exist build mkdir build
pushd build

echo:
echo Metadesk Build

echo ---------------------------------
del .\jpts_prebuild.exe
call cl /Od /nologo /W3 /Zi /FC ..\jpts_prebuild.cpp %includes%
call jpts_prebuild.exe
echo:

echo:
echo Custom Buildsuper
echo ---------------------------------
call ..\bin\tree_sitter-buildsuper_x64-win.bat ..\4coder_tree_sitter.cpp debug
REM call .\bin\tree_sitter-buildsuper_x64-win.bat .\4coder_tree_sitter.cpp release

echo:
echo Copy dll and pdb's to the 4coder root
echo ---------------------------------
copy .\custom_4coder.dll ..\..\custom_4coder.dll
copy .\custom_4coder.pdb ..\..\custom_4coder.pdb
copy .\vc140.pdb ..\..\vc140.pdb

popd 