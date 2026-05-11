#pragma once

#include <vector>
#include "gltf_loader.h"

// ---------------------------------------------------------------------------
//  Enums
// ---------------------------------------------------------------------------

enum class AppMode
{
    EDIT_ORTHO,
    VIEW_PERSPECTIVE
};

enum class CameraPreset
{
    ONE_POINT,
    TWO_POINT,
    THREE_POINT,
    FREE
};

enum class ObjectType
{
    CUBE = 0,
    CYLINDER = 1,
    ROAD = 2
};

enum class MaterialType
{
    ROUGH = 0,
    GLOSSY = 1
};

enum class ObjectSubType
{
    NONE = 0,
    SOFA,
    MEJA,
    LEMARI,
    RAK,
    KURSI,
    MEJA_BUNDAR,
    LAMPU,
    KARPET,
    KIPAS
};

enum class BonusRuleType
{
    PROXIMITY,      // jarak pusat target ke anchor < threshold
    OVERLAP_AABB,   // AABB target overlap dengan anchor
    EXCLUSION_ZONE, // tidak ada objek dalam area tertentu
    WITHIN_RANGE,   // objek target dalam jarak threshold dari objek manapun
};

struct BonusRule
{
    BonusRuleType type;
    ObjectSubType target;
    ObjectSubType anchor;     // NONE untuk EXCLUSION_ZONE/WITHIN_RANGE
    float threshold;          // max distance / half-size exclusion
    const char *description;  // teks ditampilkan di HUD
    // Untuk EXCLUSION_ZONE: anchor_x, anchor_z sebagai koordinat pusat
    float zoneX = 0.0f;
    float zoneZ = 0.0f;
};

enum class GameState
{
    MENU,
    PLAYING,
    WIN,
    LOSE
};

// ---------------------------------------------------------------------------
//  Structs
// ---------------------------------------------------------------------------

struct Vec3
{
    float x;
    float y;
    float z;
};

struct SceneObject
{
    ObjectType type;
    ObjectSubType subType = ObjectSubType::NONE;
    Vec3 position;
    float rotationY;
    MaterialType material;
    int cost = 0;
    bool isAnimated = false;
};

struct LevelRequirement
{
    ObjectType type;        // fallback compatibility
    ObjectSubType subType;  // specific furniture required
    int count;
};

struct LevelData
{
    int levelNumber;
    const char *roomName;
    const char *clientBrief;
    std::vector<LevelRequirement> requiredItems;
    std::vector<BonusRule> bonusRules;
    int budget;
    bool autoSpawnFan = true;
};

struct AppState
{
    AppMode activeMode = AppMode::VIEW_PERSPECTIVE;
    CameraPreset cameraPreset = CameraPreset::ONE_POINT;
    int selectedObjectIndex = 0;
    int selectedFurnitureType = 0;  // 0-8, untuk placement
    bool isDragging = false;
    bool smoothShading = true;
    bool directionalLightEnabled = true;
    bool depthTestEnabled = true;
    int lastMouseX = 0;
    int lastMouseY = 0;
    int mouseDownX = 0;
    int mouseDownY = 0;
    bool titleDirty = true;
    GameState gameState = GameState::MENU;
    int currentLevel = 0;
    int totalSpent = 0;
    int finalScore = 0;
    int bonusScore = 0;
    int aestheticsScore = 0;
    int penaltyCount = 0;
    char failReason[128] = {0};
    // Fly-through
    bool isFlyThrough = false;
    float flyThroughT = 0.0f;
};

// ---------------------------------------------------------------------------
//  Global variables — defined in scene.cpp
// ---------------------------------------------------------------------------

extern AppState gState;

extern int gWindowWidth;
extern int gWindowHeight;

extern Vec3 gPointLightPos;
extern Vec3 gViewTarget;
extern float gViewYawDeg;
extern float gViewPitchDeg;
extern float gViewDistance;

extern std::vector<SceneObject> gSceneObjects;
extern std::vector<LevelData> gLevels;
extern bool gKeyDown[256];
extern float gAnimTime;

// glTF model storage — indexed by static_cast<int>(ObjectSubType)
// Only entries SOFA..KIPAS are populated; others are empty.
extern GLTFModel gGLTFModels[16];
void LoadAllGLTFModels();

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

inline constexpr int    kGridHalfSize         = 20;
inline constexpr float  kGridStep             = 1.0f;
inline constexpr float  kOrthoSize            = 12.0f;
inline constexpr float  kRoomSize             = 8.0f;
inline constexpr float  kGridY                = 0.01f;
inline constexpr float  kEditMoveSpeed        = 0.18f;
inline constexpr float  kViewMoveSpeed        = 0.24f;
inline constexpr float  kPi                   = 3.14159265358979323846f;
inline constexpr float  kMouseLookSensitivity = 0.17f;

// ---------------------------------------------------------------------------
//  Function declarations
// ---------------------------------------------------------------------------

float   Clamp(float value, float minValue, float maxValue);
float   ToRadians(float degree);

bool         HasValidSelection();
SceneObject *GetSelectedObject();

const char *ModeLabel();
const char *PresetLabel();

void SetOrbitFromEyeTarget(Vec3 eye, Vec3 target);
void SetPerspectivePreset(CameraPreset preset);

void SelectAdjacentObject(int direction);
void AddSceneObject(ObjectType type, ObjectSubType subType,
                    MaterialType material, Vec3 position,
                    float rotationY, int cost);
void AddNewObjectInEditMode();
void MoveSelectedObject(float deltaX, float deltaZ);
void GetBounds(ObjectSubType subType, float &halfX, float &halfZ, float &halfY);
bool CanPlaceAt(int excludeIndex, ObjectSubType subType, Vec3 position, float rotationY = 0.0f);
void PickObjectAtMouse(int screenX, int screenY);

void InitializeLevels();
const LevelData &GetCurrentLevel();
GameState EvaluateSubmission();
const char *GetSubTypeLabel(ObjectSubType subType);

// Object management
void DeleteSelectedObject();
void SpawnRotatingFan();

// Bonus rule checker (exposed for HUD display)
bool CheckBonusRule(const BonusRule &rule);

// Fly-through
struct FlyWaypoint { Vec3 eye; Vec3 target; };
extern FlyWaypoint gFlyWaypoints[8];
extern const int gFlyWaypointCount;
void UpdateFlyThrough(float deltaT);
