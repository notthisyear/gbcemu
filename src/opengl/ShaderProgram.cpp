#include "ShaderProgram.h"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <glad/glad.h>
#include <iostream>

namespace gbcemu {

ShaderProgram::ShaderProgram() { shader_program_id = glCreateProgram(); }

void ShaderProgram::activate() { glUseProgram(shader_program_id); }

void ShaderProgram::attach_shader(Shader const &shader) const { glAttachShader(shader_program_id, shader.shader_id); }

bool ShaderProgram::link_program() const {
    glLinkProgram(shader_program_id);
    int32_t shaderLinkingResult;
    glGetProgramiv(shader_program_id, GL_LINK_STATUS, &shaderLinkingResult);

    if (!shaderLinkingResult) {
        int32_t infoLogLength;
        glGetShaderiv(shader_program_id, GL_INFO_LOG_LENGTH, &infoLogLength);
        GLchar *const infoLog{ new GLchar[infoLogLength + 1] };
        glGetProgramInfoLog(shader_program_id, infoLogLength, NULL, infoLog);

        LogUtilities::log_error(std::cout, GeneralUtilities::formatted_string("Shader linking failed - %s", infoLog));
        delete[] infoLog;
    }

    return shaderLinkingResult != 0;
}

ShaderProgram::~ShaderProgram() {
    if (shader_program_id > 0) {
        glDeleteProgram(shader_program_id);
    }
}
}