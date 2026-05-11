#pragma once

#include <vector>

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
    TIKAR,
    KIPAS
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
    ObjectType type;
    int count;
};

struct LevelData
{
    int levelNumber;
    const char *roomName;
    const char *clientBrief;
    std::vector<LevelRequirement> requiredItems;
    int budget;
};

struct AppState
{
    AppMode activeMode = AppMode::VIEW_PERSPECTIVE;
    CameraPreset cameraPreset = CameraPreset::ONE_POINT;
    int selectedObjectIndex = 0;
    bool isDragging = false;
    bool smoothShading = true;
    bool directionalLightEnabled = true;
    bool depthTestEnabled = true;
    int lastMouseX = 0;
    int lastMouseY = 0;
    bool titleDirty = true;
    GameState gameState = GameState::MENU;
    int currentLevel = 0;
    int totalSpent = 0;
    int finalScore = 0;
    char failReason[128] = {0};
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
void GetBounds(ObjectType type, float &halfX, float &halfZ);
bool CanPlaceAt(int excludeIndex, ObjectType type, Vec3 position);

void InitializeLevels();
const LevelData &GetCurrentLevel();
GameState EvaluateSubmission();
const char *GetSubTypeLabel(ObjectSubType subType);
