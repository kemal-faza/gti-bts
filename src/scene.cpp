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

void ClampSelectedObject()
{
    SceneObject *selected = GetSelectedObject();
    if (selected == nullptr) return;

    selected->position.x = Clamp(selected->position.x, -kObjectMoveLimit, kObjectMoveLimit);
    selected->position.z = Clamp(selected->position.z, -kObjectMoveLimit, kObjectMoveLimit);
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

void AddSceneObject(const ObjectType type, const MaterialType material,
                    const Vec3 position, const float rotationY)
{
    SceneObject obj;
    obj.type     = type;
    obj.material = material;
    obj.position = position;
    obj.rotationY = rotationY;
    gSceneObjects.push_back(obj);
}

void InitializeSceneData()
{
    gSceneObjects.clear();

    AddSceneObject(ObjectType::ROAD,     MaterialType::ROUGH,  {0.0f, 0.12f, 0.0f}, 0.0f);
    AddSceneObject(ObjectType::CUBE,     MaterialType::GLOSSY, {0.0f, 1.0f,  0.0f}, 0.0f);
    AddSceneObject(ObjectType::CYLINDER, MaterialType::ROUGH,  {4.0f, 0.0f,  2.0f}, 0.0f);

    gState.selectedObjectIndex = 1;
    SetPerspectivePreset(CameraPreset::ONE_POINT);
}

void AddNewObjectInEditMode()
{
    if (gState.activeMode != AppMode::EDIT_ORTHO) return;

    const int pattern = static_cast<int>(gSceneObjects.size()) % 3;
    ObjectType type     = ObjectType::CUBE;
    MaterialType material = MaterialType::GLOSSY;
    Vec3 position = {0.0f, 1.0f, 0.0f};

    if (SceneObject *selected = GetSelectedObject())
    {
        position = selected->position;
        position.x += 2.0f;
    }

    if (pattern == 1)
    {
        type     = ObjectType::CYLINDER;
        material = MaterialType::ROUGH;
        position.y = 0.0f;
    }
    else if (pattern == 2)
    {
        type     = ObjectType::ROAD;
        material = MaterialType::ROUGH;
        position.y = 0.12f;
    }
    else
    {
        type     = ObjectType::CUBE;
        material = MaterialType::GLOSSY;
        position.y = 1.0f;
    }

    AddSceneObject(type, material, position, 0.0f);
    gState.selectedObjectIndex = static_cast<int>(gSceneObjects.size()) - 1;
    ClampSelectedObject();
}

void MoveSelectedObject(const float deltaX, const float deltaZ)
{
    SceneObject *selected = GetSelectedObject();
    if (selected == nullptr) return;

    selected->position.x += deltaX;
    selected->position.z += deltaZ;
    ClampSelectedObject();
}

// ---------------------------------------------------------------------------
//  Game system — level data + submission
// ---------------------------------------------------------------------------

int GetObjectCost(const ObjectType type)
{
    switch (type)
    {
    case ObjectType::CUBE:     return 10;
    case ObjectType::CYLINDER: return 8;
    case ObjectType::ROAD:     return 5;
    }
    return 0;
}

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
        spent += GetObjectCost(obj.type);
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
