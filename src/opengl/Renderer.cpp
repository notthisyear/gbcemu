#include "Renderer.h"
#include "Shader.h"
#include "opengl/ShaderProgram.h"
#include <iostream>
#include <memory>

namespace gbcemu {

Renderer::Renderer(uint32_t width, uint32_t height) : m_width(width), m_height(height) {
    m_format = GL_RGBA;
    m_is_initialized = false;
}

void Renderer::init() {
    glCreateTextures(GL_TEXTURE_2D, 1, &m_texture_id);
    glTextureStorage2D(m_texture_id, 1, GL_RGBA8, m_width, m_height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    float vertices[] = {
        // clang-format off
        // Positions       // Texture coordinates
        1.0f,  1.0f,  0.0f, 1.0f, 0.0f,
        1.0f,  -1.0f, 0.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, 1.0f,  0.0f, 0.0f, 0.0f
        // clang-format on
    };

    uint32_t indices[] = {
        0, 1, 2, // First triangle
        2, 3, 0  // Second triangle
    };

    uint32_t vertexBufferObject, elementBufferObject;

    glGenVertexArrays(1, &m_vertex_array_object);
    glGenBuffers(1, &vertexBufferObject);
    glGenBuffers(1, &elementBufferObject);

    // Bind vertex array before bind buffer as setting attributes, as these settings are stored in the vertex array object
    glBindVertexArray(m_vertex_array_object);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBufferObject);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // The 0 and 1 is related to the location set in the vertex shader source code
    // Specifiy position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    // Specifiy texture attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Unbind current buffers (not strictly neccessary, but ensures that nothing more can happen to the VAO)
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Compile and set shaders
    Shader *vertexShader = new Shader();
    Shader *fragmentShader = new Shader();

    if (!vertexShader->try_construct_shader(Shader::ShaderType::Vertex, "D:\\programming\\gbcemu\\src\\opengl\\shaders\\vertex.vs") ||
        !fragmentShader->try_construct_shader(Shader::ShaderType::Fragment, "D:\\programming\\gbcemu\\src\\opengl\\shaders\\frag.fs")) {
        exit(1);
    }

    m_shader_program = std::make_unique<ShaderProgram>();
    m_shader_program->attach_shader(*vertexShader);
    m_shader_program->attach_shader(*fragmentShader);

    if (!m_shader_program->link_program()) {
        exit(1);
    }

    delete fragmentShader;
    delete vertexShader;

    m_is_initialized = true;
}

void Renderer::set_viewport(uint32_t width, uint32_t height) { glViewport(0, 0, width, height); }

void Renderer::update_framebuffer_and_draw(uint8_t *buffer) {
    if (!m_is_initialized)
        exit(1);

    glTextureSubImage2D(m_texture_id, 0, 0, 0, m_width, m_height, m_format, GL_UNSIGNED_BYTE, buffer);

    m_shader_program->activate();

    glBindTexture(GL_TEXTURE_2D, m_texture_id);
    glBindVertexArray(m_vertex_array_object);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

}
