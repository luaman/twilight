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
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../base" /I "../client" /I "../fs" /I "../image" /I "../include" /I "../renderer" /I "../sound" /D "WIN32" /D "NDEBUG" /D "_LIB" /D "HAVE_SDL_H" /YX /FD /c
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
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../base" /I "../client" /I "../fs" /I "../image" /I "../include" /I "../renderer" /I "../sound" /D "WIN32" /D "_DEBUG" /D "_LIB" /D "HAVE_SDL_H" /YX /FD /GZ /c
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

SOURCE=.\entities.c
# End Source File
# Begin Source File

SOURCE=.\gen_textures.c
# End Source File
# Begin Source File

SOURCE=.\gl_alias.c
# End Source File
# Begin Source File

SOURCE=.\gl_arrays.c
# End Source File
# Begin Source File

SOURCE=.\gl_brush.c
# End Source File
# Begin Source File

SOURCE=.\gl_draw.c
# End Source File
# Begin Source File

SOURCE=.\gl_info.c
# End Source File
# Begin Source File

SOURCE=.\gl_light.c
# End Source File
# Begin Source File

SOURCE=.\gl_main.c
# End Source File
# Begin Source File

SOURCE=.\gl_sprite.c
# End Source File
# Begin Source File

SOURCE=.\gl_textures.c
# End Source File
# Begin Source File

SOURCE=.\liquid.c
# End Source File
# Begin Source File

SOURCE=.\menu.c
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

SOURCE=.\palette.c
# End Source File
# Begin Source File

SOURCE=.\pointers.c
# End Source File
# Begin Source File

SOURCE=.\r_explosion.c
# End Source File
# Begin Source File

SOURCE=.\r_part.c
# End Source File
# Begin Source File

SOURCE=.\sky.c
# End Source File
# Begin Source File

SOURCE=.\surface.c
# End Source File
# Begin Source File

SOURCE=.\video.c
# End Source File
# Begin Source File

SOURCE=.\vis.c
# End Source File
# End Group
# End Target
# End Project
