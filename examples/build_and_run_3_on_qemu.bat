@ECHO OFF
SET __OLDPATH__=%PATH%
SET PATH=%PATH%;^
C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.28.29910\bin\Hostx64\x64;^
C:\Program Files\qemu

cl.exe /GS- /c /Zl^
 /I "C:\Program Files\EFI_Toolkit_2.0\include\efi"^
 /I "C:\Program Files\EFI_Toolkit_2.0\include\efi\em64t"^
 main.c
if not %errorlevel% == 0 goto :finish

link.exe /entry:main /dll main.obj
if not %errorlevel% == 0 goto :finish

"C:\Program Files\EFI_Toolkit_2.0\build\tools\bin\fwimage.exe"^
 app main.dll main.efi
if not %errorlevel% == 0 goto :finish

GPTServer.exe project3.xml
if not %errorlevel% == 0 goto :finish

qemu-system-x86_64.exe -m 1G -bios OVMF.fd disk.img

:finish
DEL main.obj
DEL main.dll
DEL main.efi
SET PATH=%__OLDPATH__%