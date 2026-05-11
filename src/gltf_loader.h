#pragma once

#include <vector>

// Forward declaration for GL texture handle
typedef unsigned int GLuint;

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
    GLuint glTextureId = 0;           // OpenGL texture (0 = no texture, use baseColor only)
};

// ---------------------------------------------------------------------------
//  A complete glTF model (collection of primitives + normalization info)
// ---------------------------------------------------------------------------
struct GLTFModel
{
    std::vector<GLTFPrimitive> primitives;
    float normalizeScale = 1.0f;
    float offsetY = 0.0f; // translation so model sits on y=0

    // Axis-aligned half-extents in local space (computed after normalization)
    float boundsHalfX = 0.5f;
    float boundsHalfY = 0.5f;
    float boundsHalfZ = 0.5f;
};

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------

// Load a glTF model from a directory containing scene.gltf + scene.bin
// Returns false on failure (prints error to stderr).
bool LoadGLTF(const char *dirPath, GLTFModel &outModel, float targetSize);

// Render a loaded glTF model using glBegin/glEnd with per-vertex color
// Set shadowMode=true during shadow pass to skip glColor4fv (so shadow stays black).
void DrawGLTFModel(const GLTFModel &model, bool shadowMode = false);

// Free OpenGL textures allocated during loading (call at shutdown).
void FreeGLTFModel(GLTFModel &model);
