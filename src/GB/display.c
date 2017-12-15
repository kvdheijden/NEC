/* MIT License
 *
 * Copyright (c) 2017 Koen van der Heijden.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "display.h"

#include <GL/glew.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "GB.h"

static GLuint _vao;
static GLuint _vbo[2];
static GLuint _ibo;
static GLuint _texture;
static GLuint _shader_program;
static GLint _texture_uniform_location;

#define NUM_VERTICES    4
#define NUM_INDICES     6

static const struct _vertex_position {
    GLfloat x;
    GLfloat y;
} _vertex_position_data[NUM_VERTICES] = {
        {.x = -1.0f, .y = 1.0f},
        {.x = 1.0f, .y = 1.0f},
        {.x = 1.0f, .y = -1.0f},
        {.x = -1.0f, .y = -1.0f}
};

static const struct _vertex_uv {
    GLuint s;
    GLuint t;
} _vertex_uv_data[NUM_VERTICES] = {
        {.s = 0, .t = 0},
        {.s = DISPLAY_WIDTH, .t = 0},
        {.s = DISPLAY_WIDTH, .t = DISPLAY_HEIGHT},
        {.s = 0, .t = DISPLAY_HEIGHT}
};

static const GLubyte _vertex_indices[NUM_INDICES] = {
        0, 1, 2,
        2, 3, 0
};

static GLuint load_shader(GLenum type, const char *shader_source)
{
    GLint success, max_length;
    GLuint _shader = glCreateShader(type);
    GLchar *buffer = 0;
    glShaderSource(_shader, 1, (const GLchar *const *) &shader_source, NULL);
    free(buffer);

    glCompileShader(_shader);
    glGetShaderiv(_shader, GL_COMPILE_STATUS, &success);
    if(success == GL_FALSE) {
        glGetShaderiv(_shader, GL_INFO_LOG_LENGTH, &max_length);
        buffer = malloc((max_length * sizeof(GLchar)) + 1);
        buffer[max_length] = 0;
        glGetShaderInfoLog(_shader, max_length, &max_length, buffer);
        log_error(buffer);
        free(buffer);
        GB_exit();
    }
    return _shader;
}

void display_setup(void)
{
    glewExperimental = true;
    if(glewInit() != GLEW_OK) {
        log_error("Error loading GLEW\n");
        exit(EXIT_FAILURE);
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Build VAO
    glGenVertexArrays(1, &_vao);
    glBindVertexArray(_vao);

    // Build VBOs
    glGenBuffers(2, _vbo);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(_vertex_position_data), _vertex_position_data, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, _vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(_vertex_uv_data), _vertex_uv_data, GL_DYNAMIC_DRAW);

    // Build IBO
    glGenBuffers(1, &_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(_vertex_indices), _vertex_indices, GL_STATIC_DRAW);

    // Build texture
    glGenTextures(1, &_texture);

    // Bind texture
    glBindTexture(GL_TEXTURE_2D, _texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA, TEXTURE_DIMENSION, TEXTURE_DIMENSION, 0, GL_RGBA, GL_FLOAT, NULL);

    // Set texture options
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Build Shaders
    GLint success, max_length;
    GLchar *log = 0;
    GLuint _vertex_shader = load_shader(GL_VERTEX_SHADER,
                                        "#version 330\n"
                                                "\n"
                                                "layout(location = 0) in vec2 position;\n"
                                                "layout(location = 1) in vec2 uvCoord;\n"
                                                "\n"
                                                "out vec2 vTexCoord;\n"
                                                "\n"
                                                "//uniform mat4 uModelViewProjectionMatrix;\n"
                                                "\n"
                                                "void main() {\n"
                                                "    // Apply MVP to in-vector\n"
                                                "    //gl_Position = uModelViewProjectionMatrix * vec4(position.xy, 0.0f, 1.0f);\n"
                                                "    gl_Position = vec4(position.xy, 0.0f, 1.0f);\n"
                                                "\n"
                                                "    // Pass texture coord to Fragment shader\n"
                                                "    vTexCoord = uvCoord / 256.0f;\n"
                                                "}"
    );
    GLuint _fragment_shader = load_shader(GL_FRAGMENT_SHADER,
                                          "#version 330\n"
                                                  "\n"
                                                  "in vec2 vTexCoord;\n"
                                                  "\n"
                                                  "out vec4 fColor;\n"
                                                  "\n"
                                                  "uniform sampler2D uTexture;\n"
                                                  "\n"
                                                  "void main() {\n"
                                                  "    fColor = texture(uTexture, vTexCoord);\n"
                                                  "}"
    );

    _shader_program = glCreateProgram();
    glAttachShader(_shader_program, _vertex_shader);
    glAttachShader(_shader_program, _fragment_shader);
    glLinkProgram(_shader_program);
    glGetProgramiv(_shader_program, GL_LINK_STATUS, &success);
    if(success == GL_FALSE) {
        glGetProgramiv(_shader_program, GL_INFO_LOG_LENGTH, &max_length);
        log = malloc(max_length * sizeof(GLchar));
        glGetProgramInfoLog(_shader_program, max_length, &max_length, log);
        log_error(log);
        free(log);
        return;
    }

    glUseProgram(_shader_program);

    _texture_uniform_location = glGetUniformLocation(_shader_program, "uTexture");

    // Delete shaders, they are linked already
    glDetachShader(_shader_program, _vertex_shader);
    glDeleteShader(_vertex_shader);
    glDetachShader(_shader_program, _fragment_shader);
    glDeleteShader(_fragment_shader);
}

void display_frame(struct display *_display)
{
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEXTURE_DIMENSION, TEXTURE_DIMENSION, 0, GL_RGBA, GL_FLOAT, _display);
    glUniform1i(_texture_uniform_location, 0);

    glBindBuffer(GL_ARRAY_BUFFER, _vbo[0]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(struct _vertex_position), 0);

    glBindBuffer(GL_ARRAY_BUFFER, _vbo[1]);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_UNSIGNED_INT, GL_FALSE, sizeof(struct _vertex_uv), 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ibo);
    glDrawElements(GL_TRIANGLES, NUM_INDICES, GL_UNSIGNED_BYTE, 0);

    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
}

void display_teardown(void)
{
    // Delete shaders
    glUseProgram(0);
    glDeleteProgram(_shader_program);

    // Delete textures
    glDeleteTextures(1, &_texture);

    // Delete IBO
    glDeleteBuffers(1, &_ibo);

    // Delete VBO
    glDeleteBuffers(2, _vbo);

    // Delete VAO
    glDeleteVertexArrays(1, &_vao);
}