#pragma once

#include "ShaderProgram.h"
#include "glad/glad.h"
#include <cstdint>
#include <memory>

namespace gbcemu {

class Renderer final {

  public:
    static uint8_t constexpr kBytesPerPixel{ 4 };

    Renderer(uint32_t const, uint32_t const);
    void init();
    void set_viewport(uint32_t const, uint32_t const);
    void update_framebuffer_and_draw(uint8_t *const);

  private:
    uint32_t m_texture_id;
    uint32_t m_vertex_array_object;
    uint32_t m_width;
    uint32_t m_height;

    std::unique_ptr<ShaderProgram> m_shader_program{ nullptr };

    GLenum m_format{ GL_RGBA };
    bool m_is_initialized{ false };
};
}