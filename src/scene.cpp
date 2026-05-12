#include "scene.h"

#include <GL/gl.h>
#include <GL/glu.h>
#include <cstdio>
#include <cmath>
#include <cstring>

// ── glTF model storage ──
GLTFModel gGLTFModels[16];

void LoadAllGLTFModels()
{
    struct { ObjectSubType subType; const char *dir; float targetSize; } models[] = {
        {ObjectSubType::SOFA,        "assets/object/sofa",           2.5f},
        {ObjectSubType::MEJA,        "assets/object/table",          2.0f},
        {ObjectSubType::LEMARI,      "assets/object/cabinet",        2.0f},
        {ObjectSubType::RAK,         "assets/object/shelf",          1.6f},
        {ObjectSubType::KURSI,       "assets/object/chair",          1.0f},
        {ObjectSubType::MEJA_BUNDAR, "assets/object/rounded-table",  1.5f},
        {ObjectSubType::LAMPU,       "assets/object/lamp",           1.5f},
        {ObjectSubType::KARPET,      "assets/object/carpet",         3.0f},
        {ObjectSubType::KIPAS,       "assets/object/fan",            1.0f},
    };

    for (const auto &m : models)
    {
        int idx = static_cast<int>(m.subType);
        if (!LoadGLTF(m.dir, gGLTFModels[idx], m.targetSize))
            fprintf(stderr, "[Init] Failed to load glTF model: %s\n", m.dir);
    }

    // ── Override shelf color to white (rak tidak punya tekstur) ──
    for (auto &prim : gGLTFModels[static_cast<int>(ObjectSubType::RAK)].primitives)
    {
        if (!prim.skip)
        {
            prim.baseColor[0] = 1.0f;
            prim.baseColor[1] = 1.0f;
            prim.baseColor[2] = 1.0f;
            prim.baseColor[3] = 1.0f;
        }
    }

    fprintf(stdout, "[Init] Loaded %d glTF models\n",
            static_cast<int>(sizeof(models) / sizeof(models[0])));
}

// ---------------------------------------------------------------------------
//  Global definitions
// ---------------------------------------------------------------------------

AppState gState;

int       gWindowWidth  = 960;
int       gWindowHeight = 640;

Vec3 gPointLightPos  = {4.0f, 4.0f, 4.0f};
Vec3 gViewTarget     = {0.0f, 1.5f, 0.0f};
float gViewYawDeg    = 0.0f;
float gViewPitchDeg  = 0.0f;
float gViewDistance  = 10.0f;

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
//  Fly-through waypoints (camera orbit animation)
// ---------------------------------------------------------------------------

FlyWaypoint gFlyWaypoints[8] = {
    {{ 0.0f,  3.0f,  9.0f}, {0.0f, 1.5f,  0.0f}},  // depan
    {{ 7.0f,  3.5f,  7.0f}, {0.0f, 1.5f,  0.0f}},  // kanan+depan
    {{ 9.0f,  4.0f,  0.0f}, {0.0f, 1.5f,  0.0f}},  // kanan
    {{ 7.0f,  4.5f, -7.0f}, {0.0f, 1.5f,  0.0f}},  // kanan+belakang
    {{ 0.0f,  5.0f, -9.0f}, {0.0f, 1.5f,  0.0f}},  // belakang
    {{-7.0f,  4.5f, -7.0f}, {0.0f, 1.5f,  0.0f}},  // kiri+belakang
    {{-9.0f,  4.0f,  0.0f}, {0.0f, 1.5f,  0.0f}},  // kiri
    {{-7.0f,  3.5f,  7.0f}, {0.0f, 1.5f,  0.0f}},  // kiri+depan
};
const int gFlyWaypointCount = 8;

void UpdateFlyThrough(float deltaT)
{
    if (!gState.isFlyThrough) return;

    gState.flyThroughT += deltaT * 0.15f;  // ~8 detik per siklus
    if (gState.flyThroughT >= static_cast<float>(gFlyWaypointCount))
        gState.flyThroughT = 0.0f;  // loop

    int idxA = static_cast<int>(gState.flyThroughT);
    int idxB = (idxA + 1) % gFlyWaypointCount;
    float t = gState.flyThroughT - static_cast<float>(idxA);

    // Smoothstep
    t = t * t * (3.0f - 2.0f * t);

    const FlyWaypoint &a = gFlyWaypoints[idxA];
    const FlyWaypoint &b = gFlyWaypoints[idxB];

    // Lerp eye position
    gViewTarget.x = a.target.x + t * (b.target.x - a.target.x);
    gViewTarget.y = a.target.y + t * (b.target.y - a.target.y);
    gViewTarget.z = a.target.z + t * (b.target.z - a.target.z);

    // Lerp eye → compute yaw/pitch from that
    float ex = a.eye.x + t * (b.eye.x - a.eye.x);
    float ey = a.eye.y + t * (b.eye.y - a.eye.y);
    float ez = a.eye.z + t * (b.eye.z - a.eye.z);

    SetOrbitFromEyeTarget({ex, ey, ez}, gViewTarget);
    gState.cameraPreset = CameraPreset::FREE;
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
        SetOrbitFromEyeTarget({0.0f, 2.0f, 10.0f}, {0.0f, 1.5f, 0.0f});
    else if (preset == CameraPreset::TWO_POINT)
        SetOrbitFromEyeTarget({8.0f, 3.0f, 8.0f}, {0.0f, 1.5f, 0.0f});
    else if (preset == CameraPreset::THREE_POINT)
        SetOrbitFromEyeTarget({7.0f, 8.0f, 7.0f}, {0.0f, 0.5f, 0.0f});
    else
        gState.cameraPreset = CameraPreset::FREE;
}

// ---------------------------------------------------------------------------
//  Object management
// ---------------------------------------------------------------------------

void DeleteSelectedObject()
{
    if (!HasValidSelection()) return;
    gSceneObjects.erase(gSceneObjects.begin() + gState.selectedObjectIndex);
    if (gState.selectedObjectIndex >= static_cast<int>(gSceneObjects.size()))
        gState.selectedObjectIndex = static_cast<int>(gSceneObjects.size()) - 1;
    if (gSceneObjects.empty())
        gState.selectedObjectIndex = 0;
    gState.titleDirty = true;
}



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
    case 8: // KIPAS (kipas berdiri di lantai)
        type = ObjectType::CYLINDER; subType = ObjectSubType::KIPAS;
        material = MaterialType::ROUGH; cost = 8;
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
        gState.penaltyCount++;
        gState.titleDirty = true;
        return;
    }

    AddSceneObject(type, subType, material, position, 0.0f, cost);
    // Set animated flag for kipas berdiri
    if (subType == ObjectSubType::KIPAS)
        gSceneObjects.back().isAnimated = true;
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

    if (!CanPlaceAt(gState.selectedObjectIndex, selected->subType, newPos, selected->rotationY))
    {
        fprintf(stderr, "[Ruang] Tidak bisa geser: area terhalang\n");
        gState.penaltyCount++;
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
    // Gunakan bounds dari glTF model jika tersedia
    int idx = static_cast<int>(subType);
    if (idx > 0 && idx < 16 && !gGLTFModels[idx].primitives.empty())
    {
        halfX = gGLTFModels[idx].boundsHalfX;
        halfZ = gGLTFModels[idx].boundsHalfZ;
        halfY = gGLTFModels[idx].boundsHalfY;
        return;
    }

    // Fallback untuk primitive types (CUBE, CYLINDER, ROAD)
    halfX=0.50f; halfZ=0.50f; halfY=0.50f;
}

// ── Rotate AABB half-extents so collision respects object rotation ──
static void GetRotatedBounds(float hx, float hz, float rotDeg,
                              float &outHx, float &outHz)
{
    const float rad = rotDeg * (kPi / 180.0f);
    const float c = std::cos(rad);
    const float s = std::sin(rad);

    // 4 corners of the AABB in local space
    const float cx[4] = {-hx,  hx, -hx,  hx};
    const float cz[4] = {-hz, -hz,  hz,  hz};

    float minX = 1e9f, maxX = -1e9f;
    float minZ = 1e9f, maxZ = -1e9f;

    for (int i = 0; i < 4; ++i)
    {
        float wx = cx[i] * c - cz[i] * s;
        float wz = cx[i] * s + cz[i] * c;
        if (wx < minX) minX = wx;
        if (wx > maxX) maxX = wx;
        if (wz < minZ) minZ = wz;
        if (wz > maxZ) maxZ = wz;
    }

    outHx = (maxX - minX) * 0.5f;
    outHz = (maxZ - minZ) * 0.5f;
}

bool CanPlaceAt(const int excludeIndex, const ObjectSubType subType,
                const Vec3 position, const float rotationY)
{
    float hw, hd, hh;
    GetBounds(subType, hw, hd, hh);

    // Rotate the new object's bounds if it has a rotation
    float rHw = hw, rHd = hd;
    if (rotationY != 0.0f)
        GetRotatedBounds(hw, hd, rotationY, rHw, rHd);

    // Boundary check (use max extent of rotated footprint)
    const float boundHw = (rHw > hw) ? rHw : hw;
    const float boundHd = (rHd > hd) ? rHd : hd;
    if (position.x - boundHw < -kRoomSize || position.x + boundHw > kRoomSize) return false;
    if (position.z - boundHd < -kRoomSize || position.z + boundHd > kRoomSize) return false;

    // Collision check against all other objects
    for (int i = 0; i < static_cast<int>(gSceneObjects.size()); ++i)
    {
        if (i == excludeIndex) continue;

        const SceneObject &other = gSceneObjects[i];
        float ohw, ohd, ohh;
        GetBounds(other.subType, ohw, ohd, ohh);

        // Rotate existing object's bounds if it has a rotation
        float rOhw = ohw, rOhd = ohd;
        if (other.rotationY != 0.0f)
            GetRotatedBounds(ohw, ohd, other.rotationY, rOhw, rOhd);

        if (position.x - rHw < other.position.x + rOhw &&
            position.x + rHw > other.position.x - rOhw &&
            position.z - rHd < other.position.z + rOhd &&
            position.z + rHd > other.position.z - rOhd)
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
                          "Bonus: meja di atas karpet, sofa dekat meja.\n"
                          "Budget: 50";
        lv.budget       = 50;
        lv.requiredItems = {
            {ObjectType::CUBE, ObjectSubType::SOFA, 2},
            {ObjectType::CUBE, ObjectSubType::MEJA, 1},
            {ObjectType::ROAD, ObjectSubType::KARPET, 1}
        };
        lv.bonusRules = {
            {BonusRuleType::OVERLAP_AABB, ObjectSubType::MEJA, ObjectSubType::KARPET, 0.0f,
             "Karpet di bawah meja (AABB overlap)"},
            {BonusRuleType::PROXIMITY, ObjectSubType::SOFA, ObjectSubType::MEJA, 4.0f,
             "Sofa dekat meja (< 4 unit)"},
        };
        gLevels.push_back(lv);
    }
    {
        LevelData lv;
        lv.levelNumber = 2;
        lv.roomName     = "Ruang Makan";
        lv.clientBrief  = "Klien: Keluarga 4 orang.\n"
                          "Wajib: 1 meja bundar, 4 kursi.\n"
                          "Bonus: kursi dekat meja, kursi tidak menumpuk.\n"
                          "Budget: 60";
        lv.budget       = 60;
        lv.requiredItems = {
            {ObjectType::CYLINDER, ObjectSubType::MEJA_BUNDAR, 1},
            {ObjectType::CUBE, ObjectSubType::KURSI, 4}
        };
        lv.bonusRules = {
            {BonusRuleType::PROXIMITY, ObjectSubType::KURSI, ObjectSubType::MEJA_BUNDAR, 2.5f,
             "Kursi mengelilingi meja (< 2.5 unit)"},
            {BonusRuleType::WITHIN_RANGE, ObjectSubType::KURSI, ObjectSubType::KURSI, 1.0f,
             "Kursi tidak menumpuk (> 1 unit antar kursi)"},
        };
        gLevels.push_back(lv);
    }
    {
        LevelData lv;
        lv.levelNumber = 3;
        lv.roomName     = "Kamar Tidur";
        lv.clientBrief  = "Klien: Mahasiswa kos.\n"
                          "Wajib: 1 lemari, 1 lampu, 1 karpet.\n"
                          "Bonus: lampu dekat lemari, pintu tidak terhalang.\n"
                          "Budget: 45";
        lv.budget       = 45;
        lv.requiredItems = {
            {ObjectType::CUBE, ObjectSubType::LEMARI, 1},
            {ObjectType::CYLINDER, ObjectSubType::LAMPU, 1},
            {ObjectType::ROAD, ObjectSubType::KARPET, 1}
        };
        lv.bonusRules = {
            {BonusRuleType::PROXIMITY, ObjectSubType::LAMPU, ObjectSubType::LEMARI, 3.0f,
             "Lampu dekat lemari (< 3 unit)"},
            {BonusRuleType::EXCLUSION_ZONE, ObjectSubType::NONE, ObjectSubType::NONE, 1.5f,
             "Pintu tidak terhalang (zona 3x3 di (7,-7))"},
        };
        lv.bonusRules.back().zoneX = 3.5f;
        lv.bonusRules.back().zoneZ = -3.5f;
        gLevels.push_back(lv);
    }
}

// ---------------------------------------------------------------------------
//  Bonus rule checking helpers
// ---------------------------------------------------------------------------

static float ObjectCenterDistance(const SceneObject &a, const SceneObject &b)
{
    float dx = a.position.x - b.position.x;
    float dz = a.position.z - b.position.z;
    return std::sqrt(dx * dx + dz * dz);
}

static bool AABBOverlap(const SceneObject &a, const SceneObject &b)
{
    float ax, az, ay, bx, bz, by;
    GetBounds(a.subType, ax, az, ay);
    GetBounds(b.subType, bx, bz, by);
    // Use max extent to account for rotation
    float rax = ax, raz = az, rbx = bx, rbz = bz;
    if (a.rotationY != 0.0f) GetRotatedBounds(ax, az, a.rotationY, rax, raz);
    if (b.rotationY != 0.0f) GetRotatedBounds(bx, bz, b.rotationY, rbx, rbz);
    return (a.position.x - rax < b.position.x + rbx &&
            a.position.x + rax > b.position.x - rbx &&
            a.position.z - raz < b.position.z + rbz &&
            a.position.z + raz > b.position.z - rbz);
}

static int CountOfSubType(ObjectSubType subType)
{
    int c = 0;
    for (const auto &obj : gSceneObjects)
        if (obj.subType == subType) ++c;
    return c;
}

bool CheckBonusRule(const BonusRule &rule)
{
    switch (rule.type)
    {
    case BonusRuleType::PROXIMITY:
    {
        // Find closest pair of (target, anchor)
        float bestDist = 1e9f;
        for (const auto &objA : gSceneObjects)
        {
            if (objA.subType != rule.target) continue;
            for (const auto &objB : gSceneObjects)
            {
                if (objB.subType != rule.anchor) continue;
                if (&objA == &objB) continue;
                float d = ObjectCenterDistance(objA, objB);
                if (d < bestDist) bestDist = d;
            }
        }
        // If WITHIN_RANGE with same type, we check spacing
        if (rule.target == rule.anchor)
            return bestDist > rule.threshold; // "tidak menumpuk"
        return bestDist < rule.threshold;
    }
    case BonusRuleType::OVERLAP_AABB:
    {
        for (const auto &objA : gSceneObjects)
        {
            if (objA.subType != rule.target) continue;
            for (const auto &objB : gSceneObjects)
            {
                if (objB.subType != rule.anchor) continue;
                if (&objA == &objB) continue;
                if (AABBOverlap(objA, objB)) return true;
            }
        }
        return false;
    }
    case BonusRuleType::EXCLUSION_ZONE:
    {
        // No objects within the exclusion zone (centered at zoneX, zoneZ)
        for (const auto &obj : gSceneObjects)
        {
            float dx = obj.position.x - rule.zoneX;
            float dz = obj.position.z - rule.zoneZ;
            float dist = std::sqrt(dx * dx + dz * dz);
            if (dist < rule.threshold) return false;
        }
        return true;
    }
    case BonusRuleType::WITHIN_RANGE:
    {
        // Maximum distance between any two of same type > threshold → means they're not on top of each other
        if (rule.target == rule.anchor)
        {
            // All pairs of same type must have distance > threshold
            for (size_t i = 0; i < gSceneObjects.size(); ++i)
            {
                if (gSceneObjects[i].subType != rule.target) continue;
                for (size_t j = i + 1; j < gSceneObjects.size(); ++j)
                {
                    if (gSceneObjects[j].subType != rule.anchor) continue;
                    float d = ObjectCenterDistance(gSceneObjects[i], gSceneObjects[j]);
                    if (d < rule.threshold) return false;
                }
            }
            return true;
        }
        return false;
    }
    }
    return false;
}

// ---------------------------------------------------------------------------
//  Aesthetics heuristic
// ---------------------------------------------------------------------------

static int ComputeAestheticsScore()
{
    if (gSceneObjects.empty()) return 0;
    // Balance (0-10): center of mass distance from origin
    float cx = 0.0f, cz = 0.0f;
    int count = 0;
    for (const auto &obj : gSceneObjects)
    {
        cx += obj.position.x;
        cz += obj.position.z;
        ++count;
    }
    cx /= static_cast<float>(count);
    cz /= static_cast<float>(count);
    float distFromCenter = std::sqrt(cx * cx + cz * cz);
    int balanceScore = 10 - static_cast<int>(distFromCenter * 2.0f);
    if (balanceScore < 0) balanceScore = 0;

    // Utilization (0-10): how many items spread across the room
    int utilizationScore = (count < 3) ? count * 3 : 10;
    if (utilizationScore > 10) utilizationScore = 10;

    return balanceScore + utilizationScore;
}

// ---------------------------------------------------------------------------
//  Scoring system
// ---------------------------------------------------------------------------

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

    // Reset scoring state
    gState.failReason[0] = '\0';
    gState.bonusScore = 0;
    gState.aestheticsScore = 0;

    // ── Requirement check ──
    bool allMet = true;
    int reqMet = 0, reqTotal = 0;
    for (const auto &req : level.requiredItems)
    {
        reqTotal += req.count;
        int count = CountOfSubType(req.subType);
        if (count >= req.count)
            reqMet += req.count;
        else
            reqMet += count;

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

    // ── Requirement score (50% = 50 points) ──
    int reqScore = 50;

    // ── Bonus rules (30% = 30 points) ──
    int bonusMet = 0;
    int bonusTotal = static_cast<int>(level.bonusRules.size());
    if (bonusTotal > 3) bonusTotal = 3;
    for (const auto &rule : level.bonusRules)
    {
        if (bonusMet >= 3) break;
        if (CheckBonusRule(rule))
            bonusMet++;
    }
    gState.bonusScore = bonusMet * 10;  // +10 per rule, max 30

    // ── Aesthetics (20% = 20 points) ──
    gState.aestheticsScore = ComputeAestheticsScore();
    if (gState.aestheticsScore > 20) gState.aestheticsScore = 20;

    // ── Penalty ──
    int penalty = gState.penaltyCount * 5;
    if (penalty > 50) penalty = 50;  // cap

    // ── Final score ──
    gState.finalScore = reqScore + gState.bonusScore + gState.aestheticsScore - penalty;
    if (gState.finalScore < 0) gState.finalScore = 0;
    if (gState.finalScore > 100) gState.finalScore = 100;

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

        // Transform click point to object's local space (inverse rotation)
        double dx = worldX - static_cast<double>(obj.position.x);
        double dz = worldZ - static_cast<double>(obj.position.z);
        if (obj.rotationY != 0.0f)
        {
            const double rad = static_cast<double>(obj.rotationY) * (kPi / 180.0);
            const double c = std::cos(rad);
            const double s = std::sin(rad);
            const double localX = dx * c + dz * s;
            const double localZ = -dx * s + dz * c;
            dx = localX;
            dz = localZ;
        }

        if (dx >= -static_cast<double>(hx) &&
            dx <=  static_cast<double>(hx) &&
            dz >= -static_cast<double>(hz) &&
            dz <=  static_cast<double>(hz))
        {
            gState.selectedObjectIndex = i;
            gState.titleDirty = true;
            break;
        }
    }
}
