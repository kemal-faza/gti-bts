#include "ui.h"
#include "scene.h"

#include <GL/gl.h>
#include <GL/freeglut.h>
#include <cstdio>
#include <cstring>

void SetColor(const float r, const float g, const float b)
{
    glColor3f(r, g, b);
}

void RenderString(float x, float y, void *font, const char *text)
{
    glRasterPos2f(x, y);
    glutBitmapString(font, reinterpret_cast<const unsigned char *>(text));
}

void RenderStringMultiline(float x, float y, void *font, const char *text)
{
    float lineY = y;
    const float lineHeight = 18.0f;
    char lineBuf[256];

    while (*text != '\0')
    {
        const char *nl = std::strchr(text, '\n');
        int len = (nl != nullptr) ? static_cast<int>(nl - text)
                                  : static_cast<int>(std::strlen(text));

        if (len > 0 && len < 256)
        {
            std::strncpy(lineBuf, text, static_cast<size_t>(len));
            lineBuf[len] = '\0';
            RenderString(x, lineY, font, lineBuf);
        }
        if (nl == nullptr) break;
        text = nl + 1;
        lineY -= lineHeight;
    }
}

void UpdateWindowTitle()
{
    char title[512];
    std::snprintf(title,
                  sizeof(title),
                  "GTI BTS | Mode:%s | Cam:%s | Sel:%d/%zu | L:%s | Shading:%s | Depth:%s | Tab Mode | [/] Select | Space Add | 1/2/3 Preset | WASD+QE Move | L Light | F Shading | Z Depth | Hold Left Drag",
                  ModeLabel(),
                  PresetLabel(),
                  gSceneObjects.empty() ? 0 : (gState.selectedObjectIndex + 1),
                  gSceneObjects.size(),
                  gState.directionalLightEnabled ? "ON" : "OFF",
                  gState.smoothShading ? "SMOOTH" : "FLAT",
                  gState.depthTestEnabled ? "ON" : "OFF");
    glutSetWindowTitle(title);
}
