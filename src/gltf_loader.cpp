#include "gltf_loader.h"
#include "json.hpp"
#include "stb_image.h"

#include <GL/gl.h>
#include <GL/glu.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

// ═══════════════════════════════════════════════════════════════════════════
//  Internal helpers
// ═══════════════════════════════════════════════════════════════════════════

// Component type sizes (glTF spec)
static int CompSize(const int compType)
{
    switch (compType)
    {
    case 5120: return 1; // BYTE
    case 5121: return 1; // UNSIGNED_BYTE
    case 5122: return 2; // SHORT
    case 5123: return 2; // UNSIGNED_SHORT
    case 5125: return 4; // UNSIGNED_INT
    case 5126: return 4; // FLOAT
    default:   return 0;
    }
}

// Number of components per type string
static int TypeCount(const std::string &t)
{
    if (t == "SCALAR") return 1;
    if (t == "VEC2")   return 2;
    if (t == "VEC3")   return 3;
    if (t == "VEC4")   return 4;
    if (t == "MAT2")   return 4;
    if (t == "MAT3")   return 9;
    if (t == "MAT4")   return 16;
    return 0;
}

// ---------------------------------------------------------------------------
//  4×4 matrix helpers (column-major, same layout as OpenGL)
// ---------------------------------------------------------------------------
struct Mat4
{
    float m[16];

    static Mat4 Identity()
    {
        Mat4 r;
        for (int i = 0; i < 16; ++i) r.m[i] = 0.0f;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    static Mat4 Mul(const Mat4 &a, const Mat4 &b)
    {
        Mat4 r;
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
            {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k)
                    sum += a.m[k * 4 + row] * b.m[col * 4 + k];
                r.m[col * 4 + row] = sum;
            }
        return r;
    }

    // Transform a 3D point (no perspective divide)
    static void Transform(const Mat4 &mat, float &x, float &y, float &z)
    {
        const float *m = mat.m;
        float nx = m[0] * x + m[4] * y + m[8]  * z + m[12];
        float ny = m[1] * x + m[5] * y + m[9]  * z + m[13];
        float nz = m[2] * x + m[6] * y + m[10] * z + m[14];
        x = nx; y = ny; z = nz;
    }

    // Build from TRS (translation, rotation quaternion [x,y,z,w], scale)
    static Mat4 FromTRS(const float t[3], const float q[4], const float s[3])
    {
        const float qx = q[0], qy = q[1], qz = q[2], qw = q[3];
        const float sx = s[0], sy = s[1], sz = s[2];
        const float tx = t[0], ty = t[1], tz = t[2];

        // Normalise quaternion
        float len = qx * qx + qy * qy + qz * qz + qw * qw;
        if (len > 0.0f) { len = 1.0f / std::sqrt(len); }
        const float x = qx * len, y = qy * len, z = qz * len, w = qw * len;

        const float xx = x * x, yy = y * y, zz = z * z;
        const float xy = x * y, xz = x * z, xw = x * w;
        const float yz = y * z, yw = y * w, zw = z * w;

        // Rotation matrix (column-major)
        float R[16] = {
            1.0f - 2.0f * (yy + zz),  2.0f * (xy + zw),          2.0f * (xz - yw),          0.0f,
            2.0f * (xy - zw),          1.0f - 2.0f * (xx + zz),  2.0f * (yz + xw),          0.0f,
            2.0f * (xz + yw),          2.0f * (yz - xw),          1.0f - 2.0f * (xx + yy),  0.0f,
            0.0f,                      0.0f,                      0.0f,                      1.0f
        };

        // Scale matrix (column-major)
        float S[16] = {
            sx,  0.0f, 0.0f, 0.0f,
            0.0f, sy,  0.0f, 0.0f,
            0.0f, 0.0f, sz,  0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        // Translation matrix (column-major)
        float T[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            tx,   ty,   tz,   1.0f
        };

        Mat4 mR, mS, mT;
        std::memcpy(mR.m, R, sizeof(R));
        std::memcpy(mS.m, S, sizeof(S));
        std::memcpy(mT.m, T, sizeof(T));

        // M = T * R * S
        Mat4 RS = Mul(mR, mS);
        return Mul(mT, RS);
    }

    // Build from column-major 16-float array (node.matrix)
    static Mat4 FromArray(const float arr[16])
    {
        Mat4 r;
        std::memcpy(r.m, arr, sizeof(float) * 16);
        return r;
    }
};

// ---------------------------------------------------------------------------
//  Get node transform (recursive traversal)
// ---------------------------------------------------------------------------
static Mat4 GetNodeTransform(const json &node)
{
    if (node.contains("matrix"))
    {
        const auto &arr = node["matrix"];
        float m[16];
        for (int i = 0; i < 16 && i < static_cast<int>(arr.size()); ++i)
            m[i] = arr[i].get<float>();
        return Mat4::FromArray(m);
    }

    // Default TRS values
    const float defT[3] = {0.0f, 0.0f, 0.0f};
    const float defR[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    const float defS[3] = {1.0f, 1.0f, 1.0f};

    float t[3], r[4], s[3];
    std::memcpy(t, defT, sizeof(t));
    std::memcpy(r, defR, sizeof(r));
    std::memcpy(s, defS, sizeof(s));

    if (node.contains("translation"))
        for (int i = 0; i < 3; ++i) t[i] = node["translation"][i].get<float>();
    if (node.contains("rotation"))
        for (int i = 0; i < 4; ++i) r[i] = node["rotation"][i].get<float>();
    if (node.contains("scale"))
        for (int i = 0; i < 3; ++i) s[i] = node["scale"][i].get<float>();

    return Mat4::FromTRS(t, r, s);
}

// ---------------------------------------------------------------------------
//  Load a texture image from a file via stb_image (reuses stb implementation
//  from texture.cpp — no STB_IMAGE_IMPLEMENTATION here).
// ---------------------------------------------------------------------------
static GLuint LoadGLTFTexture(const char *fullPath)
{
    int w, h, channels;
    // Force RGBA loading (4 channels) — menangani PNG 1-channel (grayscale)
    // yang sering dipakai glTF untuk alpha mask/floor textures
    unsigned char *data = stbi_load(fullPath, &w, &h, &channels, 4);
    if (data == nullptr)
    {
        fprintf(stderr, "[glTF] Failed to load texture: %s\n", fullPath);
        return 0;
    }

    GLuint texID = 0;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    const GLint  internalFormat = GL_RGBA;
    const GLenum format         = GL_RGBA;

    gluBuild2DMipmaps(GL_TEXTURE_2D, internalFormat, w, h, format,
                      GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);

    fprintf(stdout, "[glTF] Loaded texture: %s  (%dx%d, %d ch)\n", fullPath, w, h, channels);
    return texID;
}

// ---------------------------------------------------------------------------
//  Recursively walk scene nodes, accumulating transforms
// ---------------------------------------------------------------------------
static void WalkNodes(const json &doc,
                      const char *dirPath,
                      int nodeIndex,
                      const Mat4 &parentXform,
                      const unsigned char *binData,
                      int binLength,
                      GLTFModel &model)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc["nodes"].size()))
        return;

    const json &node = doc["nodes"][nodeIndex];
    Mat4 localXform = GetNodeTransform(node);
    Mat4 worldXform = Mat4::Mul(parentXform, localXform);

    // If this node has a mesh, extract its primitives
    if (node.contains("mesh"))
    {
        int meshIdx = node["mesh"].get<int>();
        if (meshIdx >= 0 && meshIdx < static_cast<int>(doc["meshes"].size()))
        {
            const json &mesh = doc["meshes"][meshIdx];
            const std::string meshName = mesh.value("name", "");

            for (const auto &prim : mesh["primitives"])
            {
                GLTFPrimitive p;

                // --- Read material ---
                int matIdx = -1;
                if (prim.contains("material"))
                    matIdx = prim["material"].get<int>();

                if (matIdx >= 0 && matIdx < static_cast<int>(doc["materials"].size()))
                {
                    const json &mat = doc["materials"][matIdx];

                    // Check for FLOOR: alphaMode == "BLEND" AND name contains "FLOOR"
                    std::string matName = mat.value("name", "");
                    std::string alphaMode = mat.value("alphaMode", "OPAQUE");
                    bool isBlend = (alphaMode == "BLEND");
                    bool nameHasFloor = (matName.find("FLOOR") != std::string::npos);
                    bool meshHasFloor = (meshName.find("FLOOR") != std::string::npos);

                    if (isBlend && (nameHasFloor || meshHasFloor))
                    {
                        p.skip = true;
                        fprintf(stdout, "[glTF] Skipping FLOOR mesh: '%s' material='%s'\n",
                                meshName.c_str(), matName.c_str());
                    }

                    // Read baseColorFactor
                    if (mat.contains("pbrMetallicRoughness") &&
                        mat["pbrMetallicRoughness"].contains("baseColorFactor"))
                    {
                        const auto &bcf = mat["pbrMetallicRoughness"]["baseColorFactor"];
                        for (int i = 0; i < 4 && i < static_cast<int>(bcf.size()); ++i)
                            p.baseColor[i] = bcf[i].get<float>();
                    }

                }

                if (p.skip)
                {
                    model.primitives.push_back(p);
                    continue;
                }

                // ── Load baseColorTexture (only for non-skipped primitives) ──
                if (matIdx >= 0 && matIdx < static_cast<int>(doc["materials"].size()))
                {
                    const json &mat = doc["materials"][matIdx];
                    if (mat.contains("pbrMetallicRoughness") &&
                        mat["pbrMetallicRoughness"].contains("baseColorTexture"))
                    {
                        int texIdx = mat["pbrMetallicRoughness"]["baseColorTexture"]["index"].get<int>();
                        if (texIdx >= 0 && texIdx < static_cast<int>(doc["textures"].size()))
                        {
                            int srcIdx = doc["textures"][texIdx]["source"].get<int>();
                            if (srcIdx >= 0 && srcIdx < static_cast<int>(doc["images"].size()))
                            {
                                std::string uri = doc["images"][srcIdx]["uri"].get<std::string>();
                                std::string fullPath = std::string(dirPath) + "/" + uri;
                                p.glTextureId = LoadGLTFTexture(fullPath.c_str());
                            }
                        }
                    }
                }

                // --- Helper lambda: read accessor data ---
                auto readAccessorFloats = [&](int accIdx,
                                              std::vector<float> &out,
                                              int expectedComponents) -> bool
                {
                    if (accIdx < 0 || accIdx >= static_cast<int>(doc["accessors"].size()))
                        return false;
                    const json &acc = doc["accessors"][accIdx];
                    int bvIdx = acc["bufferView"].get<int>();
                    int compType = acc["componentType"].get<int>();
                    int count = acc["count"].get<int>();
                    std::string typeStr = acc["type"].get<std::string>();
                    int numComp = TypeCount(typeStr);
                    if (numComp != expectedComponents) return false;

                    int compByte = CompSize(compType);
                    int stride = compByte * numComp;

                    const json &bv = doc["bufferViews"][bvIdx];
                    int bvByteOffset = bv.value("byteOffset", 0);
                    int bvByteStride = bv.value("byteStride", 0);
                    int accByteOffset = acc.value("byteOffset", 0);

                    const unsigned char *base = binData + bvByteOffset + accByteOffset;
                    int effectiveStride = (bvByteStride != 0) ? bvByteStride : stride;

                    out.reserve(static_cast<size_t>(count) * static_cast<size_t>(numComp));
                    for (int i = 0; i < count; ++i)
                    {
                        const unsigned char *src = base + i * effectiveStride;
                        for (int c = 0; c < numComp; ++c)
                        {
                            if (compType == 5126) // FLOAT
                            {
                                float v;
                                std::memcpy(&v, src + c * compByte, sizeof(float));
                                out.push_back(v);
                            }
                            else if (compType == 5120) // BYTE
                            {
                                out.push_back(static_cast<float>(static_cast<signed char>(src[c])));
                            }
                            else if (compType == 5121) // UNSIGNED_BYTE
                            {
                                out.push_back(static_cast<float>(src[c]));
                            }
                            else if (compType == 5122) // SHORT
                            {
                                short sv;
                                std::memcpy(&sv, src + c * compByte, sizeof(short));
                                out.push_back(static_cast<float>(sv));
                            }
                            else if (compType == 5123) // UNSIGNED_SHORT
                            {
                                unsigned short usv;
                                std::memcpy(&usv, src + c * compByte, sizeof(unsigned short));
                                out.push_back(static_cast<float>(usv));
                            }
                            else
                            {
                                // Unsupported component type
                                return false;
                            }
                        }
                    }
                    return true;
                };

                auto readIndices = [&](int accIdx,
                                       std::vector<unsigned int> &out) -> bool
                {
                    if (accIdx < 0 || accIdx >= static_cast<int>(doc["accessors"].size()))
                        return false;
                    const json &acc = doc["accessors"][accIdx];
                    int bvIdx = acc["bufferView"].get<int>();
                    int compType = acc["componentType"].get<int>();
                    int count = acc["count"].get<int>();

                    int compByte = CompSize(compType);

                    const json &bv = doc["bufferViews"][bvIdx];
                    int bvByteOffset = bv.value("byteOffset", 0);
                    int bvByteStride = bv.value("byteStride", 0);
                    int accByteOffset = acc.value("byteOffset", 0);

                    const unsigned char *base = binData + bvByteOffset + accByteOffset;
                    int effectiveStride = (bvByteStride != 0) ? bvByteStride : compByte;

                    out.reserve(static_cast<size_t>(count));
                    for (int i = 0; i < count; ++i)
                    {
                        const unsigned char *src = base + i * effectiveStride;
                        if (compType == 5125) // UNSIGNED_INT
                        {
                            unsigned int v;
                            std::memcpy(&v, src, sizeof(unsigned int));
                            out.push_back(v);
                        }
                        else if (compType == 5123) // UNSIGNED_SHORT
                        {
                            unsigned short v;
                            std::memcpy(&v, src, sizeof(unsigned short));
                            out.push_back(static_cast<unsigned int>(v));
                        }
                        else if (compType == 5122) // SHORT
                        {
                            short v;
                            std::memcpy(&v, src, sizeof(short));
                            out.push_back(static_cast<unsigned int>(v));
                        }
                        else if (compType == 5121) // UNSIGNED_BYTE
                        {
                            out.push_back(static_cast<unsigned int>(src[0]));
                        }
                        else
                        {
                            return false;
                        }
                    }
                    return true;
                };

                // --- Read attributes ---
                const auto &attrs = prim["attributes"];

                // Position (VEC3) is mandatory
                if (attrs.contains("POSITION"))
                {
                    readAccessorFloats(attrs["POSITION"].get<int>(), p.positions, 3);

                    // Transform positions by world transform
                    for (size_t i = 0; i + 2 < p.positions.size(); i += 3)
                    {
                        float x = p.positions[i];
                        float y = p.positions[i + 1];
                        float z = p.positions[i + 2];
                        Mat4::Transform(worldXform, x, y, z);
                        p.positions[i]     = x;
                        p.positions[i + 1] = y;
                        p.positions[i + 2] = z;
                    }
                }

                // Normal (VEC3) — optional but transform it too
                if (attrs.contains("NORMAL"))
                {
                    readAccessorFloats(attrs["NORMAL"].get<int>(), p.normals, 3);

                    // For normals, we should use the inverse transpose of the upper-left 3x3
                    // For simplicity with mostly uniform scales, just use the 3x3 part
                    // (In practice, Sketchfab models have uniform or near-uniform scale)
                    for (size_t i = 0; i + 2 < p.normals.size(); i += 3)
                    {
                        float x = p.normals[i];
                        float y = p.normals[i + 1];
                        float z = p.normals[i + 2];
                        // Apply upper 3x3 of world transform
                        const float *m = worldXform.m;
                        float nx = m[0] * x + m[4] * y + m[8]  * z;
                        float ny = m[1] * x + m[5] * y + m[9]  * z;
                        float nz = m[2] * x + m[6] * y + m[10] * z;
                        // Renormalise
                        float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
                        if (nl > 0.0001f) { nx /= nl; ny /= nl; nz /= nl; }
                        p.normals[i]     = nx;
                        p.normals[i + 1] = ny;
                        p.normals[i + 2] = nz;
                    }
                }

                // Texcoord (VEC2) — optional
                if (attrs.contains("TEXCOORD_0"))
                    readAccessorFloats(attrs["TEXCOORD_0"].get<int>(), p.texcoords, 2);

                // --- Read indices ---
                if (prim.contains("indices"))
                    readIndices(prim["indices"].get<int>(), p.indices);

                model.primitives.push_back(p);
            }
        }
    }

    // Recurse into children
    if (node.contains("children"))
    {
        for (const auto &childIdx : node["children"])
            WalkNodes(doc, dirPath, childIdx.get<int>(), worldXform, binData, binLength, model);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Public API
// ═══════════════════════════════════════════════════════════════════════════

bool LoadGLTF(const char *dirPath, GLTFModel &outModel, float targetSize)
{
    outModel.primitives.clear();

    std::string gltfPath = std::string(dirPath) + "/scene.gltf";
    std::string binPath  = std::string(dirPath) + "/scene.bin";

    // ── Parse JSON ──
    std::ifstream f(gltfPath);
    if (!f.is_open())
    {
        fprintf(stderr, "[glTF] ERROR: Could not open %s\n", gltfPath.c_str());
        return false;
    }

    json doc;
    try { f >> doc; }
    catch (const std::exception &e)
    {
        fprintf(stderr, "[glTF] ERROR parsing %s: %s\n", gltfPath.c_str(), e.what());
        return false;
    }

    // ── Read binary buffer ──
    FILE *bin = fopen(binPath.c_str(), "rb");
    if (!bin)
    {
        fprintf(stderr, "[glTF] ERROR: Could not open %s\n", binPath.c_str());
        return false;
    }
    fseek(bin, 0, SEEK_END);
    long binSize = ftell(bin);
    fseek(bin, 0, SEEK_SET);
    unsigned char *binData = new unsigned char[static_cast<size_t>(binSize)];
    if (fread(binData, 1, static_cast<size_t>(binSize), bin) != static_cast<size_t>(binSize))
    {
        fprintf(stderr, "[glTF] ERROR: Short read from %s\n", binPath.c_str());
        delete[] binData;
        fclose(bin);
        return false;
    }
    fclose(bin);

    // ── Walk scene graph ──
    if (!doc.contains("scenes") || doc["scenes"].empty())
    {
        fprintf(stderr, "[glTF] ERROR: No scenes in %s\n", gltfPath.c_str());
        delete[] binData;
        return false;
    }

    int sceneIdx = doc.value("scene", 0);
    const json &scene = doc["scenes"][sceneIdx];
    Mat4 rootXform = Mat4::Identity();

    if (scene.contains("nodes"))
    {
        for (const auto &nodeIdx : scene["nodes"])
            WalkNodes(doc, dirPath, nodeIdx.get<int>(), rootXform, binData, static_cast<int>(binSize), outModel);
    }

    delete[] binData;

    // ── Compute bounding box of all non-skipped primitives ──
    bool hasAny = false;
    float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
    float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;

    for (const auto &prim : outModel.primitives)
    {
        if (prim.skip) continue;
        if (prim.positions.empty()) continue;

        for (size_t i = 0; i + 2 < prim.positions.size(); i += 3)
        {
            float x = prim.positions[i];
            float y = prim.positions[i + 1];
            float z = prim.positions[i + 2];

            if (!hasAny)
            {
                minX = maxX = x;
                minY = maxY = y;
                minZ = maxZ = z;
                hasAny = true;
            }
            else
            {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
                if (z < minZ) minZ = z;
                if (z > maxZ) maxZ = z;
            }
        }
    }

    if (!hasAny)
    {
        fprintf(stderr, "[glTF] WARNING: No geometry found in %s\n", dirPath);
        return false;
    }

    // ── Compute normalize scale and Y offset ──
    float extentX = maxX - minX;
    float extentY = maxY - minY;
    float extentZ = maxZ - minZ;
    float maxExtent = extentX;
    if (extentY > maxExtent) maxExtent = extentY;
    if (extentZ > maxExtent) maxExtent = extentZ;

    if (maxExtent < 0.0001f) maxExtent = 1.0f;

    outModel.normalizeScale = targetSize / maxExtent;
    outModel.offsetY = -minY * outModel.normalizeScale;

    // Apply scale + Y offset to all positions
    for (auto &prim : outModel.primitives)
    {
        if (prim.skip) continue;
        for (size_t i = 0; i + 2 < prim.positions.size(); i += 3)
        {
            float x = prim.positions[i];
            float y = prim.positions[i + 1];
            float z = prim.positions[i + 2];

            x *= outModel.normalizeScale;
            y  = y * outModel.normalizeScale + outModel.offsetY;
            z *= outModel.normalizeScale;

            prim.positions[i]     = x;
            prim.positions[i + 1] = y;
            prim.positions[i + 2] = z;
        }
    }

    // ── Center horizontally so origin (0,0,0) is at bounding box center ──
    float centerX = (minX + maxX) * outModel.normalizeScale * 0.5f;
    float centerZ = (minZ + maxZ) * outModel.normalizeScale * 0.5f;
    for (auto &prim : outModel.primitives)
    {
        if (prim.skip) continue;
        for (size_t i = 0; i + 2 < prim.positions.size(); i += 3)
        {
            prim.positions[i]     -= centerX;
            prim.positions[i + 2] -= centerZ;
        }
    }

    // ── Compute final axis-aligned bounds (after centering) ──
    outModel.boundsHalfX = (maxX - minX) * outModel.normalizeScale * 0.5f;
    outModel.boundsHalfZ = (maxZ - minZ) * outModel.normalizeScale * 0.5f;
    float finalMinY = minY * outModel.normalizeScale + outModel.offsetY;
    float finalMaxY = maxY * outModel.normalizeScale + outModel.offsetY;
    outModel.boundsHalfY = finalMaxY - finalMinY;   // FULL height

    // Safety minimum — jangan sampai 0 agar collision tetap jalan
    if (outModel.boundsHalfX < 0.01f) outModel.boundsHalfX = 0.5f;
    if (outModel.boundsHalfZ < 0.01f) outModel.boundsHalfZ = 0.5f;
    if (outModel.boundsHalfY < 0.01f) outModel.boundsHalfY = 0.5f;

    fprintf(stdout, "[glTF] Loaded %s: %zu primitives, scale=%.4f, offsetY=%.4f, bounds=(%.2f, %.2f, %.2f)\n",
            dirPath, outModel.primitives.size(), outModel.normalizeScale, outModel.offsetY,
            outModel.boundsHalfX, outModel.boundsHalfY, outModel.boundsHalfZ);

    return true;
}

void DrawGLTFModel(const GLTFModel &model, bool shadowMode)
{
    for (const auto &prim : model.primitives)
    {
        if (prim.skip) continue;

        // ── Bind base color texture if present (skip in shadow pass) ──
        bool hadTexture = false;
        if (!shadowMode && prim.glTextureId != 0)
        {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, prim.glTextureId);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            // Reset material to neutral white so texture colors show through
            GLfloat white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,  white);
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,  white);
            hadTexture = true;
        }

        // Set vertex color from material (skip during shadow pass so shadow stays black)
        if (!shadowMode)
            glColor4fv(prim.baseColor);

        if (!prim.indices.empty())
        {
            // Indexed rendering
            glBegin(GL_TRIANGLES);
            for (size_t i = 0; i < prim.indices.size(); ++i)
            {
                unsigned int idx = prim.indices[i];
                size_t posIdx = static_cast<size_t>(idx) * 3;
                size_t normIdx = static_cast<size_t>(idx) * 3;
                size_t texIdx  = static_cast<size_t>(idx) * 2;

                if (idx * 3 + 2 < prim.positions.size())
                {
                    if (idx * 3 + 2 < prim.normals.size())
                        glNormal3f(prim.normals[normIdx], prim.normals[normIdx + 1], prim.normals[normIdx + 2]);

                    if (idx * 2 + 1 < prim.texcoords.size())
                        glTexCoord2f(prim.texcoords[texIdx], prim.texcoords[texIdx + 1]);

                    glVertex3f(prim.positions[posIdx], prim.positions[posIdx + 1], prim.positions[posIdx + 2]);
                }
            }
            glEnd();
        }
        else
        {
            // Non-indexed rendering (assume triangle fan/strip mode? Use GL_TRIANGLES)
            glBegin(GL_TRIANGLES);
            size_t vertexCount = prim.positions.size() / 3;
            for (size_t i = 0; i < vertexCount; ++i)
            {
                size_t posIdx = i * 3;
                size_t normIdx = i * 3;
                size_t texIdx  = i * 2;

                if (posIdx + 2 < prim.normals.size())
                    glNormal3f(prim.normals[normIdx], prim.normals[normIdx + 1], prim.normals[normIdx + 2]);

                if (texIdx + 1 < prim.texcoords.size())
                    glTexCoord2f(prim.texcoords[texIdx], prim.texcoords[texIdx + 1]);

                glVertex3f(prim.positions[posIdx], prim.positions[posIdx + 1], prim.positions[posIdx + 2]);
            }
            glEnd();
        }

        // ── Unbind texture ──
        if (hadTexture)
            glDisable(GL_TEXTURE_2D);
    }
}

void FreeGLTFModel(GLTFModel &model)
{
    for (auto &prim : model.primitives)
    {
        if (prim.glTextureId != 0)
        {
            glDeleteTextures(1, &prim.glTextureId);
            prim.glTextureId = 0;
        }
    }
    model.primitives.clear();
}
