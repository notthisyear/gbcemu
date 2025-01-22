#pragma once

#include "Shader.h"
#include <cstdint>
#include <glad/glad.h>


namespace gbcemu {

class ShaderProgram final {

  public:
    uint32_t shader_program_id;

    ShaderProgram();

    void activate();
    void attach_shader(Shader const &shader) const;
    bool link_program() const;

    ~ShaderProgram();
};
}