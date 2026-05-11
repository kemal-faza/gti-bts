#include "ui.h"
#include "scene.h"

#include <GL/glut.h>
#include <cstdio>

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
