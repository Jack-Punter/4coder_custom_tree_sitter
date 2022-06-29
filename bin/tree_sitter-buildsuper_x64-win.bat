@echo off

REM TODO(jack): Clean up this build file a little, some of the variables make
REM the actuall commands a little confusing

REM usage: <script> [target [mode]]
REM  works from any directory, but must remain in custom/bin path to work
REM  target : if specified determines the file that acts as the build target
REM           when not specified the build target is 4coder_default_bindings.cpp
REM  mode : if set to "release" builds with optimizations
REM  The build steps are:
REM   1. preprocess the build target
REM   2. build the metadata generator
REM   3. run the metadata generator on the result from (1)
REM   4. build the build target
REM   5. cleanup after the metadata generator
REM  All output files are generated in the current directory when the script is run

set location=%cd%
set me=%~dp0
cd %me%
cd ..
set custom_root=%cd%
set custom_bin=%custom_root%\bin
cd %location%

if NOT "%Platform%" == "X64" IF NOT "%Platform%" == "x64" (call "%custom_bin%\setup_cl_x64.bat")

set meta_target=%1
set target=%1
if "%target%" == "" set target=%custom_root%\4coder_default_bindings.cpp

set debug=/Zi
set release=/O2 /Zi
set mode=%debug%
if "%2" == "release" (set mode=%release%)

set binname=%3
if "%binname%" == "" set binname="custom_4coder"

set includes=-I"%custom_root%" -I"%custom_root%\custom_include"

set opts=/W4 /wd4310 /wd4100 /wd4201 /wd4505 /wd4996 /wd4127 /wd4510 /wd4512 /wd4610 /wd4457 /WX
set opts=%opts% /GR- /nologo /FC
set opts=%opts% %includes%
set opts=%opts% /D OS_WINDOWS=1 /D OS_LINUX=0 /D OS_MAC=0
set opts=%opts% %mode%

set preproc_file=4coder_command_metadata.i
set meta_opts=/P /Fi"%preproc_file%" /DMETA_PASS

set build_dll=/LD /link /INCREMENTAL:NO /OPT:REF /RELEASE /PDBALTPATH:%%%%_PDB%%%%
set build_dll=%build_dll% /EXPORT:get_version /EXPORT:init_apis

echo Meta Pass
echo ---------------------------------
call cl %opts% %meta_opts% "%meta_target%"
call cl %opts% "%custom_root%\4coder_metadata_generator.cpp" /Femetadata_generator
metadata_generator -R "%custom_root%" "%cd%\%preproc_file%"

REM Build the tree_sitter parser, scanner and library in release mode for performance 
REM despite what mode the custom layer is being built in

echo:
echo Compile and link tree-sitter library and parsers
echo ---------------------------------
del *.obj

REM tree-sitter lib
call cl -c /O2 %includes% /nologo /Zi %custom_root%\tree-sitter\lib.c   /Fotree-sitter-lib.obj

REM C++ Parser (This needs to be two calls to cl so that you can specify the obj file names)
call cl -c /O2 %includes% /nologo /Zi %custom_root%\lang\cpp\parser.c   /Focpp_parser.obj
call cl -c /O2 %includes% /nologo /Zi %custom_root%\lang\cpp\scanner.cc /Focpp_scanner.obj

REM C Parser
call cl -c /O2 %includes% /nologo /Zi %custom_root%\lang\c\parser.c     /Foc_parser.obj

REM Odin Parser
call cl -c /O2 %includes% /nologo /Zi %custom_root%\lang\odin\parser.c  /Foodin_parser.obj

REM Link tree-sitter lib and parser obj files into a static library to link into main custom dll
call lib  /nologo *.obj /OUT:tree-sitter.lib

echo:
echo Build custom dll
echo ---------------------------------
REM We dont need the /link because its in the %build_dll% variable
call cl %opts% "%target%" /Fe%binname% %build_dll% tree-sitter.lib

echo:
echo Cleanup
echo ---------------------------------
REM file spammation preventation
del metadata_generator*
del *.exp
del *.obj
del *.lib
del %preproc_file%
