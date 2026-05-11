#pragma once

#include "scene.h"

#include <GL/gl.h>

void InitTextures();
void ShutdownTextures();
GLuint GetTextureForSubType(ObjectSubType subType);
GLuint GetFloorTexture();
GLuint GetWallTexture();
