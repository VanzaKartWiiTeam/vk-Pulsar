@echo off
SETLOCAL EnableDelayedExpansion
cls
del build\*.o

:: CPP compilation settings
SET CC="C:\Program Files (x86)\Freescale\CW for MPC55xx and MPC56xx 2.10\PowerPC_EABI_Tools\Command_Line_Tools\mwcceppc.exe"
SET CFLAGS=-I- -i %cd% -i "../KamekInclude" -i "../GameSource" -i "../GameSource/MarioKartWii" -i PulsarEngine ^
  -opt all -inline auto -enum int -fp hard -sdata 0 -sdata2 0 -maxerrors 1 -func_align 4 %cwDWARF%
SET DEFINE=-DPROD

::: CPP Sources
%CC% %CFLAGS% -c -o "build/mkw.o" "mkw.cpp"
%CC% %CFLAGS% -c -o "build/kamek.o" "kamekLoader.cpp"

:: Link
echo Linking... %time%
"../KamekLinker/Kamek.exe" "build/mkw.o" "build/kamek.o" -static=0x80004000 -output-code=Loader.pul

:end
ENDLOCAL