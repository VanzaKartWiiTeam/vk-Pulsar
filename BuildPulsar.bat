
SETLOCAL EnableDelayedExpansion
@echo off
cls
if not exist build mkdir build
del build\*.o

::DEBUG only works if you have the map and readelf (which is part of MinGW)
SET "debug="
::SET "cwDWARF="
::if "%1" equ "-d" SET "debug=-debug=0x803992E0 -map=^"Dolphin Emulator\Maps\RMCP01.map^" -readelf=^"C:\MinGW\bin\readelf.exe^""
::if "%1" equ "-d" SET "cwDWARF=-g"

:: Sources and Compiler
SET "ENGINE=.\KamekInclude"
set "GAMESOURCE=.\GameSource"
SET "PULSAR=.\PulsarEngine"

:: Change this as necessary depending on where you put CodeWarrior
SET "CC="C:\Program Files (x86)\Freescale\CW for MPC55xx and MPC56xx 2.10\PowerPC_EABI_Tools\Command_Line_Tools\mwcceppc.exe""

:: Where the built Code.pul is deployed. The copy happens at the end of this script, after the
:: link: doing it up here deployed the *previous* build every time.
SET "RIIVO=E:\Dolphin-x64\User\Load\Riivolution\VanzaKart\VanzaKart"

:: Compiler flags and folder
:: NB: niente -proc gekko qui, a differenza di rr-pulsar: la licenza di questa edizione di
:: CodeWarrior (MPC55xx/56xx) rifiuta quel processore. Verificato che il codice generato e'
:: comunque identico per tipo a quello della build del creatore (stesse istruzioni FP classiche,
:: nessuna istruzione illegale sul 750CL), quindi il target generico non e' un problema.
SET CFLAGS=-I- -i %ENGINE% -i %GAMESOURCE% -i %PULSAR% ^
  -opt all -inline auto -enum int -fp hard -sdata 0 -sdata2 0 -maxerrors 1 -func_align 4 %cwDWARF%
:: Pass -prod to build a release: this is what enables the WiiLink codehandler
:: check that refuses online logins when the Gecko handler is loaded. It is off by
:: default so cheats stay usable for local testing.
SET DEFINE= -DBETA
if /I "%1" equ "-prod" SET DEFINE= -DBETA -DPROD
if /I "%1" equ "-prod" echo Building with -DPROD: online cheat check ENABLED

if "!CC!" == "" (
    echo You need to specify the path to CodeWarrior by setting the CC variable
    exit /b 1
)

:: Compile CPP
%CC% %CFLAGS% -c -o "build/kamek.o" "%ENGINE%\kamek.cpp"
if %ErrorLevel% neq 0 (
    echo Compilation of kamek.cpp failed
    exit /b %ErrorLevel%
)
%CC% %CFLAGS% -c -o "build/RuntimeWrite.o" "%ENGINE%\RuntimeWrite.cpp"
if %ErrorLevel% neq 0 (
    echo Compilation of RuntimeWrite.cpp failed
    exit /b %ErrorLevel%
)

SET OBJECTS=
for /R %PULSAR% %%H in (*.cpp) do (
    %CC% %CFLAGS% %DEFINE% -c -o "build/%%~nH.o" "%%H"
    if !ErrorLevel! neq 0 (
        echo Compilation of %%H failed
        exit /b !ErrorLevel!
    )
    SET "OBJECTS=build/%%~nH.o !OBJECTS!"
)

:: Link
echo Linking... %time%
".\KamekLinker\Kamek.exe" "build/kamek.o" "build/RuntimeWrite.o" %OBJECTS% %debug% -dynamic -externals="%GAMESOURCE%/symbols.txt" -versions="%GAMESOURCE%/versions.txt" -output-combined=build\Code.pul -output-map=build\Code.$KV$.map

if %ErrorLevel% neq 0 (
    echo Linking failed, nothing was deployed
    pause
    exit /b %ErrorLevel%
)

if NOT "!RIIVO!" == "" (
    if not exist "!RIIVO!\Binaries" mkdir "!RIIVO!\Binaries"
    xcopy /Y build\*.pul "!RIIVO!\Binaries" /i /q
    echo Binaries copied to !RIIVO!\Binaries
)

:end
pause
ENDLOCAL