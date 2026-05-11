#include "ui.h"
#include "scene.h"

#include <GL/gl.h>
#include <GL/freeglut.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

void SetColor(const float r, const float g, const float b)
{
    glColor3f(r, g, b);
}

// ── Stroke font scaling ──
// GLUT stroke fonts have a character height of ~119.4 units.
// We scale them proportionally to gWindowHeight so text
// stays readable at any window size.
constexpr float STROKE_H = 119.4f;

static float HeadingScale()
{
    // ~3.5% of window height → at 640: 22px, at 1080: 38px
    return gWindowHeight / 2850.0f;
}
static float BodyScale()
{
    // ~2.5% of window height → at 640: 16px, at 1080: 27px
    return gWindowHeight / 4300.0f;
}
static float HeadingLineH()
{
    return HeadingScale() * 150.0f;
}
static float BodyLineH()
{
    return BodyScale() * 140.0f;
}

float UIHeadingScale() { return HeadingScale(); }
float UIBodyScale()    { return BodyScale(); }
float UIHeadingLineH() { return HeadingLineH(); }
float UIBodyLineH()    { return BodyLineH(); }

void RenderString(float x, float y, void *font, const char *text)
{
    if (text == nullptr || *text == '\0') return;

    // Determine scale: GLUT_STROKE_ROMAN → heading, GLUT_STROKE_MONO_ROMAN → body
    const bool isHeading = (font == GLUT_STROKE_ROMAN);
    const float scale = isHeading ? HeadingScale() : BodyScale();
    const float lw = std::max(1.0f, scale * 15.0f);

    glPushMatrix();
    glLineWidth(lw);
    glTranslatef(x, y + STROKE_H * scale, 0.0f);
    glScalef(scale, -scale, 1.0f);  // flip Y: stroke font Y-up → overlay Y-down

    for (const char *c = text; *c != '\0'; ++c)
        glutStrokeCharacter(font, static_cast<int>(*c));

    glLineWidth(1.0f);
    glPopMatrix();
}

void RenderStringMultiline(float x, float y, void *font, const char *text)
{
    if (text == nullptr || *text == '\0') return;

    const float lineH = (font == GLUT_STROKE_ROMAN) ? HeadingLineH() : BodyLineH();
    char lineBuf[256];

    float lineY = y;
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
        lineY += lineH;
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
