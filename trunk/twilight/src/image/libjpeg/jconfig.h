#define HAVE_PROTOTYPES
#define HAVE_UNSIGNED_CHAR
#define HAVE_UNSIGNED_SHORT
// #define void char
// #define const
#define CHAR_IS_UNSIGNED
#define HAVE_STDDEF_H
#define HAVE_STDLIB_H
#undef NEED_BSD_STRINGS
#undef NEED_SYS_TYPES_H
#undef NEED_FAR_POINTERS		// we presume a 32-bit flat memory model
#undef NEED_SHORT_EXTERNAL_NAMES
#undef INCOMPLETE_TYPES_BROKEN

#define JDCT_DEFAULT  JDCT_FLOAT
#define JDCT_FASTEST  JDCT_FLOAT

#ifdef JPEG_INTERNALS

#undef RIGHT_SHIFT_IS_UNSIGNED

#endif /* JPEG_INTERNALS */

#ifdef JPEG_CJPEG_DJPEG

// These defines indicate which image (non-JPEG) file formats are allowed.
#define BMP_SUPPORTED		// BMP image file format
#define GIF_SUPPORTED		// GIF image file format
#define PPM_SUPPORTED		// PBMPLUS PPM/PGM image file format
#undef RLE_SUPPORTED		// Utah RLE image file format
#define TARGA_SUPPORTED		// Targa image file format

#undef TWO_FILE_COMMANDLINE
#define USE_SETMODE
#undef NEED_SIGNAL_CATCHER
#undef DONT_USE_B_MODE
#undef PROGRESS_REPORT

#endif /* JPEG_CJPEG_DJPEG */
