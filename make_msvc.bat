@echo off

if exist "%VCINSTALLDIR%" (
	goto compile
)

if exist "%VS100COMNTOOLS%" (
	set VSPATH="%VS100COMNTOOLS%"
	goto set_env
)
if exist "%VS90COMNTOOLS%" (
	set VSPATH="%VS90COMNTOOLS%"
	goto set_env
)
if exist "%VS80COMNTOOLS%" (
	set VSPATH="%VS80COMNTOOLS%"
	goto set_env
)

echo You need Microsoft Visual Studio 8, 9 or 10 installed
pause
exit

:set_env
@set INCLUDE=D:\WpdPack\Include;D:\WpdPack\Include\pcap;%INCLUDE%
@set LIB=D:\WpdPack\lib;%LIB%
call %VSPATH%vsvars32.bat

:compile
@echo Building l2cap...
@cl /c /D_CRT_SECURE_NO_DEPRECATE /DWIN32 /DWPCAP /D__MINGW32__ /W3 /O2 /TC /Zi /GS- /GL /nologo /MT /I src\ /I src\svpackets\ /I src\clpackets\ src\*.c src\clpackets\*.c src\svpackets\*.c
@rc /l"0x0409" /nologo /fo"res.res" src\res.rc
@link /nologo /out:l2cap.exe wpcap.lib ws2_32.lib comctl32.lib kernel32.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS /LTCG /DYNAMICBASE:NO /MACHINE:X86 res.res *.obj
@mt /nologo /outputresource:"l2cap.exe;#1" /manifest src\manifest.xml

@del res.res *.pdb
@del *.obj