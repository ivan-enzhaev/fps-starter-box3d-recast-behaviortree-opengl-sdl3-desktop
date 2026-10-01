#pragma once

#include <glad/glad.h>

extern const char *vertexShaderSource;
extern const char *fragmentShaderSource;
extern const char *lineVertexShaderSource;
extern const char *lineFragmentShaderSource;

GLuint createShaderProgram(const char *vertexSrc, const char *fragmentSrc);
