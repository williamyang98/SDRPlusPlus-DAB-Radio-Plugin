#include "./texture.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif 

#include <GL/gl.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include <imgui/stb_image.h>

Texture::Texture(uint32_t id, int width, int height)
: m_id(id), m_width(width), m_height(height) 
{}

Texture::~Texture() {
    const GLuint id = GLuint(m_id);
    glDeleteTextures(1, &id);
}


std::unique_ptr<Texture> Texture::LoadFromMemory(const uint8_t* data, const size_t total_bytes) {
    int width = 0;
    int height = 0;
    int bits_per_pixel = 0;
    uint8_t* image_data = stbi_load_from_memory(
        data, int(total_bytes), 
        &width, &height, &bits_per_pixel, 4
    );

    if (image_data == nullptr) {
        return nullptr;
    }
    
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // wrap texture on x (S) and y (T) axis 
    #ifdef _WIN32
    // NOTE: apparently we need a header file for opengl 1.2
    //       windows doesn't give this at all
    constexpr auto GL_CLAMP_TO_EDGE = 0x812F;
    #endif
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // give image buffer to opengl
    // stbi_set_flip_vertically_on_load(1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    stbi_image_free(image_data);
    return std::make_unique<Texture>(id, width, height);
}
