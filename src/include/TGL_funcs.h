/*
    funcs.h

    GL function defs.

    Copyright (C) 2001		Zephaniah E. Hull.

    Please refer to doc/copyright/GPL for terms of license.

    $Id$
*/

#ifndef __TGL_funcs_h
#define __TGL_funcs_h

#include "qtypes.h"
#include "TGL_types.h"
#include "TGL_defines.h"

#define TWIGL_NEED(ret, name, args)	extern ret (* q##name) args
#include "TGL_funcs_list.h"
#undef TWIGL_NEED

qboolean GLF_Init (void);

#endif // __TGL_funcs_h
