#include "scene.h"

#include <GL/gl.h>
#include <GL/glu.h>
#include <cstdio>
#include <cmath>

// ---------------------------------------------------------------------------
//  Global definitions
// ---------------------------------------------------------------------------

AppState gState;

int       gWindowWidth  = 960;
int       gWindowHeight = 640;

Vec3 gPointLightPos  = {8.0f, 6.0f, 8.0f};
Vec3 gViewTarget     = {0.0f, 2.0f, 0.0f};
float gViewYawDeg    = 0.0f;
float gViewPitchDeg  = 0.0f;
float gViewDistance  = 18.0f;

std::vector<SceneObject> gSceneObjects;
std::vector<LevelData> gLevels;
bool gKeyDown[256] = {false};
float gAnimTime = 0.0f;

// ---------------------------------------------------------------------------
//  Utility
// ---------------------------------------------------------------------------

float Clamp(const float value, const float minValue, const float maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

float ToRadians(const float degree)
{
    return degree * (kPi / 180.0f);
}

// ---------------------------------------------------------------------------
//  Selection helpers
// ---------------------------------------------------------------------------

bool HasValidSelection()
{
    return !gSceneObjects.empty() && gState.selectedObjectIndex >= 0 &&
           gState.selectedObjectIndex < static_cast<int>(gSceneObjects.size());
}

SceneObject *GetSelectedObject()
{
    if (!HasValidSelection()) return nullptr;
    return &gSceneObjects[gState.selectedObjectIndex];
}

// ---------------------------------------------------------------------------
//  Labels
// ---------------------------------------------------------------------------

const char *ModeLabel()
{
    return gState.activeMode == AppMode::EDIT_ORTHO ? "EDIT_ORTHO" : "VIEW_PERSPECTIVE";
}

const char *PresetLabel()
{
    if (gState.activeMode == AppMode::EDIT_ORTHO)
        return "TOP_DOWN";

    switch (gState.cameraPreset)
    {
    case CameraPreset::ONE_POINT:   return "1_POINT";
    case CameraPreset::TWO_POINT:   return "2_POINT";
    case CameraPreset::THREE_POINT: return "3_POINT";
    case CameraPreset::FREE:        return "FREE";
    }
    return "UNKNOWN";
}

// ---------------------------------------------------------------------------
//  Camera helpers
// ---------------------------------------------------------------------------

void SetOrbitFromEyeTarget(const Vec3 eye, const Vec3 target)
{
    const float dx = eye.x - target.x;
    const float dy = eye.y - target.y;
    const float dz = eye.z - target.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (distance <= 0.0001f) return;

    gViewTarget   = target;
    gViewDistance = distance;
    gViewPitchDeg = std::asin(dy / distance) * 180.0f / kPi;
    gViewYawDeg   = std::atan2(dx, dz) * 180.0f / kPi;
}

void SetPerspectivePreset(const CameraPreset preset)
{
    gState.cameraPreset = preset;

    if (preset == CameraPreset::ONE_POINT)
        SetOrbitFromEyeTarget({0.0f, 2.0f, 18.0f}, {0.0f, 2.0f, 0.0f});
    else if (preset == CameraPreset::TWO_POINT)
        SetOrbitFromEyeTarget({16.0f, 3.0f, 16.0f}, {0.0f, 2.0f, 0.0f});
    else if (preset == CameraPreset::THREE_POINT)
        SetOrbitFromEyeTarget({14.0f, 14.0f, 14.0f}, {0.0f, 0.0f, 0.0f});
    else
        gState.cameraPreset = CameraPreset::FREE;
}

// ---------------------------------------------------------------------------
//  Object management
// ---------------------------------------------------------------------------

void SelectAdjacentObject(const int direction)
{
    if (gSceneObjects.empty())
    {
        gState.selectedObjectIndex = 0;
        return;
    }

    const int count = static_cast<int>(gSceneObjects.size());
    int idx = gState.selectedObjectIndex;
    idx = (idx + direction + count) % count;
    gState.selectedObjectIndex = idx;
}

void AddSceneObject(const ObjectType type, const ObjectSubType subType,
                    const MaterialType material, const Vec3 position,
                    const float rotationY, const int cost)
{
    SceneObject obj;
    obj.type     = type;
    obj.subType  = subType;
    obj.material = material;
    obj.position = position;
    obj.rotationY = rotationY;
    obj.cost     = cost;
    gSceneObjects.push_back(obj);
}

static Vec3 FindFreePosition(const ObjectSubType subType)
{
    // Coba di tengah dulu, lalu spread ke grid
    const float offsets[] = {0.0f, 2.0f, 4.0f, 6.0f};
    for (float o : offsets)
    {
        Vec3 candidates[] = {
            {0.0f,   0.0f, 0.0f},
            { o,     0.0f, 0.0f},
            {-o,     0.0f, 0.0f},
            {0.0f,   0.0f,  o},
            {0.0f,   0.0f, -o},
            { o,     0.0f,  o},
            { o,     0.0f, -o},
            {-o,     0.0f,  o},
            {-o,     0.0f, -o},
        };
        for (const auto &pos : candidates)
        {
            if (CanPlaceAt(-1, subType, pos))
                return pos;
        }
    }
    // Fallback: tempatkan di (0,0,0) walaupun collided
    return {0.0f, 0.0f, 0.0f};
}

void AddNewObjectInEditMode()
{
    if (gState.activeMode != AppMode::EDIT_ORTHO) return;

    // Use the user-selected furniture type
    const int typeIdx = gState.selectedFurnitureType;
    ObjectType type      = ObjectType::CUBE;
    ObjectSubType subType = ObjectSubType::MEJA;
    MaterialType material = MaterialType::GLOSSY;
    int cost = 10;

    switch (typeIdx)
    {
    case 0: // MEJA
        type = ObjectType::CUBE; subType = ObjectSubType::MEJA;
        material = MaterialType::GLOSSY; cost = 10;
        break;
    case 1: // SOFA
        type = ObjectType::CUBE; subType = ObjectSubType::SOFA;
        material = MaterialType::ROUGH; cost = 10;
        break;
    case 2: // KURSI
        type = ObjectType::CUBE; subType = ObjectSubType::KURSI;
        material = MaterialType::ROUGH; cost = 8;
        break;
    case 3: // MEJA_BUNDAR
        type = ObjectType::CYLINDER; subType = ObjectSubType::MEJA_BUNDAR;
        material = MaterialType::ROUGH; cost = 8;
        break;
    case 4: // LEMARI
        type = ObjectType::CUBE; subType = ObjectSubType::LEMARI;
        material = MaterialType::GLOSSY; cost = 12;
        break;
    case 5: // RAK
        type = ObjectType::CUBE; subType = ObjectSubType::RAK;
        material = MaterialType::GLOSSY; cost = 8;
        break;
    case 6: // KARPET
        type = ObjectType::ROAD; subType = ObjectSubType::KARPET;
        material = MaterialType::ROUGH; cost = 5;
        break;
    case 7: // LAMPU
        type = ObjectType::CYLINDER; subType = ObjectSubType::LAMPU;
        material = MaterialType::ROUGH; cost = 6;
        break;
    case 8: // TIKAR
        type = ObjectType::ROAD; subType = ObjectSubType::TIKAR;
        material = MaterialType::ROUGH; cost = 3;
        break;
    default:
        type = ObjectType::CUBE; subType = ObjectSubType::MEJA;
        material = MaterialType::GLOSSY; cost = 10;
        break;
    }

    // Cari posisi kosong
    Vec3 position = FindFreePosition(subType);

    // Collision check sebelum placement
    if (!CanPlaceAt(-1, subType, position))
    {
        fprintf(stderr, "[Ruang] Tidak bisa menempatkan: area terhalang atau di luar ruangan\n");
        gState.titleDirty = true;
        return;
    }

    AddSceneObject(type, subType, material, position, 0.0f, cost);
    gState.selectedObjectIndex = static_cast<int>(gSceneObjects.size()) - 1;
}

void MoveSelectedObject(const float deltaX, const float deltaZ)
{
    SceneObject *selected = GetSelectedObject();
    if (selected == nullptr) return;

    Vec3 newPos = selected->position;
    newPos.x += deltaX;
    newPos.z += deltaZ;

    newPos.x = Clamp(newPos.x, -kRoomSize, kRoomSize);
    newPos.z = Clamp(newPos.z, -kRoomSize, kRoomSize);

    if (!CanPlaceAt(gState.selectedObjectIndex, selected->subType, newPos))
    {
        fprintf(stderr, "[Ruang] Tidak bisa geser: area terhalang\n");
        gState.titleDirty = true;
        return;
    }

    selected->position = newPos;
}

// ---------------------------------------------------------------------------
//  Collision detection
// ---------------------------------------------------------------------------

void GetBounds(const ObjectSubType subType,
               float &halfX, float &halfZ, float &halfY)
{
    switch (subType)
    {
    case ObjectSubType::MEJA:        halfX=1.00f; halfZ=0.60f; halfY=1.00f; break;
    case ObjectSubType::SOFA:        halfX=1.10f; halfZ=0.45f; halfY=0.75f; break;
    case ObjectSubType::KURSI:       halfX=0.50f; halfZ=0.50f; halfY=1.00f; break;
    case ObjectSubType::LEMARI:      halfX=0.70f; halfZ=0.28f; halfY=2.40f; break;
    case ObjectSubType::RAK:         halfX=0.65f; halfZ=0.18f; halfY=1.40f; break;
    case ObjectSubType::MEJA_BUNDAR: halfX=0.75f; halfZ=0.75f; halfY=1.04f; break;
    case ObjectSubType::LAMPU:       halfX=0.22f; halfZ=0.22f; halfY=1.33f; break;
    case ObjectSubType::KARPET:      halfX=1.50f; halfZ=1.00f; halfY=0.04f; break;
    case ObjectSubType::TIKAR:       halfX=1.00f; halfZ=0.60f; halfY=0.02f; break;
    case ObjectSubType::KIPAS:       halfX=0.12f; halfZ=0.12f; halfY=0.15f; break;
    default:
        // Fallback to legacy ObjectType-based — gunakan subType NONE
        halfX=0.50f; halfZ=0.50f; halfY=0.50f; break;
    }
}

bool CanPlaceAt(const int excludeIndex, const ObjectSubType subType, const Vec3 position)
{
    float hw, hd, hh;
    GetBounds(subType, hw, hd, hh);

    // Boundary check
    if (position.x - hw < -kRoomSize || position.x + hw > kRoomSize) return false;
    if (position.z - hd < -kRoomSize || position.z + hd > kRoomSize) return false;

    // Collision check against all other objects
    for (int i = 0; i < static_cast<int>(gSceneObjects.size()); ++i)
    {
        if (i == excludeIndex) continue;

        const SceneObject &other = gSceneObjects[i];
        float ohw, ohd, ohh;
        GetBounds(other.subType, ohw, ohd, ohh);

        if (position.x - hw < other.position.x + ohw &&
            position.x + hw > other.position.x - ohw &&
            position.z - hd < other.position.z + ohd &&
            position.z + hd > other.position.z - ohd)
        {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  Game system — level data + submission
// ---------------------------------------------------------------------------

void InitializeLevels()
{
    gLevels.clear();

    {
        LevelData lv;
        lv.levelNumber = 1;
        lv.roomName     = "Ruang Tamu";
        lv.clientBrief  = "Klien: Pasangan muda menginginkan ruang tamu minimalis.\n"
                          "Wajib: 2 sofa, 1 meja, 1 karpet.\n"
                          "Budget: 50";
        lv.budget       = 50;
        lv.requiredItems = {
            {ObjectType::CUBE, ObjectSubType::SOFA, 2},
            {ObjectType::CUBE, ObjectSubType::MEJA, 1},
            {ObjectType::ROAD, ObjectSubType::KARPET, 1}
        };
        gLevels.push_back(lv);
    }
    {
        LevelData lv;
        lv.levelNumber = 2;
        lv.roomName     = "Ruang Makan";
        lv.clientBrief  = "Klien: Keluarga 4 orang.\n"
                          "Wajib: 1 meja bundar, 4 kursi.\n"
                          "Budget: 60";
        lv.budget       = 60;
        lv.requiredItems = {
            {ObjectType::CYLINDER, ObjectSubType::MEJA_BUNDAR, 1},
            {ObjectType::CUBE, ObjectSubType::KURSI, 4}
        };
        gLevels.push_back(lv);
    }
    {
        LevelData lv;
        lv.levelNumber = 3;
        lv.roomName     = "Kamar Tidur";
        lv.clientBrief  = "Klien: Mahasiswa kos.\n"
                          "Wajib: 1 lemari, 1 lampu, 1 karpet.\n"
                          "Budget: 45";
        lv.budget       = 45;
        lv.requiredItems = {
            {ObjectType::CUBE, ObjectSubType::LEMARI, 1},
            {ObjectType::CYLINDER, ObjectSubType::LAMPU, 1},
            {ObjectType::ROAD, ObjectSubType::KARPET, 1}
        };
        gLevels.push_back(lv);
    }
}

const LevelData &GetCurrentLevel()
{
    return gLevels[gState.currentLevel];
}

GameState EvaluateSubmission()
{
    // Hitung total cost
    int spent = 0;
    for (const auto &obj : gSceneObjects)
        spent += obj.cost;
    gState.totalSpent = spent;

    const LevelData &level = GetCurrentLevel();

    // Satu loop: hitung requirement sekaligus cari penyebab gagal
    bool allMet = true;
    gState.failReason[0] = '\0';

    for (const auto &req : level.requiredItems)
    {
        int count = 0;
        for (const auto &obj : gSceneObjects)
        {
            if (req.subType != ObjectSubType::NONE)
            {
                if (obj.subType == req.subType) ++count;
            }
            else
            {
                if (obj.type == req.type) ++count;
            }
        }

        if (count < req.count)
        {
            allMet = false;
            if (gState.failReason[0] == '\0')
            {
                std::snprintf(gState.failReason, sizeof(gState.failReason),
                              "Item kurang: butuh %d %s lagi.",
                              req.count - count,
                              GetSubTypeLabel(req.subType));
            }
        }
    }

    if (!allMet)
    {
        gState.finalScore = 0;
        return GameState::LOSE;
    }

    if (spent > level.budget)
    {
        gState.finalScore = 0;
        std::snprintf(gState.failReason, sizeof(gState.failReason),
                      "Budget melebihi: %d / %d", spent, level.budget);
        return GameState::LOSE;
    }

    // WIN
    gState.finalScore = 100;
    return GameState::WIN;
}

const char *GetSubTypeLabel(const ObjectSubType subType)
{
    switch (subType)
    {
    case ObjectSubType::SOFA:        return "Sofa";
    case ObjectSubType::MEJA:        return "Meja";
    case ObjectSubType::LEMARI:      return "Lemari";
    case ObjectSubType::RAK:         return "Rak";
    case ObjectSubType::KURSI:       return "Kursi";
    case ObjectSubType::MEJA_BUNDAR: return "Meja Bundar";
    case ObjectSubType::LAMPU:       return "Lampu";
    case ObjectSubType::KARPET:      return "Karpet";
    case ObjectSubType::TIKAR:       return "Tikar";
    case ObjectSubType::KIPAS:       return "Kipas";
    default:                         return "Objek";
    }
}

void PickObjectAtMouse(const int screenX, const int screenY)
{
    if (gState.activeMode != AppMode::EDIT_ORTHO) return;
    if (gState.gameState != GameState::PLAYING) return;

    // Set up ortho projection matrix (same as ConfigureProjectionMatrix for EDIT_ORTHO)
    const float aspect = (gWindowHeight == 0)
                             ? 1.0f
                             : static_cast<float>(gWindowWidth) / static_cast<float>(gWindowHeight);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-kOrthoSize * aspect, kOrthoSize * aspect,
            -kOrthoSize, kOrthoSize, -100.0, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    gluLookAt(0.0, 24.0, 0.0,
              0.0,  0.0, 0.0,
              0.0,  0.0, -1.0);

    // Get matrices for unproject
    GLdouble modelview[16], projection[16];
    GLint viewport[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    // Restore matrices
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    // GLUT Y is top-down, OpenGL viewport Y is bottom-up
    const double winY = static_cast<double>(viewport[3]) - static_cast<double>(screenY);

    GLdouble nearX, nearY, nearZ, farX, farY, farZ;
    gluUnProject(static_cast<double>(screenX), winY, 0.0,
                 modelview, projection, viewport,
                 &nearX, &nearY, &nearZ);
    gluUnProject(static_cast<double>(screenX), winY, 1.0,
                 modelview, projection, viewport,
                 &farX, &farY, &farZ);

    // Intersect ray with plane y=0
    // P = near + t*(far-near), find t where y=0
    const double dy = farY - nearY;
    if (std::abs(dy) < 0.000001) return;
    const double t = -nearY / dy;
    const double worldX = nearX + t * (farX - nearX);
    const double worldZ = nearZ + t * (farZ - nearZ);

    // Iterate in reverse (topmost object last in array = highest index)
    for (int i = static_cast<int>(gSceneObjects.size()) - 1; i >= 0; --i)
    {
        const SceneObject &obj = gSceneObjects[i];
        float hx, hz, hy;
        GetBounds(obj.subType, hx, hz, hy);

        if (worldX >= static_cast<double>(obj.position.x - hx) &&
            worldX <= static_cast<double>(obj.position.x + hx) &&
            worldZ >= static_cast<double>(obj.position.z - hz) &&
            worldZ <= static_cast<double>(obj.position.z + hz))
        {
            gState.selectedObjectIndex = i;
            gState.titleDirty = true;
            break;
        }
    }
}
