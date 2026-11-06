@echo off
rem Build dist\forever_mods.html: C++ puzzle/app + Phone shell compiled to WebAssembly (single offline html).
rem First run: install emsdk once with:  cd .emsdk ^&^& emsdk install latest ^&^& emsdk activate latest
setlocal
set HERE=%~dp0
set SRC=%HERE%..\..\Source
set EMSDK_DIR=%HERE%.emsdk

if not exist "%EMSDK_DIR%\emsdk_env.bat" (
  echo emsdk not found in %EMSDK_DIR%. See README.md.
  exit /b 1
)
call "%EMSDK_DIR%\emsdk_env.bat" >nul

if not exist "%HERE%build" mkdir "%HERE%build"
if not exist "%HERE%dist" mkdir "%HERE%dist"

call em++ -O2 -std=c++17 -fexceptions ^
  -I "%HERE%..\ModHost\shim" -I "%HERE%..\ModHost" -I "%SRC%\Dependence" -I "%SRC%\Dependence\common" -I "%SRC%\Core" -I "%SRC%\Basic" -I "%SRC%\Basic\player" ^
  "%HERE%web_main.cpp" "%HERE%..\ModHost\host_core.cpp" ^
  "%SRC%\Dependence\player\canvas.cpp" "%SRC%\Dependence\player\app_factory.cpp" "%SRC%\Dependence\player\puzzle_factory.cpp" ^
  "%SRC%\Dependence\player\app_mod.cpp" "%SRC%\Dependence\player\puzzle_mod.cpp" ^
  "%SRC%\Dependence\common\json.cpp" "%SRC%\Dependence\common\error.cpp" "%SRC%\Dependence\common\handle.cpp" ^
  "%SRC%\Dependence\story\event.cpp" "%SRC%\Dependence\story\expression.cpp" ^
  "%SRC%\Core\player\phone.cpp" "%SRC%\Core\player\app.cpp" "%SRC%\Core\player\puzzle.cpp" ^
  "%SRC%\Basic\player\puzzle_basic.cpp" "%SRC%\Basic\player\puzzle_poker.cpp" "%SRC%\Basic\player\puzzle_burger.cpp" "%SRC%\Basic\player\puzzle_miner.cpp" "%SRC%\Basic\player\puzzle_tower.cpp" "%SRC%\Basic\player\puzzle_chase.cpp" "%SRC%\Basic\player\puzzle_rhythm.cpp" "%SRC%\Basic\player\puzzle_tubes.cpp" "%SRC%\Basic\player\puzzle_coins.cpp" "%SRC%\Basic\player\puzzle_dance.cpp" "%SRC%\Basic\player\puzzle_dodge.cpp" "%SRC%\Basic\player\app_basic.cpp" ^
  -sALLOW_MEMORY_GROWTH=1 -sMODULARIZE=1 -sEXPORT_NAME=createModule -sENVIRONMENT=web -sSINGLE_FILE=1 ^
  -sDISABLE_EXCEPTION_CATCHING=0 ^
  --embed-file "%HERE%..\..\Resource\Mod\Basic@/res/Basic" ^
  "-sEXPORTED_RUNTIME_METHODS=UTF8ToString,UTF32ToString,stringToUTF8,HEAPU8" ^
  "-sEXPORTED_FUNCTIONS=_malloc,_free" ^
  -o "%HERE%build\forever_mods.js"
if errorlevel 1 exit /b 1

python "%HERE%make_html.py" "%HERE%build\forever_mods.js" "%HERE%index.template.html" "%HERE%dist\forever_mods.html"
if errorlevel 1 exit /b 1
echo Built %HERE%dist\forever_mods.html
