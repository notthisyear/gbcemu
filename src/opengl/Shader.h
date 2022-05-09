#pragma once

#include <glad/glad.h>
#include <string>

namespace gbcemu {

class Shader {

  public:
    enum class ShaderType { Vertex, Fragment };
    uint32_t shader_id;
    Shader::ShaderType type;

    bool try_construct_shader(Shader::ShaderType, const std::string &);

    ~Shader();

  private:
    GLenum m_shader_type;

    void set_shader_type(Shader::ShaderType type);
};
}