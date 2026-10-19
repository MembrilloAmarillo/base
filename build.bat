@echo off
setlocal

:: Run from a Visual Studio developer prompt. Paths can be overridden in the environment.
if not defined CC set "CC=cl"
if not defined CXX set "CXX=cl"
if not defined SDL_PATH set "SDL_PATH=%~dp0code\third-party\SDL\VisualC\x64\Debug"
if not defined GLSLC_PATH (
    if defined VULKAN_SDK (
        set "GLSLC_PATH=%VULKAN_SDK%\Bin\glslc.exe"
    ) else (
        set "GLSLC_PATH=glslc"
    )
)
if not defined GRAPHICS_OPT set "GRAPHICS_OPT=/Od"
set "GRAPHICS_DEFINES="
set INC=/Icode /I"%VULKAN_SDK%\Include" /I"code\third-party\SDL\include"
set LIBS=kernel32.lib user32.lib /LIBPATH:"%SDL_PATH%" SDL3.lib /LIBPATH:"%VULKAN_SDK%\Lib" vulkan-1.lib
set GRAPHICS_OBJECTS=graphics.obj graphics_memory.obj graphics_upload.obj graphics_lifetime.obj graphics_command.obj graphics_pipeline.obj graphics_resources.obj graphics_swapchain.obj
set "TARGET=%~1"
if not defined TARGET set "TARGET=all"
if not "%~2"=="" goto usage
for %%T in (all debug release shaders clean vulkan_cube_test graphics_api_test graphics_resource_test graphics_benchmark) do (
    if /i "%TARGET%"=="%%T" goto run
)

:usage
echo Usage: %~nx0 [all^|debug^|release^|shaders^|clean^|vulkan_cube_test^|graphics_api_test^|graphics_resource_test^|graphics_benchmark]
exit /b 1

:run
pushd "%~dp0"
if errorlevel 1 exit /b 1
call :%TARGET%
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%

:debug
set "GRAPHICS_OPT=/Od"
goto all

:release
set "GRAPHICS_OPT=/O2"
goto all

:all
call :vulkan_cube_test
if errorlevel 1 exit /b 1
call :graphics_resource_test
if errorlevel 1 exit /b 1
call :graphics_api_test
if errorlevel 1 exit /b 1
call :graphics_benchmark
exit /b %ERRORLEVEL%

:vulkan_cube_test
call :text_shaders
if errorlevel 1 exit /b 1
set "GRAPHICS_DEFINES="
call :graphics_backend
if errorlevel 1 exit /b 1
%CXX% /nologo /std:c++20 /FC /Z7 /W3 %GRAPHICS_OPT% /EHsc /D_CRT_SECURE_NO_WARNINGS %INC% code\test\vulkan_cube_test.cpp /link %GRAPHICS_OBJECTS% %LIBS% /OUT:vulkan_cube_test.exe
if errorlevel 1 exit /b 1
echo Font demo built. Run vulkan_cube_test.exe --frames 120 or --interactive from the repository root.
exit /b 0

:graphics_api_test
call :api_shaders
if errorlevel 1 exit /b 1
set "GRAPHICS_DEFINES=/DGRAPHICS_TESTING"
call :graphics_backend
if errorlevel 1 exit /b 1
%CC% /nologo /TC /std:clatest /FC /Z7 /W3 %GRAPHICS_OPT% %GRAPHICS_DEFINES% /D_CRT_SECURE_NO_WARNINGS %INC% code\test\graphics_api_test.c /link %GRAPHICS_OBJECTS% %LIBS% /OUT:graphics_api_test.exe
if errorlevel 1 exit /b 1
echo Graphics API tests built. Run graphics_api_test.exe from the repository root.
exit /b 0

:graphics_resource_test
set "GRAPHICS_DEFINES="
call :graphics_backend
if errorlevel 1 exit /b 1
%CC% /nologo /TC /std:clatest /FC /Z7 /W3 %GRAPHICS_OPT% %INC% code\test\graphics_resource_test.c /link %GRAPHICS_OBJECTS% %LIBS% /OUT:graphics_resource_test.exe
exit /b %ERRORLEVEL%

:graphics_benchmark
set "GRAPHICS_OPT=/O2"
set "GRAPHICS_DEFINES=/DGRAPHICS_BENCHMARK"
call :graphics_backend
if errorlevel 1 exit /b 1
%CC% /nologo /TC /std:clatest /FC /W3 /O2 %GRAPHICS_DEFINES% %INC% code\test\graphics_benchmark.c /link %GRAPHICS_OBJECTS% %LIBS% /OUT:graphics_benchmark.exe
exit /b %ERRORLEVEL%

:graphics_backend
if not exist "%VULKAN_SDK%\Include\vulkan\vulkan.h" (
    echo Vulkan SDK headers not found. Set VULKAN_SDK to your SDK installation.
    exit /b 1
)
if not exist "%SDL_PATH%\SDL3.lib" (
    echo SDL3.lib not found. Build SDL3 or set SDL_PATH to its library directory.
    exit /b 1
)
for %%S in (graphics graphics_memory graphics_upload graphics_lifetime graphics_command graphics_pipeline graphics_resources graphics_swapchain) do (
    %CC% /nologo /TC /std:clatest /FC /Z7 /W3 %GRAPHICS_OPT% %GRAPHICS_DEFINES% %INC% /c code\rendering\%%S.c /Fo:%%S.obj
    if errorlevel 1 exit /b 1
)
if not exist SDL3.dll (
    copy /y "%SDL_PATH%\SDL3.dll" SDL3.dll >nul
    if errorlevel 1 exit /b 1
)
exit /b 0

:shader
"%GLSLC_PATH%" --target-env=vulkan1.3 "%~1" -o "%~1.spv"
exit /b %ERRORLEVEL%

:text_shaders
for %%S in (data\Sample.vert data\Sample.frag) do (
    call :shader "%%S"
    if errorlevel 1 exit /b 1
)
exit /b 0

:api_shaders
for %%S in (code\test\shaders\graphics_test.vert code\test\shaders\graphics_test.frag) do (
    call :shader "%%S"
    if errorlevel 1 exit /b 1
)
exit /b 0

:shaders
call :text_shaders
if errorlevel 1 exit /b 1
call :api_shaders
exit /b %ERRORLEVEL%

:clean
:: Delete only known outputs; do not remove arbitrary executables or user files.
for %%T in (vulkan_cube_test graphics_api_test graphics_resource_test graphics_benchmark) do (
    for %%E in (exe pdb obj ilk) do if exist "%%T.%%E" del /q "%%T.%%E"
)
for %%O in (%GRAPHICS_OBJECTS% graphics_cube.obj) do if exist "%%O" del /q "%%O"
for %%S in (data\Sample.vert.spv data\Sample.frag.spv code\test\shaders\graphics_test.vert.spv code\test\shaders\graphics_test.frag.spv) do (
    if exist "%%S" del /q "%%S"
)
echo Clean completed.
exit /b 0
