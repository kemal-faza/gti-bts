#pragma once

#include "scene.h"

void ApplyShadingMode();
void ApplyDepthTestMode();

void ConfigureProjectionMatrix();
void ApplyCameraViewMatrix();

void ApplyObjectMaterial(MaterialType material, bool selected);
void SetupLights();

void DrawCylinderPrimitive(float radius, float height);
void DrawObjectGeometry(const SceneObject &obj);
void DrawGrid();
void DrawSceneObjects();
void DrawPointLightMarker();

void Display();
