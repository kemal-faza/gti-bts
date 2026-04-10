#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{
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

    struct Vec3
    {
        float x;
        float y;
        float z;
    };

    struct SceneObject
    {
        ObjectType type;
        Vec3 position;
        float rotationY;
        MaterialType material;
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
    };

    AppState gState;

    int gWindowWidth = 960;
    int gWindowHeight = 640;

    constexpr int kGridHalfSize = 20;
    constexpr float kGridStep = 1.0f;
    constexpr float kOrthoSize = 12.0f;
    constexpr float kObjectMoveLimit = 18.0f;
    constexpr float kGridY = 0.01f;
    constexpr float kEditMoveSpeed = 0.18f;
    constexpr float kViewMoveSpeed = 0.24f;
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kMouseLookSensitivity = 0.17f;

    Vec3 gPointLightPos = {8.0f, 6.0f, 8.0f};
    Vec3 gViewTarget = {0.0f, 2.0f, 0.0f};
    float gViewYawDeg = 0.0f;
    float gViewPitchDeg = 0.0f;
    float gViewDistance = 18.0f;

    std::vector<SceneObject> gSceneObjects;
    bool gKeyDown[256] = {false};

    float Clamp(const float value, const float minValue, const float maxValue)
    {
        if (value < minValue)
        {
            return minValue;
        }
        if (value > maxValue)
        {
            return maxValue;
        }
        return value;
    }

    float ToRadians(const float degree)
    {
        return degree * (kPi / 180.0f);
    }

    void ApplyShadingMode()
    {
        glShadeModel(gState.smoothShading ? GL_SMOOTH : GL_FLAT);
    }

    void ApplyDepthTestMode()
    {
        if (gState.depthTestEnabled)
        {
            glEnable(GL_DEPTH_TEST);
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }
    }

    bool HasValidSelection()
    {
        return !gSceneObjects.empty() && gState.selectedObjectIndex >= 0 && gState.selectedObjectIndex < static_cast<int>(gSceneObjects.size());
    }

    SceneObject *GetSelectedObject()
    {
        if (!HasValidSelection())
        {
            return nullptr;
        }
        return &gSceneObjects[gState.selectedObjectIndex];
    }

    void ClampSelectedObject()
    {
        SceneObject *selected = GetSelectedObject();
        if (selected == nullptr)
        {
            return;
        }

        selected->position.x = Clamp(selected->position.x, -kObjectMoveLimit, kObjectMoveLimit);
        selected->position.z = Clamp(selected->position.z, -kObjectMoveLimit, kObjectMoveLimit);
    }

    const char *ModeLabel()
    {
        return gState.activeMode == AppMode::EDIT_ORTHO ? "EDIT_ORTHO" : "VIEW_PERSPECTIVE";
    }

    const char *PresetLabel()
    {
        if (gState.activeMode == AppMode::EDIT_ORTHO)
        {
            return "TOP_DOWN";
        }

        switch (gState.cameraPreset)
        {
        case CameraPreset::ONE_POINT:
            return "1_POINT";
        case CameraPreset::TWO_POINT:
            return "2_POINT";
        case CameraPreset::THREE_POINT:
            return "3_POINT";
        case CameraPreset::FREE:
            return "FREE";
        }

        return "UNKNOWN";
    }

    void UpdateWindowTitle()
    {
        char title[512];
        std::snprintf(title,
                      sizeof(title),
                      "GTI BTS | Mode:%s | Cam:%s | Sel:%d/%zu | L:%s | Shading:%s | Depth:%s | Tab Mode | [/] Select | Space Add | 1/2/3 Preset | WASD+QE Move | L Light | F Shading | Z Depth | Hold Left Drag",
                      ModeLabel(),
                      PresetLabel(),
                      gSceneObjects.empty() ? 0 : (gState.selectedObjectIndex + 1),
                      gSceneObjects.size(),
                      gState.directionalLightEnabled ? "ON" : "OFF",
                      gState.smoothShading ? "SMOOTH" : "FLAT",
                      gState.depthTestEnabled ? "ON" : "OFF");
        glutSetWindowTitle(title);
    }

    void SetOrbitFromEyeTarget(const Vec3 eye, const Vec3 target)
    {
        const float dx = eye.x - target.x;
        const float dy = eye.y - target.y;
        const float dz = eye.z - target.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (distance <= 0.0001f)
        {
            return;
        }

        gViewTarget = target;
        gViewDistance = distance;
        gViewPitchDeg = std::asin(dy / distance) * 180.0f / kPi;
        gViewYawDeg = std::atan2(dx, dz) * 180.0f / kPi;
    }

    void SetPerspectivePreset(const CameraPreset preset)
    {
        gState.cameraPreset = preset;

        if (preset == CameraPreset::ONE_POINT)
        {
            SetOrbitFromEyeTarget({0.0f, 2.0f, 18.0f}, {0.0f, 2.0f, 0.0f});
        }
        else if (preset == CameraPreset::TWO_POINT)
        {
            SetOrbitFromEyeTarget({16.0f, 3.0f, 16.0f}, {0.0f, 2.0f, 0.0f});
        }
        else if (preset == CameraPreset::THREE_POINT)
        {
            SetOrbitFromEyeTarget({14.0f, 14.0f, 14.0f}, {0.0f, 0.0f, 0.0f});
        }
        else
        {
            gState.cameraPreset = CameraPreset::FREE;
        }
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

    void AddSceneObject(const ObjectType type, const MaterialType material, const Vec3 position, const float rotationY)
    {
        SceneObject obj;
        obj.type = type;
        obj.material = material;
        obj.position = position;
        obj.rotationY = rotationY;
        gSceneObjects.push_back(obj);
    }

    void InitializeSceneData()
    {
        gSceneObjects.clear();

        AddSceneObject(ObjectType::ROAD, MaterialType::ROUGH, {0.0f, 0.12f, 0.0f}, 0.0f);
        AddSceneObject(ObjectType::CUBE, MaterialType::GLOSSY, {0.0f, 1.0f, 0.0f}, 0.0f);
        AddSceneObject(ObjectType::CYLINDER, MaterialType::ROUGH, {4.0f, 0.0f, 2.0f}, 0.0f);

        gState.selectedObjectIndex = 1;
        SetPerspectivePreset(CameraPreset::ONE_POINT);
    }

    void AddNewObjectInEditMode()
    {
        if (gState.activeMode != AppMode::EDIT_ORTHO)
        {
            return;
        }

        const int pattern = static_cast<int>(gSceneObjects.size()) % 3;
        ObjectType type = ObjectType::CUBE;
        MaterialType material = MaterialType::GLOSSY;
        Vec3 position = {0.0f, 1.0f, 0.0f};

        if (SceneObject *selected = GetSelectedObject())
        {
            position = selected->position;
            position.x += 2.0f;
        }

        if (pattern == 1)
        {
            type = ObjectType::CYLINDER;
            material = MaterialType::ROUGH;
            position.y = 0.0f;
        }
        else if (pattern == 2)
        {
            type = ObjectType::ROAD;
            material = MaterialType::ROUGH;
            position.y = 0.12f;
        }
        else
        {
            type = ObjectType::CUBE;
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
        if (selected == nullptr)
        {
            return;
        }

        selected->position.x += deltaX;
        selected->position.z += deltaZ;
        ClampSelectedObject();
    }

    void ConfigureProjectionMatrix()
    {
        const float aspect = (gWindowHeight == 0) ? 1.0f : static_cast<float>(gWindowWidth) / static_cast<float>(gWindowHeight);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        if (gState.activeMode == AppMode::EDIT_ORTHO)
        {
            glOrtho(-kOrthoSize * aspect, kOrthoSize * aspect, -kOrthoSize, kOrthoSize, -100.0, 100.0);
        }
        else
        {
            gluPerspective(60.0, aspect, 0.1, 200.0);
        }
    }

    void ApplyCameraViewMatrix()
    {
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        if (gState.activeMode == AppMode::EDIT_ORTHO)
        {
            gluLookAt(0.0, 24.0, 0.0,
                      0.0, 0.0, 0.0,
                      0.0, 0.0, -1.0);
            return;
        }

        const float yawRad = ToRadians(gViewYawDeg);
        const float pitchRad = ToRadians(gViewPitchDeg);
        const float cosPitch = std::cos(pitchRad);

        const Vec3 eye = {
            gViewTarget.x + std::sin(yawRad) * cosPitch * gViewDistance,
            gViewTarget.y + std::sin(pitchRad) * gViewDistance,
            gViewTarget.z + std::cos(yawRad) * cosPitch * gViewDistance};

        gluLookAt(eye.x, eye.y, eye.z,
                  gViewTarget.x, gViewTarget.y, gViewTarget.z,
                  0.0, 1.0, 0.0);
    }

    void ApplyObjectMaterial(const MaterialType material, const bool selected)
    {
        GLfloat ambient[4] = {0.10f, 0.10f, 0.10f, 1.0f};
        GLfloat diffuse[4] = {0.50f, 0.50f, 0.50f, 1.0f};
        GLfloat specular[4] = {0.08f, 0.08f, 0.08f, 1.0f};
        GLfloat emission[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        float shininess = 8.0f;

        if (material == MaterialType::GLOSSY)
        {
            ambient[0] = 0.20f;
            ambient[1] = 0.07f;
            ambient[2] = 0.03f;
            diffuse[0] = 0.90f;
            diffuse[1] = 0.34f;
            diffuse[2] = 0.18f;
            specular[0] = 0.95f;
            specular[1] = 0.95f;
            specular[2] = 0.95f;
            shininess = 96.0f;
        }

        if (selected && gState.activeMode == AppMode::EDIT_ORTHO)
        {
            emission[0] = 0.12f;
            emission[1] = 0.12f;
            emission[2] = 0.22f;
        }

        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emission);
    }

    void SetupLights()
    {
        glEnable(GL_LIGHTING);

        if (gState.directionalLightEnabled)
        {
            glEnable(GL_LIGHT0);
        }
        else
        {
            glDisable(GL_LIGHT0);
        }
        glEnable(GL_LIGHT1);

        const GLfloat globalAmbient[] = {0.08f, 0.08f, 0.09f, 1.0f};
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

        const GLfloat dirAmbient[] = {0.08f, 0.08f, 0.07f, 1.0f};
        const GLfloat dirDiffuse[] = {0.75f, 0.72f, 0.66f, 1.0f};
        const GLfloat dirSpecular[] = {0.40f, 0.38f, 0.34f, 1.0f};
        const GLfloat dirPosition[] = {-0.45f, 1.0f, 0.25f, 0.0f};
        glLightfv(GL_LIGHT0, GL_AMBIENT, dirAmbient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, dirDiffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, dirSpecular);
        glLightfv(GL_LIGHT0, GL_POSITION, dirPosition);

        const GLfloat pointAmbient[] = {0.05f, 0.04f, 0.03f, 1.0f};
        const GLfloat pointDiffuse[] = {1.0f, 0.90f, 0.70f, 1.0f};
        const GLfloat pointSpecular[] = {1.0f, 0.95f, 0.80f, 1.0f};
        const GLfloat pointPosition[] = {gPointLightPos.x, gPointLightPos.y, gPointLightPos.z, 1.0f};
        glLightfv(GL_LIGHT1, GL_AMBIENT, pointAmbient);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, pointDiffuse);
        glLightfv(GL_LIGHT1, GL_SPECULAR, pointSpecular);
        glLightfv(GL_LIGHT1, GL_POSITION, pointPosition);
        glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 1.0f);
        glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.03f);
        glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.01f);
    }

    void DrawCylinderPrimitive(const float radius, const float height)
    {
        GLUquadric *quad = gluNewQuadric();
        if (quad == nullptr)
        {
            return;
        }

        gluQuadricNormals(quad, GLU_SMOOTH);

        glPushMatrix();
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        gluCylinder(quad, radius, radius, height, 24, 1);

        glPushMatrix();
        glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
        gluDisk(quad, 0.0, radius, 24, 1);
        glPopMatrix();

        glTranslatef(0.0f, 0.0f, height);
        gluDisk(quad, 0.0, radius, 24, 1);
        glPopMatrix();

        gluDeleteQuadric(quad);
    }

    void DrawObjectGeometry(const SceneObject &obj)
    {
        switch (obj.type)
        {
        case ObjectType::CUBE:
            glutSolidCube(2.0);
            break;
        case ObjectType::CYLINDER:
            DrawCylinderPrimitive(0.7f, 2.2f);
            break;
        case ObjectType::ROAD:
            glScalef(6.0f, 0.24f, 3.0f);
            glutSolidCube(1.0f);
            break;
        }
    }

    void DrawGrid()
    {
        const GLboolean lightingEnabled = glIsEnabled(GL_LIGHTING);
        if (lightingEnabled)
        {
            glDisable(GL_LIGHTING);
        }

        glColor3f(0.35f, 0.35f, 0.35f);
        glBegin(GL_LINES);
        for (int i = -kGridHalfSize; i <= kGridHalfSize; ++i)
        {
            const float k = i * kGridStep;
            glVertex3f(k, kGridY, -kGridHalfSize * kGridStep);
            glVertex3f(k, kGridY, kGridHalfSize * kGridStep);

            glVertex3f(-kGridHalfSize * kGridStep, kGridY, k);
            glVertex3f(kGridHalfSize * kGridStep, kGridY, k);
        }
        glEnd();

        if (lightingEnabled)
        {
            glEnable(GL_LIGHTING);
        }
    }

    void DrawSceneObjects()
    {
        for (int i = 0; i < static_cast<int>(gSceneObjects.size()); ++i)
        {
            const SceneObject &obj = gSceneObjects[i];
            const bool isSelected = (i == gState.selectedObjectIndex);

            glPushMatrix();
            glTranslatef(obj.position.x, obj.position.y, obj.position.z);
            glRotatef(obj.rotationY, 0.0f, 1.0f, 0.0f);
            ApplyObjectMaterial(obj.material, isSelected);
            DrawObjectGeometry(obj);
            glPopMatrix();
        }

        const GLfloat noEmission[] = {0.0f, 0.0f, 0.0f, 1.0f};
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, noEmission);
    }

    void DrawPointLightMarker()
    {
        glPushMatrix();
        glTranslatef(gPointLightPos.x, gPointLightPos.y, gPointLightPos.z);

        const GLfloat ambient[] = {0.20f, 0.16f, 0.08f, 1.0f};
        const GLfloat diffuse[] = {0.92f, 0.82f, 0.45f, 1.0f};
        const GLfloat specular[] = {0.0f, 0.0f, 0.0f, 1.0f};
        const GLfloat emission[] = {0.95f, 0.80f, 0.35f, 1.0f};
        const GLfloat noEmission[] = {0.0f, 0.0f, 0.0f, 1.0f};

        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 4.0f);
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emission);
        glutSolidSphere(0.35, 18, 18);
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, noEmission);

        glPopMatrix();
    }

    void Display()
    {
        // Step 1: clear color and depth buffer.
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Step 2: determine projection matrix.
        ConfigureProjectionMatrix();

        // Step 3: determine camera view matrix.
        ApplyCameraViewMatrix();

        // Step 4: place and compute lights in world space.
        SetupLights();

        // Step 5: render geometry by looping scene data.
        DrawGrid();
        DrawSceneObjects();
        DrawPointLightMarker();

        // Step 6: swap buffers.
        glutSwapBuffers();
    }

    void Reshape(int width, int height)
    {
        if (width <= 0 || height <= 0)
        {
            return;
        }

        gWindowWidth = width;
        gWindowHeight = height;
        glViewport(0, 0, width, height);
        glutPostRedisplay();
    }

    void KeyboardDown(unsigned char key, int, int)
    {
        const unsigned char lowered = static_cast<unsigned char>(std::tolower(key));

        if (key == 27)
        {
            std::exit(0);
        }

        if (key == '\t')
        {
            if (gState.activeMode == AppMode::EDIT_ORTHO)
            {
                gState.activeMode = AppMode::VIEW_PERSPECTIVE;
            }
            else
            {
                gState.activeMode = AppMode::EDIT_ORTHO;
            }

            gState.isDragging = false;
            glutSetCursor(GLUT_CURSOR_LEFT_ARROW);
            UpdateWindowTitle();
            glutPostRedisplay();
            return;
        }

        if (gState.activeMode == AppMode::EDIT_ORTHO)
        {
            if (key == ' ')
            {
                AddNewObjectInEditMode();
                UpdateWindowTitle();
                glutPostRedisplay();
                return;
            }

            if (key == '[')
            {
                SelectAdjacentObject(-1);
                UpdateWindowTitle();
                glutPostRedisplay();
                return;
            }

            if (key == ']')
            {
                SelectAdjacentObject(1);
                UpdateWindowTitle();
                glutPostRedisplay();
                return;
            }

            if (lowered == 'w' || lowered == 'a' || lowered == 's' || lowered == 'd')
            {
                gKeyDown[lowered] = true;
            }
            return;
        }

        // VIEW_PERSPECTIVE input state transitions.
        if (key == '1')
        {
            SetPerspectivePreset(CameraPreset::ONE_POINT);
            UpdateWindowTitle();
            glutPostRedisplay();
            return;
        }
        if (key == '2')
        {
            SetPerspectivePreset(CameraPreset::TWO_POINT);
            UpdateWindowTitle();
            glutPostRedisplay();
            return;
        }
        if (key == '3')
        {
            SetPerspectivePreset(CameraPreset::THREE_POINT);
            UpdateWindowTitle();
            glutPostRedisplay();
            return;
        }
        if (lowered == 'l')
        {
            gState.directionalLightEnabled = !gState.directionalLightEnabled;
            UpdateWindowTitle();
            glutPostRedisplay();
            return;
        }
        if (lowered == 'f')
        {
            gState.smoothShading = !gState.smoothShading;
            ApplyShadingMode();
            UpdateWindowTitle();
            glutPostRedisplay();
            return;
        }
        if (lowered == 'z')
        {
            gState.depthTestEnabled = !gState.depthTestEnabled;
            ApplyDepthTestMode();
            UpdateWindowTitle();
            glutPostRedisplay();
            return;
        }

        if (lowered == 'w' || lowered == 'a' || lowered == 's' || lowered == 'd' || lowered == 'q' || lowered == 'e')
        {
            gKeyDown[lowered] = true;
        }
    }

    void KeyboardUp(unsigned char key, int, int)
    {
        const unsigned char lowered = static_cast<unsigned char>(std::tolower(key));
        gKeyDown[lowered] = false;
    }

    void MouseButton(int button, int state, int x, int y)
    {
        if (button != GLUT_LEFT_BUTTON)
        {
            return;
        }

        if (state == GLUT_DOWN)
        {
            gState.isDragging = true;
            gState.lastMouseX = x;
            gState.lastMouseY = y;
            glutSetCursor(GLUT_CURSOR_INFO);
        }
        else if (state == GLUT_UP)
        {
            gState.isDragging = false;
            glutSetCursor(GLUT_CURSOR_LEFT_ARROW);
        }
    }

    void MouseDrag(int x, int y)
    {
        if (!gState.isDragging)
        {
            return;
        }

        const int deltaX = x - gState.lastMouseX;
        const int deltaY = y - gState.lastMouseY;
        gState.lastMouseX = x;
        gState.lastMouseY = y;

        if (deltaX == 0 && deltaY == 0)
        {
            return;
        }

        if (gState.activeMode == AppMode::EDIT_ORTHO)
        {
            const float aspect = (gWindowHeight == 0) ? 1.0f : static_cast<float>(gWindowWidth) / static_cast<float>(gWindowHeight);
            const float worldPerPixelX = (2.0f * kOrthoSize * aspect) / static_cast<float>(gWindowWidth);
            const float worldPerPixelZ = (2.0f * kOrthoSize) / static_cast<float>(gWindowHeight);
            MoveSelectedObject(deltaX * worldPerPixelX, deltaY * worldPerPixelZ);
        }
        else
        {
            gViewYawDeg -= deltaX * kMouseLookSensitivity;
            gViewPitchDeg += deltaY * kMouseLookSensitivity;
            gViewPitchDeg = Clamp(gViewPitchDeg, -85.0f, 85.0f);

            if (gState.cameraPreset != CameraPreset::FREE)
            {
                gState.cameraPreset = CameraPreset::FREE;
                UpdateWindowTitle();
            }
        }

        glutPostRedisplay();
    }

    void Idle()
    {
        bool changed = false;
        float deltaX = 0.0f;
        float deltaZ = 0.0f;

        if (gState.activeMode == AppMode::EDIT_ORTHO)
        {
            if (gKeyDown[static_cast<unsigned char>('w')])
            {
                deltaZ -= kEditMoveSpeed;
                changed = true;
            }
            if (gKeyDown[static_cast<unsigned char>('s')])
            {
                deltaZ += kEditMoveSpeed;
                changed = true;
            }
            if (gKeyDown[static_cast<unsigned char>('a')])
            {
                deltaX -= kEditMoveSpeed;
                changed = true;
            }
            if (gKeyDown[static_cast<unsigned char>('d')])
            {
                deltaX += kEditMoveSpeed;
                changed = true;
            }

            if (changed)
            {
                MoveSelectedObject(deltaX, deltaZ);
                glutPostRedisplay();
            }
            return;
        }

        const float yawRad = ToRadians(gViewYawDeg);
        const float forwardX = -std::sin(yawRad);
        const float forwardZ = -std::cos(yawRad);
        const float rightX = std::cos(yawRad);
        const float rightZ = -std::sin(yawRad);

        if (gKeyDown[static_cast<unsigned char>('w')])
        {
            gViewTarget.x += forwardX * kViewMoveSpeed;
            gViewTarget.z += forwardZ * kViewMoveSpeed;
            changed = true;
        }
        if (gKeyDown[static_cast<unsigned char>('s')])
        {
            gViewTarget.x -= forwardX * kViewMoveSpeed;
            gViewTarget.z -= forwardZ * kViewMoveSpeed;
            changed = true;
        }
        if (gKeyDown[static_cast<unsigned char>('a')])
        {
            gViewTarget.x -= rightX * kViewMoveSpeed;
            gViewTarget.z -= rightZ * kViewMoveSpeed;
            changed = true;
        }
        if (gKeyDown[static_cast<unsigned char>('d')])
        {
            gViewTarget.x += rightX * kViewMoveSpeed;
            gViewTarget.z += rightZ * kViewMoveSpeed;
            changed = true;
        }
        if (gKeyDown[static_cast<unsigned char>('q')])
        {
            gViewTarget.y += kViewMoveSpeed;
            changed = true;
        }
        if (gKeyDown[static_cast<unsigned char>('e')])
        {
            gViewTarget.y -= kViewMoveSpeed;
            changed = true;
        }

        if (changed)
        {
            gState.cameraPreset = CameraPreset::FREE;
            UpdateWindowTitle();
            glutPostRedisplay();
        }
    }

    void InitGL()
    {
        InitializeSceneData();

        glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
        ApplyDepthTestMode();
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_LIGHT1);
        glEnable(GL_NORMALIZE);
        ApplyShadingMode();

        glutIgnoreKeyRepeat(1);
        glutSetCursor(GLUT_CURSOR_LEFT_ARROW);
    }
} // namespace

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(gWindowWidth, gWindowHeight);
    glutCreateWindow("GTI BTS");

    InitGL();
    UpdateWindowTitle();

    glutDisplayFunc(Display);
    glutReshapeFunc(Reshape);
    glutKeyboardFunc(KeyboardDown);
    glutKeyboardUpFunc(KeyboardUp);
    glutMouseFunc(MouseButton);
    glutMotionFunc(MouseDrag);
    glutIdleFunc(Idle);

    glutMainLoop();
    return 0;
}