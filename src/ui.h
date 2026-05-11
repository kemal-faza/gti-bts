#pragma once

#include <cstddef>

// ── Font system (stb_truetype-based, scalable, Arial-like) ──

// Init: load TTF, bake glyph atlas into OpenGL texture.
// Returns false if no font is available (fallback to stroke fonts in renderer).
bool FontInit();
void FontCleanup();
bool FontIsReady();

// Render text with given pixel scale (proportional to window height).
void RenderString(float x, float y, float scale, const char *text);
void RenderStringMultiline(float x, float y, float scale, const char *text);

void UpdateWindowTitle();
void SetColor(float r, float g, float b);

// ── Scale helpers (controlled renders proportional to gWindowHeight) ──
// Heading: ~2.0% of window height. Body: ~1.4% of window height.
float HeadingScale();
float BodyScale();
float HeadingLineH();  // line height including spacing
float BodyLineH();
