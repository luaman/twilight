/*
	$RCSfile$

	Copyright (C) 1999-2000  Brian Paul, All Rights Reserved.

	Permission is hereby granted, free of charge, to any person obtaining a
	copy of this software and associated documentation files (the "Software"),
	to deal in the Software without restriction, including without limitation
	the rights to use, copy, modify, merge, publish, distribute, sublicense,
	and/or sell copies of the Software, and to permit persons to whom the
	Software is furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included
	in all copies or substantial portions of the Software.
	
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
	BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
	AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
	CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


	The Mesa OpenGL headers were originally adapted in 2001 for dynamic OpenGL
	binding by Zephaniah E. Hull and later rewritten by Joseph Carter.  This
	version of the file is for the generation 3 DynGL code, and has been
	adapted by Joseph Carter.  He and Zeph have decided to hereby disclaim all
	Copyright of this work.  It is released to the Public Domain WITHOUT ANY
	WARRANTY whatsoever, express or implied, in the hopes that others will use
	it instead of other less-evolved hacks which usually don't work right.  ;)

	$Id$
*/

#define DYNGL_DONT_NEED(ret, func, params)
#define DYNGL_DONT_EXT(ret, func, params, extension)
#define DYNGL_DONT_WANT(ret, func, params, alt)

#ifndef DYNGL_NEED
#error "dglfuncs.h included without DYNGL_NEED"
#endif

#ifndef DYNGL_EXT
#error "dglfuncs.h included without DYNGL_EXT"
#endif

#ifndef DYNGL_WANT
#error "dglfuncs.h included without DYNGL_WANT"
#endif

DYNGL_EXT (void, glActiveTextureARB, (GLenum), "GL_ARB_multitexture")
DYNGL_NEED (void, glAlphaFunc, (GLenum func, GLclampf ref))
DYNGL_NEED (void, glBegin, (GLenum mode))
DYNGL_NEED (void, glBindTexture, (GLenum target, GLuint texture))
DYNGL_NEED (void, glBlendFunc, (GLenum sfactor, GLenum dfactor))
DYNGL_NEED (void, glClear, (GLbitfield mask))
DYNGL_NEED (void, glClearColor, (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha))
DYNGL_EXT (void, glClientActiveTextureARB, (GLenum), "GL_ARB_multitexture")
DYNGL_NEED (void, glColor4f, (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha))
DYNGL_NEED (void, glColor4fv, (const GLfloat * v))
DYNGL_NEED (void, glColorPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid * ptr))
DYNGL_NEED (void, glCullFace, (GLenum mode))
DYNGL_NEED (void, glDeleteTextures, (GLsizei n, const GLuint * textures))
DYNGL_NEED (void, glDepthFunc, (GLenum func))
DYNGL_NEED (void, glDepthMask, (GLboolean flag))
DYNGL_NEED (void, glDepthRange, (GLclampd near_val, GLclampd far_val))
DYNGL_NEED (void, glDisable, (GLenum cap))
DYNGL_NEED (void, glDisableClientState, (GLenum cap))
DYNGL_NEED (void, glDrawArrays, (GLenum mode, GLint first, GLsizei count))
DYNGL_NEED (void, glDrawBuffer, (GLenum mode))
DYNGL_NEED (void, glDrawElements, (GLenum mode, GLsizei count, GLenum type, const GLvoid * indices))
DYNGL_WANT (void, glDrawRangeElements, (GLenum, GLuint, GLuint, GLsizei, GLenum, const GLvoid *), Alt_glDrawRangeElements)
DYNGL_EXT (void, glDrawRangeElementsEXT, (GLenum, GLuint, GLuint, GLsizei, GLenum, const GLvoid *), "GL_EXT_draw_range_elements")
DYNGL_NEED (void, glEnable, (GLenum cap))
DYNGL_NEED (void, glEnableClientState, (GLenum cap))
DYNGL_NEED (void, glEnd, (void))
DYNGL_NEED (void, glFinish, (void))
DYNGL_NEED (void, glFlush, (void))
DYNGL_NEED (GLenum, glGetError, ())
DYNGL_NEED (void, glFrustum, (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val))
DYNGL_NEED (void, glGenTextures, (GLsizei n, GLuint * textures))
DYNGL_NEED (void, glGetIntegerv, (GLenum pname, GLint * params))
DYNGL_NEED (const GLubyte *, glGetString, (GLenum name))
DYNGL_NEED (void, glHint, (GLenum target, GLenum mode))
DYNGL_NEED (void, glLoadIdentity, (void))
DYNGL_EXT (void, glLockArraysEXT, (GLint, GLsizei), "GL_EXT_compiled_vertex_array")
DYNGL_NEED (void, glMatrixMode, (GLenum mode))
DYNGL_WANT (void, glLoadTransposeMatrixf, (const GLfloat * m), Alt_glLoadTransposeMatrixf)
DYNGL_WANT (void, glMultTransposeMatrixf, (const GLfloat * m), Alt_glMultTransposeMatrixf)
DYNGL_EXT (void, glLoadTransposeMatrixfARB, (const GLfloat * m), "GL_ARB_transpose_matrix")
DYNGL_EXT (void, glMultTransposeMatrixfARB, (const GLfloat * m), "GL_ARB_transpose_matrix")
DYNGL_NEED (void, glLoadMatrixf, (const GLfloat * m))
DYNGL_NEED (void, glMultMatrixf, (const GLfloat * m))
DYNGL_NEED (void, glOrtho, (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val))
DYNGL_NEED (void, glPolygonMode, (GLenum face, GLenum mode))
DYNGL_NEED (void, glPopMatrix, (void))
DYNGL_NEED (void, glPushMatrix, (void))
DYNGL_NEED (void, glReadPixels, (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid * pixels))
DYNGL_NEED (void, glRotatef, (GLfloat angle, GLfloat x, GLfloat y, GLfloat z))
DYNGL_NEED (void, glTexCoordPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid * ptr))
DYNGL_NEED (void, glTexEnvi, (GLenum target, GLenum pname, GLint param))
DYNGL_NEED (void, glTexImage2D, (GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid * pixels))
DYNGL_NEED (void, glTexParameteri, (GLenum target, GLenum pname, GLint param))
DYNGL_NEED (void, glTexSubImage2D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * pixels))
DYNGL_NEED (void, glTranslatef, (GLfloat x, GLfloat y, GLfloat z))
DYNGL_EXT (void, glUnlockArraysEXT, (void), "GL_EXT_compiled_vertex_array")
DYNGL_NEED (void, glVertex2f, (GLfloat x, GLfloat y))
DYNGL_NEED (void, glVertexPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid * ptr))
DYNGL_NEED (void, glViewport, (GLint x, GLint y, GLsizei width, GLsizei height))

DYNGL_EXT (void, glCombinerParameterfvNV, (GLenum pname, const GLfloat * params), "GL_NV_register_combiners")
DYNGL_EXT (void, glCombinerInputNV, (GLenum stage, GLenum portion, GLenum variable, GLenum input, GLenum mapping, GLenum componentUsage), "GL_NV_register_combiners")
DYNGL_EXT (void, glCombinerOutputNV, (GLenum stage, GLenum portion, GLenum abOutput, GLenum cdOutput, GLenum sumOutput, GLenum scale, GLenum bias, GLboolean abDotProduct, GLboolean cdDotProduct, GLboolean muxSum), "GL_NV_register_combiners")
DYNGL_EXT (void, glFinalCombinerInputNV, (GLenum variable, GLenum input, GLenum mapping, GLenum componentUsage), "GL_NV_register_combiners")

DYNGL_EXT (void, glBindBufferARB, (GLenum target, GLuint buffer), "GL_ARB_vertex_buffer_object")
DYNGL_EXT (void, glDeleteBuffersARB, (GLsizei n, const GLuint * buffers), "GL_ARB_vertex_buffer_object")
DYNGL_EXT (void, glGenBuffersARB, (GLsizei n, GLuint * buffers), "GL_ARB_vertex_buffer_object")
DYNGL_EXT (void, glBufferDataARB, (GLenum target, GLsizeiptrARB size, const GLvoid * data, GLenum usage), "GL_ARB_vertex_buffer_object")
DYNGL_EXT (void, glBufferSubDataARB, (GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid * data), "GL_ARB_vertex_buffer_object")
DYNGL_EXT (void, glGetBufferSubDataARB, (GLenum target, GLintptrARB offset, GLsizeiptrARB size, GLvoid * data), "GL_ARB_vertex_buffer_object")
DYNGL_EXT (GLvoid*, glMapBufferARB, (GLenum target, GLenum access), "GL_ARB_vertex_buffer_object")
DYNGL_EXT (GLboolean, glUnmapBufferARB, (GLenum target), "GL_ARB_vertex_buffer_object")
DYNGL_EXT (void, glGetBufferParameterivARB, (GLenum target, GLenum pname, GLint * params), "GL_ARB_vertex_buffer_object")
DYNGL_EXT (void, glGetBufferPointervARB, (GLenum target, GLenum pname, GLvoid *params), "GL_ARB_vertex_buffer_object")

#undef DYNGL_DONT_NEED
#undef DYNGL_DONT_EXT
#undef DYNGL_DONT_WANT

