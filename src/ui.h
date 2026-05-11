#pragma once

void UpdateWindowTitle();
void SetColor(float r, float g, float b);
void RenderString(float x, float y, void *font, const char *text);
void RenderStringMultiline(float x, float y, void *font, const char *text);
