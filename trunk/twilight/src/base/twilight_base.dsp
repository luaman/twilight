# Microsoft Developer Studio Project File - Name="twilight_base" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=twilight_base - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "twilight_base.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "twilight_base.mak" CFG="twilight_base - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "twilight_base - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "twilight_base - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "twilight_base - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../include" /I "../../SDL/include" /D "WIN32" /D "NDEBUG" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x40c /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "twilight_base - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../include" /I "../../SDL/include" /D "WIN32" /D "_DEBUG" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x40c /d "_DEBUG"
# ADD RSC /l 0x40c /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "twilight_base - Win32 Release"
# Name "twilight_base - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\buildnum.c
# End Source File
# Begin Source File

SOURCE=.\cmd.c
# End Source File
# Begin Source File

SOURCE=.\collision.c
# End Source File
# Begin Source File

SOURCE=.\cpu.c
# End Source File
# Begin Source File

SOURCE=.\crc.c
# End Source File
# Begin Source File

SOURCE=.\cvar.c
# End Source File
# Begin Source File

SOURCE=.\locs.c
# End Source File
# Begin Source File

SOURCE=.\mathlib.c
# End Source File
# Begin Source File

SOURCE=.\matrixlib.c
# End Source File
# Begin Source File

SOURCE=.\mdfour.c
# End Source File
# Begin Source File

SOURCE=.\mod_brush.c
# End Source File
# Begin Source File

SOURCE=.\model.c
# End Source File
# Begin Source File

SOURCE=.\parm.c
# End Source File
# Begin Source File

SOURCE=.\strlib.c
# End Source File
# Begin Source File

SOURCE=.\wad.c
# End Source File
# Begin Source File

SOURCE=.\zone.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\include\anorm_dots.h
# End Source File
# Begin Source File

SOURCE=..\..\include\anorms.h
# End Source File
# Begin Source File

SOURCE=..\..\include\bspfile.h
# End Source File
# Begin Source File

SOURCE=..\..\include\cmd.h
# End Source File
# Begin Source File

SOURCE=..\..\include\collision.h
# End Source File
# Begin Source File

SOURCE=..\..\include\common.h
# End Source File
# Begin Source File

SOURCE=..\..\include\console.h
# End Source File
# Begin Source File

SOURCE=..\..\include\cpu.h
# End Source File
# Begin Source File

SOURCE=..\..\include\crc.h
# End Source File
# Begin Source File

SOURCE=..\..\include\cvar.h
# End Source File
# Begin Source File

SOURCE=..\..\include\draw.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gl_warp_sin.h
# End Source File
# Begin Source File

SOURCE=..\..\include\host.h
# End Source File
# Begin Source File

SOURCE=..\..\include\info.h
# End Source File
# Begin Source File

SOURCE=..\..\include\input.h
# End Source File
# Begin Source File

SOURCE=..\..\include\keys.h
# End Source File
# Begin Source File

SOURCE=..\..\include\locs.h
# End Source File
# Begin Source File

SOURCE=..\..\include\mathlib.h
# End Source File
# Begin Source File

SOURCE=..\..\include\matrixlib.h
# End Source File
# Begin Source File

SOURCE=..\..\include\mdfour.h
# End Source File
# Begin Source File

SOURCE=..\..\include\menu.h
# End Source File
# Begin Source File

SOURCE=..\..\include\mod_brush.h
# End Source File
# Begin Source File

SOURCE=..\..\include\model.h
# End Source File
# Begin Source File

SOURCE=..\..\include\modelgen.h
# End Source File
# Begin Source File

SOURCE=..\..\include\pmove.h
# End Source File
# Begin Source File

SOURCE=..\..\include\pr_comp.h
# End Source File
# Begin Source File

SOURCE=..\..\include\qtypes.h
# End Source File
# Begin Source File

SOURCE=..\..\include\quakedef.h
# End Source File
# Begin Source File

SOURCE=..\..\include\r_explosion.h
# End Source File
# Begin Source File

SOURCE=..\..\include\sbar.h
# End Source File
# Begin Source File

SOURCE=..\..\include\spritegn.h
# End Source File
# Begin Source File

SOURCE=..\..\include\strlib.h
# End Source File
# Begin Source File

SOURCE=..\..\include\sys.h
# End Source File
# Begin Source File

SOURCE=..\..\include\teamplay.h
# End Source File
# Begin Source File

SOURCE=..\..\include\twiconfig.h
# End Source File
# Begin Source File

SOURCE=..\..\include\vid.h
# End Source File
# Begin Source File

SOURCE=..\..\include\view.h
# End Source File
# Begin Source File

SOURCE=..\..\include\wad.h
# End Source File
# Begin Source File

SOURCE=..\..\include\win32config.h
# End Source File
# Begin Source File

SOURCE=..\..\include\zone.h
# End Source File
# End Group
# End Target
# End Project
