#include "scene.h"
#include "renderer.h"
#include "texture.h"
#include "ui.h"

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>

// =========================================================================
//  GLUT Callbacks
// =========================================================================

// -------------------------------------------------------------------------
//  Reshape
// -------------------------------------------------------------------------

void Reshape(int width, int height)
{
    if (width <= 0 || height <= 0) return;

    gWindowWidth  = width;
    gWindowHeight = height;
    glViewport(0, 0, width, height);
    glutPostRedisplay();
}

// -------------------------------------------------------------------------
//  Keyboard — dispatch handlers
// -------------------------------------------------------------------------

static void StartLevel(int index)
{
    gState.currentLevel = index;
    gState.gameState = GameState::PLAYING;
    gState.activeMode = AppMode::EDIT_ORTHO;
    gSceneObjects.clear();
    glutSetCursor(GLUT_CURSOR_LEFT_ARROW);
    gState.titleDirty = true;
    glutPostRedisplay();
}

static bool HandleGlobalKeys(unsigned char key)
{
    if (key == 27)                  // Esc
    {
        if (gState.gameState == GameState::MENU)
        {
            std::exit(0);
        }
        // From PLAYING, WIN, LOSE — go back to menu
        gState.gameState = GameState::MENU;
        gState.titleDirty = true;
        glutPostRedisplay();
        return true;
    }

    if (key == '\t')                // Tab — toggle mode
    {
        if (gState.gameState != GameState::PLAYING) return true;

        if (gState.activeMode == AppMode::EDIT_ORTHO)
            gState.activeMode = AppMode::VIEW_PERSPECTIVE;
        else
            gState.activeMode = AppMode::EDIT_ORTHO;

        gState.isDragging = false;
        glutSetCursor(GLUT_CURSOR_LEFT_ARROW);
        gState.titleDirty = true;
        glutPostRedisplay();
        return true;
    }

    if (key == '\r' || key == '\n') // Enter
    {
        if (gState.gameState == GameState::MENU)
        {
            StartLevel(0);
            return true;
        }
        if (gState.gameState == GameState::PLAYING)
        {
            GameState result = EvaluateSubmission();
            gState.gameState = result;
            gState.titleDirty = true;
            glutPostRedisplay();
            return true;
        }
        if (gState.gameState == GameState::WIN)
        {
            // Next level (if not last)
            int next = gState.currentLevel + 1;
            if (next < static_cast<int>(gLevels.size()))
                StartLevel(next);
            else
                gState.gameState = GameState::MENU;
            return true;
        }
        if (gState.gameState == GameState::LOSE)
        {
            // Retry — restart current level
            StartLevel(gState.currentLevel);
            return true;
        }
    }

    return false;
}

static void HandleEditKeys(unsigned char key)
{
    if (key == ' ')
    {
        AddNewObjectInEditMode();
        gState.titleDirty = true;
        glutPostRedisplay();
        return;
    }

    if (key == '[')
    {
        SelectAdjacentObject(-1);
        gState.titleDirty = true;
        glutPostRedisplay();
        return;
    }

    if (key == ']')
    {
        SelectAdjacentObject(1);
        gState.titleDirty = true;
        glutPostRedisplay();
        return;
    }

    if (key == 'w' || key == 'a' || key == 's' || key == 'd')
        gKeyDown[key] = true;
}

static void HandleViewKeys(unsigned char key)
{
    if (key == '1')
    {
        SetPerspectivePreset(CameraPreset::ONE_POINT);
        gState.titleDirty = true;
        glutPostRedisplay();
        return;
    }
    if (key == '2')
    {
        SetPerspectivePreset(CameraPreset::TWO_POINT);
        gState.titleDirty = true;
        glutPostRedisplay();
        return;
    }
    if (key == '3')
    {
        SetPerspectivePreset(CameraPreset::THREE_POINT);
        gState.titleDirty = true;
        glutPostRedisplay();
        return;
    }

    if (key == 'l')
    {
        gState.directionalLightEnabled = !gState.directionalLightEnabled;
        gState.titleDirty = true;
        glutPostRedisplay();
        return;
    }
    if (key == 'f')
    {
        gState.smoothShading = !gState.smoothShading;
        ApplyShadingMode();
        gState.titleDirty = true;
        glutPostRedisplay();
        return;
    }
    if (key == 'z')
    {
        gState.depthTestEnabled = !gState.depthTestEnabled;
        ApplyDepthTestMode();
        gState.titleDirty = true;
        glutPostRedisplay();
        return;
    }

    if (key == 'w' || key == 'a' || key == 's' || key == 'd' ||
        key == 'q' || key == 'e')
    {
        gKeyDown[key] = true;
    }
}

void KeyboardDown(unsigned char key, int, int)
{
    const unsigned char lowered = static_cast<unsigned char>(std::tolower(key));
    if (HandleGlobalKeys(key)) return;

    // Edit/view keys only work during PLAYING
    if (gState.gameState != GameState::PLAYING) return;

    if (gState.activeMode == AppMode::EDIT_ORTHO)
        HandleEditKeys(lowered);
    else
        HandleViewKeys(lowered);
}

void KeyboardUp(unsigned char key, int, int)
{
    const unsigned char lowered = static_cast<unsigned char>(std::tolower(key));
    gKeyDown[lowered] = false;
}

// -------------------------------------------------------------------------
//  Mouse
// -------------------------------------------------------------------------

void MouseButton(int button, int state, int x, int y)
{
    if (button != GLUT_LEFT_BUTTON) return;

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
    if (!gState.isDragging) return;

    const int deltaX = x - gState.lastMouseX;
    const int deltaY = y - gState.lastMouseY;
    gState.lastMouseX = x;
    gState.lastMouseY = y;

    if (deltaX == 0 && deltaY == 0) return;

    if (gState.activeMode == AppMode::EDIT_ORTHO)
    {
        const float aspect = (gWindowHeight == 0)
                                 ? 1.0f
                                 : static_cast<float>(gWindowWidth) / static_cast<float>(gWindowHeight);
        const float worldPerPixelX = (2.0f * kOrthoSize * aspect) / static_cast<float>(gWindowWidth);
        const float worldPerPixelZ = (2.0f * kOrthoSize) / static_cast<float>(gWindowHeight);
        MoveSelectedObject(deltaX * worldPerPixelX, deltaY * worldPerPixelZ);
    }
    else
    {
        gViewYawDeg   -= static_cast<float>(deltaX) * kMouseLookSensitivity;
        gViewPitchDeg += static_cast<float>(deltaY) * kMouseLookSensitivity;
        gViewPitchDeg  = Clamp(gViewPitchDeg, -85.0f, 85.0f);

        if (gState.cameraPreset != CameraPreset::FREE)
        {
            gState.cameraPreset = CameraPreset::FREE;
            gState.titleDirty = true;
        }
    }

    glutPostRedisplay();
}

// -------------------------------------------------------------------------
//  Idle (continuous key-driven movement)
// -------------------------------------------------------------------------

void Idle()
{
    // Dirty title flag — rebuild window title once per frame if needed
    if (gState.titleDirty)
    {
        UpdateWindowTitle();
        gState.titleDirty = false;
    }

    // Only process movement during PLAYING state
    if (gState.gameState != GameState::PLAYING) return;

    bool changed = false;
    float deltaX = 0.0f;
    float deltaZ = 0.0f;

    if (gState.activeMode == AppMode::EDIT_ORTHO)
    {
        if (gKeyDown[static_cast<unsigned char>('w')]) { deltaZ -= kEditMoveSpeed; changed = true; }
        if (gKeyDown[static_cast<unsigned char>('s')]) { deltaZ += kEditMoveSpeed; changed = true; }
        if (gKeyDown[static_cast<unsigned char>('a')]) { deltaX -= kEditMoveSpeed; changed = true; }
        if (gKeyDown[static_cast<unsigned char>('d')]) { deltaX += kEditMoveSpeed; changed = true; }

        if (changed)
        {
            MoveSelectedObject(deltaX, deltaZ);
            glutPostRedisplay();
        }
        return;
    }

    // ---- VIEW mode camera movement ----
    const float yawRad   = ToRadians(gViewYawDeg);
    const float forwardX = -std::sin(yawRad);
    const float forwardZ = -std::cos(yawRad);
    const float rightX   =  std::cos(yawRad);
    const float rightZ   = -std::sin(yawRad);

    if (gKeyDown[static_cast<unsigned char>('w')]) { gViewTarget.x += forwardX * kViewMoveSpeed; gViewTarget.z += forwardZ * kViewMoveSpeed; changed = true; }
    if (gKeyDown[static_cast<unsigned char>('s')]) { gViewTarget.x -= forwardX * kViewMoveSpeed; gViewTarget.z -= forwardZ * kViewMoveSpeed; changed = true; }
    if (gKeyDown[static_cast<unsigned char>('a')]) { gViewTarget.x -= rightX   * kViewMoveSpeed; gViewTarget.z -= rightZ   * kViewMoveSpeed; changed = true; }
    if (gKeyDown[static_cast<unsigned char>('d')]) { gViewTarget.x += rightX   * kViewMoveSpeed; gViewTarget.z += rightZ   * kViewMoveSpeed; changed = true; }
    if (gKeyDown[static_cast<unsigned char>('q')]) { gViewTarget.y += kViewMoveSpeed; changed = true; }
    if (gKeyDown[static_cast<unsigned char>('e')]) { gViewTarget.y -= kViewMoveSpeed; changed = true; }

    if (changed)
    {
        gState.cameraPreset = CameraPreset::FREE;
        gState.titleDirty = true;
        glutPostRedisplay();
    }

}

// =========================================================================
//  Initialisation
// =========================================================================

void InitGL()
{
    InitializeLevels();
    gState.gameState = GameState::MENU;
    gState.currentLevel = 0;

    InitTextures();

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

// =========================================================================
//  Entry point
// =========================================================================

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(gWindowWidth, gWindowHeight);
    glutCreateWindow("GTI BTS");

    InitGL();
    UpdateWindowTitle();
    gState.titleDirty = false;

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
