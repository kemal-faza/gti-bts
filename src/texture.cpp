#include "texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <GL/gl.h>
#include <GL/glu.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
//  Procedural texture generation
// ---------------------------------------------------------------------------

static GLuint GenerateCheckerTexture(const int size, const int checkers,
                                     const GLubyte r1, const GLubyte g1, const GLubyte b1,
                                     const GLubyte r2, const GLubyte g2, const GLubyte b2)
{
    const int channels = 3; // RGB
    GLubyte *pixels = new GLubyte[static_cast<size_t>(size) * static_cast<size_t>(size) * static_cast<size_t>(channels)];

    const int checkSize = size / checkers;

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            int xi = x / checkSize;
            int yi = y / checkSize;
            bool light = ((xi + yi) % 2) == 0;

            int idx = (y * size + x) * channels;
            pixels[idx + 0] = light ? r1 : r2;
            pixels[idx + 1] = light ? g1 : g2;
            pixels[idx + 2] = light ? b1 : b2;
        }
    }

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    gluBuild2DMipmaps(GL_TEXTURE_2D, channels, size, size,
                      GL_RGB, GL_UNSIGNED_BYTE, pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    delete[] pixels;
    return texID;
}

// ---------------------------------------------------------------------------
//  Texture storage
// ---------------------------------------------------------------------------

static constexpr int kMaxSubTypes = 32;
static GLuint gTextures[kMaxSubTypes] = {0};
static GLuint gFloorTexture = 0;
static GLuint gWallTexture = 0;

// ---------------------------------------------------------------------------
//  File texture loader (via stb_image)
// ---------------------------------------------------------------------------

static GLuint LoadTextureFromFile(const char *path)
{
    int w, h, channels;
    unsigned char *data = stbi_load(path, &w, &h, &channels, 0);
    if (data == nullptr)
    {
        fprintf(stderr, "[Ruang] Gagal load texture: %s\n", path);
        return 0;
    }

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    const GLint  internalFormat = (channels == 4) ? GL_RGBA : GL_RGB;
    const GLenum format         = (channels == 4) ? GL_RGBA : GL_RGB;

    gluBuild2DMipmaps(GL_TEXTURE_2D, internalFormat, w, h, format,
                      GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return texID;
}

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------

void InitTextures()
{
    // Bersihkan tekstur lama sebelum generate baru (safe re-call)
    ShutdownTextures();

    // ── Step 1: Load file textures ──
    struct { ObjectSubType subType; const char *path; } fileMappings[] = {
        {ObjectSubType::SOFA,       "assets/texture/fabric.png"},
        {ObjectSubType::MEJA,       "assets/texture/wood-grain.png"},
        {ObjectSubType::LEMARI,     "assets/texture/wood-grain.png"},
        {ObjectSubType::RAK,        "assets/texture/wood-grain.png"},
        {ObjectSubType::KURSI,      "assets/texture/fabric.png"},
        {ObjectSubType::MEJA_BUNDAR,"assets/texture/wood-grain.png"},
        {ObjectSubType::LAMPU,      "assets/texture/metal.png"},
        {ObjectSubType::KARPET,     "assets/texture/carpet.png"},
        {ObjectSubType::KIPAS,      "assets/texture/metal.png"},
    };

    for (const auto &m : fileMappings)
        gTextures[static_cast<int>(m.subType)] = LoadTextureFromFile(m.path);

    // Floor texture
    gFloorTexture = LoadTextureFromFile("assets/texture/wood-floor.png");

    // Wall texture
    gWallTexture = LoadTextureFromFile("assets/texture/wall.png");

    // ── Step 2: Fallback procedural untuk yang gagal load ──
    struct { ObjectSubType subType; int ck; GLubyte r1,g1,b1, r2,g2,b2; } fallback[] = {
        {ObjectSubType::SOFA,        4, 180,120,80,  140,90,50},
        {ObjectSubType::MEJA,        4, 200,170,120, 160,130,90},
        {ObjectSubType::LEMARI,      6, 180,150,100, 140,110,70},
        {ObjectSubType::RAK,         4, 160,140,110, 120,100,80},
        {ObjectSubType::KURSI,       3,  70,60,180,  50,40,140},
        {ObjectSubType::MEJA_BUNDAR, 4, 190,160,130, 150,120,90},
        {ObjectSubType::LAMPU,       2, 240,210,100, 200,170,60},
        {ObjectSubType::KARPET,      8, 210,110,70,  230,190,150},
        {ObjectSubType::KIPAS,       2, 200,200,200, 150,150,150},
    };

    for (const auto &p : fallback)
    {
        const int idx = static_cast<int>(p.subType);
        if (gTextures[idx] == 0)
        {
            gTextures[idx] = GenerateCheckerTexture(64, p.ck,
                                                    p.r1, p.g1, p.b1,
                                                    p.r2, p.g2, p.b2);
        }
    }

    // Floor fallback — dark checkerboard
    if (gFloorTexture == 0)
        gFloorTexture = GenerateCheckerTexture(128, 8, 80,80,85, 55,55,60);

    // Wall fallback — light grey checker
    if (gWallTexture == 0)
        gWallTexture = GenerateCheckerTexture(128, 8, 180,185,190, 160,165,170);
}

void ShutdownTextures()
{
    for (int i = 0; i < kMaxSubTypes; ++i)
    {
        if (gTextures[i] != 0)
        {
            glDeleteTextures(1, &gTextures[i]);
            gTextures[i] = 0;
        }
    }

    if (gFloorTexture != 0)
    {
        glDeleteTextures(1, &gFloorTexture);
        gFloorTexture = 0;
    }

    if (gWallTexture != 0)
    {
        glDeleteTextures(1, &gWallTexture);
        gWallTexture = 0;
    }
}

GLuint GetTextureForSubType(const ObjectSubType subType)
{
    const int idx = static_cast<int>(subType);
    if (idx < 0 || idx >= kMaxSubTypes) return 0;
    return gTextures[idx];
}

GLuint GetFloorTexture()
{
    return gFloorTexture;
}

GLuint GetWallTexture()
{
    return gWallTexture;
}
