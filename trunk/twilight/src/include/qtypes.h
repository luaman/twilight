#ifndef _QTYPES_H
#define _QTYPES_H

typedef unsigned char byte;

#define _DEF_BYTE_

// KJB Undefined true and false defined in SciTech's DEBUG.H header
#undef true
#undef false

typedef enum { false, true } qboolean;

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef bound
#define bound(a,b,c) (max(a, min(b, c)))
#endif

#endif // _QTYPES_H
