#include "texture.h"

#include <GL/gl.h>
#include <GL/glu.h>
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

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------

void InitTextures()
{
    // Reset all
    std::memset(gTextures, 0, sizeof(gTextures));

    // Generate procedural textures per furniture subtype
    // Colors: (r1,g1,b1) = light, (r2,g2,b2) = dark

    gTextures[static_cast<int>(ObjectSubType::SOFA)] =
        GenerateCheckerTexture(64, 4, 180, 120, 80, 140, 90, 50);

    gTextures[static_cast<int>(ObjectSubType::MEJA)] =
        GenerateCheckerTexture(64, 4, 200, 170, 120, 160, 130, 90);

    gTextures[static_cast<int>(ObjectSubType::LEMARI)] =
        GenerateCheckerTexture(64, 6, 180, 150, 100, 140, 110, 70);

    gTextures[static_cast<int>(ObjectSubType::RAK)] =
        GenerateCheckerTexture(64, 4, 160, 140, 110, 120, 100, 80);

    gTextures[static_cast<int>(ObjectSubType::KURSI)] =
        GenerateCheckerTexture(64, 3, 70, 60, 180, 50, 40, 140);

    gTextures[static_cast<int>(ObjectSubType::MEJA_BUNDAR)] =
        GenerateCheckerTexture(32, 4, 190, 160, 130, 150, 120, 90);

    gTextures[static_cast<int>(ObjectSubType::LAMPU)] =
        GenerateCheckerTexture(32, 2, 240, 210, 100, 200, 170, 60);

    gTextures[static_cast<int>(ObjectSubType::KARPET)] =
        GenerateCheckerTexture(128, 8, 210, 110, 70, 230, 190, 150);

    gTextures[static_cast<int>(ObjectSubType::TIKAR)] =
        GenerateCheckerTexture(128, 6, 180, 200, 140, 140, 160, 100);

    gTextures[static_cast<int>(ObjectSubType::KIPAS)] =
        GenerateCheckerTexture(64, 2, 200, 200, 200, 150, 150, 150);
}

GLuint GetTextureForSubType(const ObjectSubType subType)
{
    const int idx = static_cast<int>(subType);
    if (idx < 0 || idx >= kMaxSubTypes) return 0;
    return gTextures[idx];
}
