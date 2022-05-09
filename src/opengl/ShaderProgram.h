#pragma once

#include "Shader.h"
#include <glad/glad.h>
#include <string>

namespace gbcemu {

class ShaderProgram {

  public:
    unsigned int shader_program_id;

    ShaderProgram();

    void activate();
    void attach_shader(const Shader &shader) const;
    bool link_program() const;

    ~ShaderProgram();
};
}