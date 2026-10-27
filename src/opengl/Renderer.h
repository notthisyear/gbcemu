#pragma once

#include "ShaderProgram.h"
#include "glad/glad.h"
#include <cstdint>
#include <memory>

namespace gbcemu {

class Renderer final {

  public:
    static std::uint8_t constexpr kBytesPerPixel{ 4 };

    Renderer(std::uint32_t const, std::uint32_t const);
    void init();
    void set_viewport(std::uint32_t const, std::uint32_t const);
    void update_framebuffer_and_draw(std::uint8_t *const);

  private:
    std::uint32_t m_texture_id;
    std::uint32_t m_vertex_array_object;
    std::uint32_t m_width;
    std::uint32_t m_height;

    std::unique_ptr<ShaderProgram> m_shader_program{ nullptr };

    GLenum m_format{ GL_RGBA };
    bool m_is_initialized{ false };
};
}