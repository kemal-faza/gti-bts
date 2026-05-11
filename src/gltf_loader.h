#pragma once

#include <vector>

// ---------------------------------------------------------------------------
//  A single drawable primitive from a glTF mesh
// ---------------------------------------------------------------------------
struct GLTFPrimitive
{
    std::vector<float> positions;      // xyz interleaved
    std::vector<float> normals;        // xyz interleaved (may be empty)
    std::vector<float> texcoords;      // uv interleaved (may be empty)
    std::vector<unsigned int> indices; // triangle indices
    float baseColor[4] = {0.8f, 0.8f, 0.8f, 1.0f};
    bool skip = false;                 // FLOOR mesh — skip in rendering
};

// ---------------------------------------------------------------------------
//  A complete glTF model (collection of primitives + normalization info)
// ---------------------------------------------------------------------------
struct GLTFModel
{
    std::vector<GLTFPrimitive> primitives;
    float normalizeScale = 1.0f;
    float offsetY = 0.0f; // translation so model sits on y=0
};

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------

// Load a glTF model from a directory containing scene.gltf + scene.bin
// Returns false on failure (prints error to stderr).
bool LoadGLTF(const char *dirPath, GLTFModel &outModel, float targetSize);

// Render a loaded glTF model using glBegin/glEnd with per-vertex color
void DrawGLTFModel(const GLTFModel &model);
