#include "scene.h"

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

void AddNewObjectInEditMode()
{
    if (gState.activeMode != AppMode::EDIT_ORTHO) return;

    const int pattern = static_cast<int>(gSceneObjects.size()) % 3;
    ObjectType type      = ObjectType::CUBE;
    ObjectSubType subType = ObjectSubType::MEJA;
    MaterialType material = MaterialType::GLOSSY;
    int cost = 10;
    Vec3 position = {0.0f, 1.0f, 0.0f};

    if (SceneObject *selected = GetSelectedObject())
    {
        position = selected->position;
        position.x += 2.0f;
    }

    if (pattern == 1)
    {
        type     = ObjectType::CYLINDER;
        subType  = ObjectSubType::MEJA_BUNDAR;
        material = MaterialType::ROUGH;
        cost     = 8;
        position.y = 0.0f;
    }
    else if (pattern == 2)
    {
        type     = ObjectType::ROAD;
        subType  = ObjectSubType::KARPET;
        material = MaterialType::ROUGH;
        cost     = 5;
        position.y = 0.12f;
    }
    else
    {
        type     = ObjectType::CUBE;
        subType  = ObjectSubType::MEJA;
        material = MaterialType::GLOSSY;
        cost     = 10;
        position.y = 1.0f;
    }

    // Collision check sebelum placement
    if (!CanPlaceAt(-1, type, position))
        return;

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

    if (!CanPlaceAt(gState.selectedObjectIndex, selected->type, newPos))
        return;

    selected->position = newPos;
}

// ---------------------------------------------------------------------------
//  Collision detection
// ---------------------------------------------------------------------------

void GetBounds(const ObjectType type, float &halfX, float &halfZ)
{
    switch (type)
    {
    case ObjectType::CUBE:     halfX = 1.0f; halfZ = 1.0f; break;
    case ObjectType::CYLINDER: halfX = 0.7f; halfZ = 0.7f; break;
    case ObjectType::ROAD:     halfX = 3.0f; halfZ = 1.5f; break;
    }
}

bool CanPlaceAt(const int excludeIndex, const ObjectType type, const Vec3 position)
{
    float hw, hd;
    GetBounds(type, hw, hd);

    // Boundary check
    if (position.x - hw < -kRoomSize || position.x + hw > kRoomSize) return false;
    if (position.z - hd < -kRoomSize || position.z + hd > kRoomSize) return false;

    // Collision check against all other objects
    for (int i = 0; i < static_cast<int>(gSceneObjects.size()); ++i)
    {
        if (i == excludeIndex) continue;

        const SceneObject &other = gSceneObjects[i];
        float ohw, ohd;
        GetBounds(other.type, ohw, ohd);

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
            {ObjectType::CUBE, 2},
            {ObjectType::ROAD, 1}
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
            {ObjectType::CYLINDER, 1},
            {ObjectType::CUBE, 4}
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
            {ObjectType::CUBE, 1},
            {ObjectType::CYLINDER, 1},
            {ObjectType::ROAD, 1}
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
            if (obj.type == req.type) ++count;

        if (count < req.count)
        {
            allMet = false;
            if (gState.failReason[0] == '\0')
            {
                std::snprintf(gState.failReason, sizeof(gState.failReason),
                              "Item kurang: butuh %d lagi.", req.count - count);
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
