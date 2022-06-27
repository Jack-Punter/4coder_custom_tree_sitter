@echo off

if not exist build mkdir build
pushd build
call ..\bin\tree_sitter-buildsuper_x64-win.bat ..\4coder_tree_sitter.cpp debug
REM call .\bin\tree_sitter-buildsuper_x64-win.bat .\4coder_tree_sitter.cpp release

echo:
echo Copy dll and pdb's to the 4coder root
echo ---------------------------------
copy .\custom_4coder.dll ..\..\custom_4coder.dll
copy .\custom_4coder.pdb ..\..\custom_4coder.pdb
copy .\vc140.pdb ..\..\vc140.pdb

popd 