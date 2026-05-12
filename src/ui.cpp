// ═══════════════════════════════════════════════════════════════════════════
//  ui.cpp — Text rendering via stb_truetype
//  Bakes all printable ASCII glyphs into a single OpenGL texture atlas
//  at init time, then renders text as scaled textured quads.
//  Falls back to GLUT stroke fonts if TTF is unavailable.
// ═══════════════════════════════════════════════════════════════════════════

#include "ui.h"
#include "scene.h"

#include <GL/gl.h>
#include <GL/freeglut.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>

// ── stb_truetype implementation ──
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// ── Constants ──
static constexpr int ATLAS_W = 1024;
static constexpr int ATLAS_H = 512;
static constexpr float BAKE_SIZE = 48.0f;  // px — glyph baking resolution

// ── Per-glyph info after baking into the atlas ──
struct GlyphInfo
{
    float u0, v0, u1, v1;  // UV in atlas texture
    float xoff, yoff;      // offset from cursor (in bake-space pixels)
    float xadvance;        // advance (in bake-space pixels)
    int   bitmapW, bitmapH; // bitmap pixel dimensions
};

// ── Font state ──
static bool          s_fontReady   = false;
static GLuint        s_atlasTexture= 0;
static GlyphInfo     s_glyphs[95];  // ASCII 32..126 → index 0..94
static float         s_bakeAscent  = 0.0f;
static float         s_bakeDescent = 0.0f;
static float         s_bakeLineGap = 0.0f;
static unsigned char *s_ttfData    = nullptr;

// ── Scale helpers ──
// Heading ≈ 2.0% of window height, Body ≈ 1.4% of window height.
// These produce readable text without dominating the viewport.
float HeadingScale() { return gWindowHeight * 0.020f; }
float BodyScale()    { return gWindowHeight * 0.014f; }
float HeadingLineH() { return (s_bakeAscent - s_bakeDescent + s_bakeLineGap) * (HeadingScale() / BAKE_SIZE); }
float BodyLineH()    { return (s_bakeAscent - s_bakeDescent + s_bakeLineGap) * (BodyScale() / BAKE_SIZE); }

// ═══════════════════════════════════════════════════════════════════════════
//  Atlas baking
// ═══════════════════════════════════════════════════════════════════════════

static bool BakeFontAtlas(stbtt_fontinfo &font)
{
    // Allocate atlas buffer (RGBA, initialised to zero)
    GLubyte *atlas = new GLubyte[static_cast<size_t>(ATLAS_W) * ATLAS_H * 4]();

    float scale = stbtt_ScaleForPixelHeight(&font, BAKE_SIZE);

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    s_bakeAscent  = static_cast<float>(ascent)  * scale;
    s_bakeDescent = static_cast<float>(descent) * scale;
    s_bakeLineGap = static_cast<float>(lineGap) * scale;

    // Simple row-based packing
    int packX = 0, packY = 0, rowH = 0;

    for (int i = 0; i < 95; ++i)
    {
        int codepoint = i + 32;
        int w, h, xoff, yoff;
        unsigned char *bitmap = stbtt_GetCodepointBitmap(&font, scale, scale,
                                                          codepoint, &w, &h,
                                                          &xoff, &yoff);
        if (bitmap == nullptr || w <= 0 || h <= 0)
        {
            // Empty glyph (e.g. space) — store metrics but no bitmap
            int advance;
            int lsb;
            stbtt_GetCodepointHMetrics(&font, codepoint, &advance, &lsb);
            s_glyphs[i].u0 = s_glyphs[i].u1 = 0.0f;
            s_glyphs[i].v0 = s_glyphs[i].v1 = 0.0f;
            s_glyphs[i].xoff      = 0.0f;
            s_glyphs[i].yoff      = 0.0f;
            s_glyphs[i].xadvance  = static_cast<float>(advance) * scale;
            s_glyphs[i].bitmapW   = 0;
            s_glyphs[i].bitmapH   = 0;
            if (bitmap) stbtt_FreeBitmap(bitmap, nullptr);
            continue;
        }

        // Advance to next row if this glyph doesn't fit
        if (packX + w + 1 > ATLAS_W)
        {
            packX = 0;
            packY += rowH + 1;
            rowH = 0;
        }

        // Check atlas overflow
        if (packY + h + 1 > ATLAS_H)
        {
            fprintf(stderr, "[Font] Atlas too small! (%dx%d)\n", ATLAS_W, ATLAS_H);
            delete[] atlas;
            stbtt_FreeBitmap(bitmap, nullptr);
            return false;
        }

        // Copy bitmap into atlas (grayscale → RGBA)
        for (int py = 0; py < h; ++py)
        {
            for (int px = 0; px < w; ++px)
            {
                int atlasIdx = ((packY + py) * ATLAS_W + (packX + px)) * 4;
                atlas[atlasIdx + 0] = 255;  // R
                atlas[atlasIdx + 1] = 255;  // G
                atlas[atlasIdx + 2] = 255;  // B
                atlas[atlasIdx + 3] = bitmap[py * w + px];  // A
            }
        }

        // Store glyph info
        // glTexImage2D treats data row 0 as V=0 (bottom of texture).
        // stb_truetype packs from the top of data (packY=0).
        // So a glyph's top row (data row packY) is at V = packY/ATLAS_H
        // (near bottom of texture — low V = top of glyph data).
        // v0 < v1: v0 = top of glyph, v1 = bottom of glyph.
        s_glyphs[i].u0 = static_cast<float>(packX)         / static_cast<float>(ATLAS_W);
        s_glyphs[i].v0 = static_cast<float>(packY)         / static_cast<float>(ATLAS_H);
        s_glyphs[i].u1 = static_cast<float>(packX + w)     / static_cast<float>(ATLAS_W);
        s_glyphs[i].v1 = static_cast<float>(packY + h)     / static_cast<float>(ATLAS_H);

        int advance;
        int lsb;
        stbtt_GetCodepointHMetrics(&font, codepoint, &advance, &lsb);
        s_glyphs[i].xoff     = static_cast<float>(xoff);
        s_glyphs[i].yoff     = static_cast<float>(yoff);
        s_glyphs[i].xadvance = static_cast<float>(advance) * scale;
        s_glyphs[i].bitmapW  = w;
        s_glyphs[i].bitmapH  = h;

        rowH = std::max(rowH, h);
        packX += w + 1;

        stbtt_FreeBitmap(bitmap, nullptr);
    }

    // ── Upload atlas to OpenGL ──
    glGenTextures(1, &s_atlasTexture);
    glBindTexture(GL_TEXTURE_2D, s_atlasTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_W, ATLAS_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, atlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    delete[] atlas;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Public API
// ═══════════════════════════════════════════════════════════════════════════

bool FontInit()
{
    // Try bundled font first, then system paths
    const char *paths[] = {
        "assets/font/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        nullptr
    };

    s_ttfData = nullptr;
    for (const char **pp = paths; *pp != nullptr; ++pp)
    {
        FILE *fp = fopen(*pp, "rb");
        if (!fp) continue;

        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (sz <= 0) { fclose(fp); continue; }

        s_ttfData = new unsigned char[static_cast<size_t>(sz)];
        if (fread(s_ttfData, 1, static_cast<size_t>(sz), fp) != static_cast<size_t>(sz))
        {
            delete[] s_ttfData;
            s_ttfData = nullptr;
            fclose(fp);
            continue;
        }
        fclose(fp);

        // Try to init font
        stbtt_fontinfo font;
        if (!stbtt_InitFont(&font, s_ttfData, 0))
        {
            delete[] s_ttfData;
            s_ttfData = nullptr;
            continue;
        }

        // Bake atlas
        s_fontReady = BakeFontAtlas(font);
        if (!s_fontReady)
        {
            delete[] s_ttfData;
            s_ttfData = nullptr;
            continue;
        }

        fprintf(stdout, "[Font] Loaded: %s  (baked at %.0fpx)\n", *pp, BAKE_SIZE);
        return true;
    }

    fprintf(stderr, "[Font] No TTF font found; text will not render.\n");
    s_fontReady = false;
    return false;
}

void FontCleanup()
{
    if (s_atlasTexture != 0)
    {
        glDeleteTextures(1, &s_atlasTexture);
        s_atlasTexture = 0;
    }
    if (s_ttfData != nullptr)
    {
        delete[] s_ttfData;
        s_ttfData = nullptr;
    }
    s_fontReady = false;
}

bool FontIsReady() { return s_fontReady; }

void SetColor(const float r, const float g, const float b)
{
    glColor3f(r, g, b);
}

void RenderString(float x, float y, float scale, const char *text)
{
    if (!s_fontReady || text == nullptr || *text == '\0') return;

    const float factor = scale / BAKE_SIZE;  // display scale relative to bake

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, s_atlasTexture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float cursorX = x;
    for (const char *cp = text; *cp != '\0'; ++cp)
    {
        int ch = static_cast<int>(*cp) - 32;
        if (ch < 0 || ch > 94)
        {
            // Non-printable character — skip
            cursorX += BAKE_SIZE * factor * 0.5f;
            continue;
        }

        const GlyphInfo &g = s_glyphs[ch];
        if (g.bitmapW <= 0 || g.bitmapH <= 0)  // e.g. space
        {
            cursorX += g.xadvance * factor;
            continue;
        }

        // In overlay (Y-down): text top at y, baseline at y + s_bakeAscent * factor
        float dx = g.xoff * factor;
        float dy = (s_bakeAscent - g.yoff) * factor;
        float dw = static_cast<float>(g.bitmapW) * factor;
        float dh = static_cast<float>(g.bitmapH) * factor;

        glBegin(GL_QUADS);
        glTexCoord2f(g.u0, g.v0); glVertex2f(cursorX + dx, y + dy);
        glTexCoord2f(g.u1, g.v0); glVertex2f(cursorX + dx + dw, y + dy);
        glTexCoord2f(g.u1, g.v1); glVertex2f(cursorX + dx + dw, y + dy + dh);
        glTexCoord2f(g.u0, g.v1); glVertex2f(cursorX + dx, y + dy + dh);
        glEnd();

        cursorX += g.xadvance * factor;
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

void RenderStringMultiline(float x, float y, float scale, const char *text)
{
    if (!s_fontReady || text == nullptr || *text == '\0') return;

    const float lineH = (s_bakeAscent - s_bakeDescent + s_bakeLineGap) * (scale / BAKE_SIZE);
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
            RenderString(x, lineY, scale, lineBuf);
        }
        if (nl == nullptr) break;
        text = nl + 1;
        lineY += lineH;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Window title
// ═══════════════════════════════════════════════════════════════════════════

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
