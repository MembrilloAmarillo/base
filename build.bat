@echo off
:: === VARIABLES DE USUARIO ===
:: Set these paths according to your system
set "GLSLC_PATH=C:\VulkanSDK\1.4.328.1\Bin\glslc.exe"

:: Compiler
set CC=cl
set CXX=cl

:: Debug flags
set CFLAGS_DEBUG=/nologo /std:c11 /FC /Z7 /DDEBUG /W3 /wd4996 /Od /Ob1 /TC /fsanitize=address
set CXXFLAGS_DEBUG=/nologo /std:c++14 /FC /Z7 /W3 /Od /Ob1 /TP /fsanitize=address
set INC=/Icode /I"C:\VulkanSDK\1.4.328.1\Include"

:: Release flags
set CFLAGS_RELEASE=/O2 /W3 /wd4996 /TC
set CXXFLAGS_RELEASE=/O2 /std:c++14 /W3 /TP

:: Libraries (Windows .lib files)
set LIBS=kernel32.lib user32.lib gdi32.lib /LIBPATH:"C:\VulkanSDK\1.4.328.1\Lib" vulkan-1.lib ws2_32.lib

:: Source files
set XXHASH=code\third-party\xxhash.c
set SRC_C=code\Samples\UI_Sample.c
set VMA=code\third-party\vk_mem_alloc.c

:: Output files
set DEBUG_EXE=UI_Sample.exe
set RELEASE_EXE=UI_Sample_Release.exe

:: === MAIN TARGETS ===
if "%1"=="" goto all
if /i "%1"=="all" goto all
if /i "%1"=="shaders" goto shaders
if /i "%1"=="clean" goto clean
if /i "%1"=="debug" goto debug
if /i "%1"=="release" goto release

echo Usage: %0 [all^|debug^|release^|shaders^|clean]
exit /b 1

:all
call :debug
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
echo Shader compilation completed.
exit /b

:clean
echo Cleaning build artifacts...
del /q UI_Sample.exe UI_Sample_Release.exe vk_mem_alloc.obj vk_mem_alloc_release.obj 2>nul
del /q .\data\*.spv 2>nul
echo Clean completed.
exit /b
