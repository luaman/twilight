# Microsoft Developer Studio Project File - Name="twilight_renderer" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=twilight_renderer - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "twilight_renderer.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "twilight_renderer.mak" CFG="twilight_renderer - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "twilight_renderer - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "twilight_renderer - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "twilight_renderer - Win32 Release"

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
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../include" /I "../image" /I "../../SDL/include" /D "WIN32" /D "NDEBUG" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x40c /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "twilight_renderer - Win32 Debug"

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
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../include" /I "../image" /I "../../SDL/include" /D "WIN32" /D "_DEBUG" /D "_LIB" /YX /FD /GZ /c
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

# Name "twilight_renderer - Win32 Release"
# Name "twilight_renderer - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\dyngl.c
# End Source File
# Begin Source File

SOURCE=.\gl_alias.c
# End Source File
# Begin Source File

SOURCE=.\gl_alias.h
# End Source File
# Begin Source File

SOURCE=.\gl_arrays.c
# End Source File
# Begin Source File

SOURCE=.\gl_info.c
# End Source File
# Begin Source File

SOURCE=.\gl_textures.c
# End Source File
# Begin Source File

SOURCE=.\liquid.c
# End Source File
# Begin Source File

SOURCE=.\mod_alias.c
# End Source File
# Begin Source File

SOURCE=.\mod_brush.c
# End Source File
# Begin Source File

SOURCE=.\mod_sprite.c
# End Source File
# Begin Source File

SOURCE=.\noise.c
# End Source File
# Begin Source File

SOURCE=.\noise_textures.c
# End Source File
# Begin Source File

SOURCE=.\pointers.c
# End Source File
# Begin Source File

SOURCE=.\sky.c
# End Source File
# Begin Source File

SOURCE=.\surface.c
# End Source File
# Begin Source File

SOURCE=.\vis.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\dglfuncs.h
# End Source File
# Begin Source File

SOURCE=.\dyngl.h
# End Source File
# Begin Source File

SOURCE=.\gl_arrays.h
# End Source File
# Begin Source File

SOURCE=.\gl_info.h
# End Source File
# Begin Source File

SOURCE=.\gl_textures.h
# End Source File
# Begin Source File

SOURCE=.\glquake.h
# End Source File
# Begin Source File

SOURCE=.\light.h
# End Source File
# Begin Source File

SOURCE=.\liquid.h
# End Source File
# Begin Source File

SOURCE=.\mod_alias.h
# End Source File
# Begin Source File

SOURCE=.\mod_brush.h
# End Source File
# Begin Source File

SOURCE=.\mod_sprite.h
# End Source File
# Begin Source File

SOURCE=.\pointers.h
# End Source File
# Begin Source File

SOURCE=.\r_part.c
# End Source File
# Begin Source File

SOURCE=.\r_part.h
# End Source File
# Begin Source File

SOURCE=.\sky.h
# End Source File
# Begin Source File

SOURCE=.\surface.h
# End Source File
# Begin Source File

SOURCE=.\vis.h
# End Source File
# End Group
# End Target
# End Project
