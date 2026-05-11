#pragma once

// Stroke font scale helpers — exposed so renderer.cpp can use them
// for layout calculations (line heights, margins, etc.)
float UIHeadingScale();
float UIBodyScale();
float UIHeadingLineH();
float UIBodyLineH();

void UpdateWindowTitle();
void SetColor(float r, float g, float b);
void RenderString(float x, float y, void *font, const char *text);
void RenderStringMultiline(float x, float y, void *font, const char *text);
