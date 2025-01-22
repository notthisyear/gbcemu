#include "Shader.h"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <fstream>
#include <iostream>
#include <sstream>

namespace gbcemu {
bool Shader::try_construct_shader(Shader::ShaderType const type, std::string const &shader_source_path) {

    set_shader_type(type);

    std::ifstream shader_file;
    shader_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    std::string shader_code;
    try {
        shader_file.open(shader_source_path);
        std::stringstream shader_stream;

        shader_stream << shader_file.rdbuf();
        shader_file.close();

        shader_code = shader_stream.str();
    }

    catch (std::ifstream::failure const &e) {
        LogUtilities::log_error(std::cout, GeneralUtilities::formatted_string("Failed to read shader file '%s'", shader_source_path));
        return false;
    }

    char const *const shader_source{ shader_code.c_str() };

    uint32_t const shader{ glCreateShader(m_shader_type) };
    glShaderSource(shader, 1, &shader_source, NULL);
    glCompileShader(shader);

    int32_t shaderCompilationResult;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &shaderCompilationResult);

    if (!shaderCompilationResult) {
        int32_t infoLogLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);
        GLchar *const infoLog{ new GLchar[infoLogLength + 1] };
        glGetShaderInfoLog(shader, infoLogLength, NULL, infoLog);
        LogUtilities::log_error(std::cout, GeneralUtilities::formatted_string("Shader compilation failed - %s", infoLog));
        delete[] infoLog;
    }

    shader_id = (shaderCompilationResult != 0) ? shader : 0U;
    return shaderCompilationResult != 0;
}

void Shader::set_shader_type(Shader::ShaderType const type) {
    switch (type) {

    case ShaderType::Vertex:
        m_shader_type = GL_VERTEX_SHADER;
        break;
    case ShaderType::Fragment:
        m_shader_type = GL_FRAGMENT_SHADER;
        break;
    default:
        throw std::invalid_argument("Unsupported shader type");
    }
}

Shader::~Shader() {
    if (shader_id != 0) {
        glDeleteShader(shader_id);
    }
}
}