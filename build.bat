@echo off
:: Initialize Visual Studio environment
:: call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvarsall.bat" x64
:: call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
:: === VARIABLES DE USUARIO ===
:: Set these paths according to your system
set "GLSLC_PATH=C:\VulkanSDK\1.4.321.1\Bin\glslc.exe"
set SDL_PATH=".\code\third-party\SDL\VisualC\x64\Debug\"
:: Compiler
set CC=cl
set CXX=cl

:: Debug flags
set CFLAGS_DEBUG=/nologo /std:c++17 /FC /Z7 /DDEBUG /W3 /wd4996 /wd4576 /Od /Ob1 /EHsc
set CXXFLAGS_DEBUG=/nologo /std:c++17 /FC /Z7 /W3 /wd4996 /wd4576 /Od /Ob1 /EHsc
set INC=/Icode /I"C:\VulkanSDK\1.4.321.1\Include" /I".\code\third-party\SDL\include"

:: Release flags
set CFLAGS_RELEASE=/O2 /W3 /wd4996 /std:c++17
set CXXFLAGS_RELEASE=/O2 /std:c++17 /W3 

:: Libraries (Windows .lib files)
set LIBS=kernel32.lib ntdll.lib user32.lib gdi32.lib /LIBPATH:%SDL_PATH% SDL3.lib /LIBPATH:"C:\VulkanSDK\1.4.321.1\Lib" vulkan-1.lib ws2_32.lib

:: Source files
set XXHASH=code\third-party\xxhash.c
set SRC_C=code\Samples\UI_Sample.c
set SampleRender_SRC=code\Samples\RenderLibrary.c
set VMA=code\third-party\vk_mem_alloc.c

:: Output files
set RenderLibraryExe=RenderLibrary.exe
set DEBUG_EXE=UI_Sample.exe
set RELEASE_EXE=UI_Sample_Release.exe

set SDL_USAGE=

:: === MAIN TARGETS ===
if "%1"==""                 goto all
if /i "%1"=="all"           goto all
if /i "%1"=="shaders"       goto shaders
if /i "%1"=="clean"         goto clean
if /i "%1"=="debug"         goto debug
if /i "%1"=="release"       goto release
if /i "%1"=="RenderLibrary" goto render
if /i "%1"=="DrawSample"    goto draw_sample
if /i "%1"=="todolist"      goto todolist
if /i "%1"=="csv_test"      goto csv_test
if /i "%2"=="SDL_USAGE"     set SDL_USAGE=/D SDL_USAGE

echo Usage: %0 [all^|debug^|release^|shaders^|clean]
exit /b 1

:all
call :debug
call :render
call :draw_sample
if errorlevel 1 exit /b 1
call :shaders
exit /b

:debug
echo Compiling %DEBUG_EXE% (Debug)...
del /q vk_mem_alloc.obj 2>nul

:: Compile VMA as C++ first
%CXX% %CXXFLAGS_DEBUG% %INC% /c "%VMA%" /Fo:vk_mem_alloc.obj
if errorlevel 1 exit /b 1

:: Compile main app as C and link with the VMA object file
%CC% %CFLAGS_DEBUG% %INC% /c "%XXHASH%" /Fo:xxhash.obj
%CC% %CFLAGS_DEBUG% %INC% "%SRC_C%" /link xxhash.obj vk_mem_alloc.obj %LIBS% /OUT:"%DEBUG_EXE%"
if errorlevel 1 exit /b 1
echo Debug build completed.
exit /b

:render
%CC% %CFLAGS_DEBUG% %INC% "%SampleRender_SRC%" /link xxhash.obj vk_mem_alloc.obj %LIBS% /OUT:"%RenderLibraryExe%"
if errorlevel 1 exit /b 1
echo Release build completed.
exit /b

:draw_sample
%CC% %CFLAGS_DEBUG% %INC% ./code/Samples/DrawSample.c /link xxhash.obj vk_mem_alloc.obj %LIBS% /OUT:DrawSample.exe
if errorlevel 1 exit /b 1
echo Release build completed.
exit /b

:todolist
del /q vk_mem_alloc.obj 2>nul
del /q xxhash.obj 2>nul

:: Compile xxhash as C++ (same as other files)
%CXX% /TP %CXXFLAGS_DEBUG% %INC% /c "%XXHASH%" /Fo:xxhash.obj
if errorlevel 1 exit /b 1

:: Compile VMA as C++
%CXX% /TP %CXXFLAGS_DEBUG% %INC% /c "%VMA%" /Fo:vk_mem_alloc.obj
if errorlevel 1 exit /b 1

echo %CXX% %CXXFLAGS_DEBUG% %INC% ./code/Samples/ToDoList.cpp %SDL_USAGE% /link xxhash.obj vk_mem_alloc.obj %LIBS% /OUT:todolist.exe

:: Compile and link ToDoList
%CXX% %CXXFLAGS_DEBUG% %INC% ./code/Samples/ToDoList.cpp %SDL_USAGE% /link xxhash.obj vk_mem_alloc.obj %LIBS% /OUT:todolist.exe
if errorlevel 1 exit /b 1
echo ToDoList build completed.
exit /b

:csv_test
echo %CXX% %CFLAGS_RELEASE% %INC% ./code/Samples/CsvTest.cpp /link %LIBS% /OUT:csv_test.exe
%CXX% %CFLAGS_RELEASE% %INC% ./code/Samples/CsvTest.cpp /link %LIBS% /OUT:csv_test.exe

if errorlevel 1 exit /b 1
echo csv_test build completed.
exit /b

:release
echo Compiling %RELEASE_EXE% (Release)...
del /q vk_mem_alloc_release.obj 2>nul

:: Compile VMA as C++ first
%CXX% %CXXFLAGS_RELEASE% %INC% /c "%VMA%" /Fo:vk_mem_alloc_release.obj
if errorlevel 1 exit /b 1

:: Compile main app as C and link with the VMA object file
%CC% %CFLAGS_RELEASE% %INC% "%SRC_C%" /link vk_mem_alloc_release.obj %LIBS% /OUT:"%RELEASE_EXE%"
if errorlevel 1 exit /b 1
echo Release build completed.
exit /b

:shaders
echo Compiling shaders...
if not exist "%GLSLC_PATH%" (
    echo Warning: glslc not found at %GLSLC_PATH%. Skipping shader compilation.
    echo Please update GLSLC_PATH or install the Vulkan SDK.
    exit /b 0
)
"%GLSLC_PATH%" ./data/compute.comp -o ./data/compute.comp.spv
"%GLSLC_PATH%" ./data/ColoredTriangle.vert -o ./data/ColoredTriangle.vert.spv
"%GLSLC_PATH%" ./data/ColoredTriangle.frag -o ./data/ColoredTriangle.frag.spv
"%GLSLC_PATH%" ./data/ui_render.vert -o ./data/ui_render.vert.spv
"%GLSLC_PATH%" ./data/ui_render.frag -o ./data/ui_render.frag.spv
"%GLSLC_PATH%" ./data/Sample.vert -o ./data/Sample.vert.spv
"%GLSLC_PATH%" ./data/Sample.frag -o ./data/Sample.frag.spv
"%GLSLC_PATH%" ./data/line.vert -o ./data/line.vert.spv
"%GLSLC_PATH%" ./data/line.frag -o ./data/line.frag.spv

echo Shader compilation completed.
exit /b

:clean
echo Cleaning build artifacts...
del /q *.exe *.pdb *.obj *.ilk 2>nul
del /q .\data\*.spv 2>nul
echo Clean completed.
exit /b
