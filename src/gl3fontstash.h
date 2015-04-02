//
// Copyright (c) 2009-2013 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
// Using VAO and VBO for OpenGL 3+ compatibility if GLFONTSTASH_IMPLEMENTATION is defined
// glfonsProjection() must be called to set a projection matrix as GL_PROJECTION is deprecated in OGL 3.
// 
// gl3fonsProjection() has been changed to take additional params for screen width and height to
// calculate a top left projection matrix

#ifndef GLFONTSTASH_H
#define GLFONTSTASH_H

FONScontext* gl3fonsCreate(int width, int height, int flags);
void gl3fonsDelete(FONScontext* ctx);
void gl3fonsProjection(FONScontext* ctx, GLfloat *mat, int screenWidth, int screenHeight);

unsigned int gl3fonsRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

#endif

#ifdef GLFONTSTASH_IMPLEMENTATION

struct GLFONScontext 
{
	GLuint tex;
	GLuint vao;					// Vertex Array Object
	GLuint vbo;					// Vertex Buffer Object
	GLuint shader;				// Shader program
	GLfloat	projMat[16];		// Projection matrix
	GLuint texture_uniform;		// Uniform for texture sampler
	GLuint projMat_uniform;		// Uniform for projection matrix
	int width, height;
};
typedef struct GLFONScontext GLFONScontext;

char vertexShaderText[] = "#version 330\r\n\
	\r\n\
	uniform mat4 projMat;\r\n\
	\r\n\
	layout(location = 0) in vec2 vert;\r\n\
	layout(location = 1) in vec2 coord;\r\n\
	layout(location = 2) in vec4 color;\r\n\
	\r\n\
	out vec2 T;\r\n\
	out vec4 C;\r\n\
	\r\n\
	void main() {\r\n\
		gl_Position = projMat * vec4(vert.x,vert.y, 1.0, 1.0);\r\n\
		T = coord;\r\n\
		C = color;\r\n\
	}";

char fragmentShaderText[] = "#version 330\r\n\
	\r\n\
	out vec4 Color;\r\n\
	\r\n\
	in vec2 T;\r\n\
	in vec4 C;\r\n\
	\r\n\
	uniform sampler2D texture0;\r\n\
	\r\n\
	void main() {\r\n\
		Color = C;\r\n\
		Color.a = texture(texture0, T).r;\r\n\
	}";

static GLuint shader() 
{
	GLint compiled = 0;
	GLint linked = 0;
	const GLchar* stringptrs[1];
	
	// Create the shaders
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	// Load and compile the vertex shader
	stringptrs[0] = vertexShaderText;
	glShaderSource(vertexShader, 1, stringptrs, 0);
	glCompileShader(vertexShader);

	// Check compile status
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &compiled);
	if (!compiled) 
	{
		GLint len = 0;
		glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH , &len); 

		if (len > 1) 
		{
			GLchar* compiler_log = (GLchar*)malloc(len);
			glGetShaderInfoLog(vertexShader, len, 0, compiler_log);	
			free (compiler_log);
		}

		return 0;
	}

	// Load and compile the fragment shader
	stringptrs[0] = fragmentShaderText;
	glShaderSource(fragmentShader, 1, stringptrs, 0);
	glCompileShader(fragmentShader);

	// Check compile status
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &compiled);
	if (!compiled) 
	{
		GLint len = 0;
		glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH , &len); 

		if (len > 1) 
		{
			GLchar* compiler_log = (GLchar*)malloc(len);
			glGetShaderInfoLog(fragmentShader, len, 0, compiler_log);	
			free (compiler_log);
		}

		return 0;
	}
	
	// Setup and link shader program
	GLuint programID = glCreateProgram();
	glAttachShader(programID, vertexShader);
	glAttachShader(programID, fragmentShader);
	glLinkProgram(programID);
	
	glGetProgramiv(programID, GL_LINK_STATUS, &linked);		
	if (!linked) 
	{
		GLint len = 0;
		glGetProgramiv(programID, GL_INFO_LOG_LENGTH , &len); 

		if (len > 1) 
		{
			GLchar* compiler_log = (GLchar*)malloc(len);	
			glGetProgramInfoLog(programID, len, 0, compiler_log);				
			free (compiler_log);
		} 

		return 0;
	}

	// Cleanup
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	
	return programID;
}

static int gl3fons__renderCreate(void* userPtr, int width, int height)
{
	GLFONScontext* gl = (GLFONScontext*)userPtr;
	// Create may be called multiple times, delete existing texture.
	if (gl->tex != 0) 
	{
		glDeleteTextures(1, &gl->tex);
		gl->tex = 0;
	}
	glGenTextures(1, &gl->tex);
	if (!gl->tex)
	{
		return 0;
	}

	// Create shader
	if (gl->shader != 0) 
	{
		glDeleteProgram(gl->shader);
		gl->shader = 0;
	}
	gl->shader = shader();
	if (!gl->shader)
	{
		return 0;
	}

	// Get uniforms
	gl->texture_uniform = glGetUniformLocation(gl->shader, "texture0");
	gl->projMat_uniform = glGetUniformLocation(gl->shader, "projMat");
	
	// Setup projection matrix as an identity matrix
	for (int i = 0; i < 16; i++)
	{
		gl->projMat[i] = 0.0;
	}
	gl->projMat[0] = 1.0;
	gl->projMat[5] = 1.0;
	gl->projMat[10] = 1.0;
	gl->projMat[15] = 1.0;
	
	// Create VAO
	if (gl->vao != 0) 
	{
		glDeleteVertexArrays(1, &gl->vao);
		gl->vao = 0;
	}
	glGenVertexArrays(1, &gl->vao);
	if (!gl->vao)
	{
		return 0;
	}
	
	// Create VBO
	if (gl->vbo != 0) 
	{
		glDeleteBuffers(1, &gl->vbo);
		gl->vbo = 0;
	}
	glGenBuffers(1, &gl->vbo);
	if (!gl->vbo)
	{
		return 0;
	}

	gl->width = width;
	gl->height = height;
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gl->tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, gl->width, gl->height, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	return 1;
}

static int gl3fons__renderResize(void* userPtr, int width, int height)
{
	// Reuse create to resize.
	return gl3fons__renderCreate(userPtr, width, height);
}

static void gl3fons__renderUpdate(void* userPtr, int* rect, const unsigned char* data)
{
	GLFONScontext* gl = (GLFONScontext*)userPtr;

	if (gl->tex == 0)
	{
		return;
	}

	int w = rect[2] - rect[0];
	int h = rect[3] - rect[1];

	glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
	glBindTexture(GL_TEXTURE_2D, gl->tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, gl->width);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, rect[0]);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, rect[1]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, rect[0], rect[1], w, h, GL_RED,GL_UNSIGNED_BYTE, data);
	glPopClientAttrib();
}

static void gl3fons__renderDraw(void* userPtr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts)
{
	GLFONScontext* gl = (GLFONScontext*)userPtr;

	if (gl->tex == 0 || gl->shader == 0 || gl->vao == 0 || gl->vbo == 0)
	{
		return;
	}

	glEnable(GL_TEXTURE_2D);
	
	// Init shader
	glUseProgram(gl->shader);

	// Init texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gl->tex);
	glUniform1i(gl->texture_uniform, 0);
	
	// Init projection matrix
	glUniformMatrix4fv(gl->projMat_uniform, 1, false, gl->projMat);
	
	// Bind vao
	glBindVertexArray(gl->vao);
	
	// Setup buffer
	glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
	glBufferData(GL_ARRAY_BUFFER, (2 * sizeof(float) * 2 * nverts) + (sizeof(int) * nverts), NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * 2 * nverts	, verts);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(float) * 2 * nverts, sizeof(float) * 2 * nverts	, tcoords);
	glBufferSubData(GL_ARRAY_BUFFER, 2 * sizeof(float) * 2 * nverts, sizeof(int) * nverts, colors);
	
	// Setup attributes
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void *) (sizeof(float) * 2 * nverts));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(int), (void *) (2 * sizeof(float) * 2 * nverts));
	
	glDrawArrays(GL_TRIANGLES, 0, nverts);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	glBindVertexArray(0);
	glUseProgram(0);
}

static void gl3fons__renderDelete(void* userPtr)
{
	GLFONScontext* gl = (GLFONScontext*)userPtr;
	if (gl->tex != 0)
	{
		glDeleteTextures(1, &gl->tex);
		gl->tex = 0;
	}
	
	gl->texture_uniform = 0;
	gl->projMat_uniform = 0;
	if (gl->shader != 0) 
	{
		glDeleteProgram(gl->shader);
		gl->shader = 0;
	}
	if (gl->vao != 0) 
	{
		glDeleteVertexArrays(1, &gl->vao);
		gl->vao = 0;
	}	
	if (gl->vbo != 0) 
	{
		glDeleteBuffers(1, &gl->vbo);
		gl->vbo = 0;
	}
	
	free(gl);
}

FONScontext* gl3fonsCreate(int width, int height, int flags)
{
	FONSparams params;
	GLFONScontext* gl;

	gl = (GLFONScontext*)malloc(sizeof(GLFONScontext));
	if (gl == NULL)
	{
		goto error;
	}
	memset(gl, 0, sizeof(GLFONScontext));

	memset(&params, 0, sizeof(params));
	params.width = width;
	params.height = height;
	params.flags = (unsigned char)flags;
	params.renderCreate = gl3fons__renderCreate;
	params.renderResize = gl3fons__renderResize;
	params.renderUpdate = gl3fons__renderUpdate;
	params.renderDraw = gl3fons__renderDraw; 
	params.renderDelete = gl3fons__renderDelete;
	params.userPtr = gl;

	return fonsCreateInternal(&params);

error:
	if (gl != NULL) free(gl);
	return NULL;
}

void gl3fonsDelete(FONScontext* ctx)
{
	fonsDeleteInternal(ctx);
}

// Keeping in mind that in OpenGL 0,0 is the bottom left corner of your screen but for text 
// it makes more sense for 0,0 to be the top left this would calculate a usable projection matrix:
// memset(mat, 0, 16 * sizeof(GLfloat));
// mat[0] = 2.0 / screenwidth;
// mat[5] = -2.0 / screenheight;
// mat[10] = 2.0;
// mat[12] = -1.0;
// mat[13] = 1.0;
// mat[14] = -1.0;
// mat[15] = 1.0;
void gl3fonsProjection(FONScontext* ctx, GLfloat *mat, int screenWidth, int screenHeight)
{
	mat[0] = 2.0f / screenWidth;
	mat[5] = -2.0f / screenHeight;
	mat[10] = 2.0f;
	mat[12] = -1.0f;
	mat[13] = 1.0f;
	mat[14] = -1.0f;
	mat[15] = 1.0f;

	GLFONScontext* gl = (GLFONScontext*)(ctx->params.userPtr);

	for (int i = 0; i < 16; i++) 
	{
		gl->projMat[i] = mat[i];
	}
}

unsigned int gl3fonsRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	return (r) | (g << 8) | (b << 16) | (a << 24);
}

#endif
