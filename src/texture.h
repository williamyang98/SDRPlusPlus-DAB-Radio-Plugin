#pragma once

#include <stdint.h>
#include <stddef.h>
#include <memory>
#include "imgui.h"

class Texture
{
private:
    const uint32_t m_id;
    const int m_width; 
    const int m_height;
public:
    Texture(uint32_t id, int width, int height);
    ~Texture();
    Texture(Texture&) = delete;
    Texture(Texture&&) = delete;
    Texture& operator=(Texture&) = delete;
    Texture& operator=(Texture&&) = delete;
    uint32_t GetTextureID() const { return m_id; }
    inline int GetWidth() const { return m_width; }
    inline int GetHeight() const { return m_height; }
public:
    static std::unique_ptr<Texture> LoadFromMemory(const uint8_t* data, const size_t total_bytes);
};
