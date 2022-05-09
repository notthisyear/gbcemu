#pragma once

#include "ShaderProgram.h"
#include "glad/glad.h"
#include <memory>
#include <stdint.h>

namespace gbcemu {

class Renderer {

  public:
    const uint8_t BytesPerPixel = 4;

    Renderer(uint32_t, uint32_t);
    void init();
    void set_viewport(uint32_t, uint32_t);
    void update_framebuffer_and_draw(uint8_t *);

  private:
    GLenum m_format;
    std::unique_ptr<ShaderProgram> m_shader_program;

    uint32_t m_texture_id;
    uint32_t m_vertex_array_object;
    uint32_t m_width;
    uint32_t m_height;

    bool m_is_initialized;
};
}