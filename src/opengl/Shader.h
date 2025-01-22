#pragma once

#include <cstdint>
#include <glad/glad.h>
#include <string>


namespace gbcemu {

class Shader final {

  public:
    enum class ShaderType { Vertex, Fragment };
    uint32_t shader_id;
    Shader::ShaderType type;

    bool try_construct_shader(Shader::ShaderType const, std::string const &);

    ~Shader();

  private:
    GLenum m_shader_type;

    void set_shader_type(Shader::ShaderType const type);
};
}