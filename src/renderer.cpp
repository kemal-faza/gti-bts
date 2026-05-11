#include "renderer.h"
#include "texture.h"
#include "ui.h"

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <cmath>
#include <cstdio>

// ---------------------------------------------------------------------------
//  Shading / depth helpers
// ---------------------------------------------------------------------------

void ApplyShadingMode()
{
    glShadeModel(gState.smoothShading ? GL_SMOOTH : GL_FLAT);
}

void ApplyDepthTestMode()
{
    if (gState.depthTestEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
}

// ---------------------------------------------------------------------------
//  Projection & view
// ---------------------------------------------------------------------------

void ConfigureProjectionMatrix()
{
    const float aspect = (gWindowHeight == 0)
                             ? 1.0f
                             : static_cast<float>(gWindowWidth) / static_cast<float>(gWindowHeight);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    if (gState.activeMode == AppMode::EDIT_ORTHO)
        glOrtho(-kOrthoSize * aspect, kOrthoSize * aspect, -kOrthoSize, kOrthoSize, -100.0, 100.0);
    else
        gluPerspective(60.0, aspect, 0.1, 200.0);
}

void ApplyCameraViewMatrix()
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (gState.activeMode == AppMode::EDIT_ORTHO)
    {
        gluLookAt(0.0, 24.0, 0.0,
                  0.0,  0.0, 0.0,
                  0.0,  0.0, -1.0);
        return;
    }

    const float yawRad   = ToRadians(gViewYawDeg);
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

// ---------------------------------------------------------------------------
//  Material / Lighting
// ---------------------------------------------------------------------------

void ApplyObjectMaterial(const MaterialType material, const bool selected)
{
    GLfloat ambient[4]   = {0.10f, 0.10f, 0.10f, 1.0f};
    GLfloat diffuse[4]   = {0.50f, 0.50f, 0.50f, 1.0f};
    GLfloat specular[4]  = {0.08f, 0.08f, 0.08f, 1.0f};
    GLfloat emission[4]  = {0.0f,  0.0f,  0.0f,  1.0f};
    float   shininess    = 8.0f;

    if (material == MaterialType::GLOSSY)
    {
        ambient[0]  = 0.20f;  ambient[1]  = 0.07f;  ambient[2]  = 0.03f;
        diffuse[0]  = 0.90f;  diffuse[1]  = 0.34f;  diffuse[2]  = 0.18f;
        specular[0] = 0.95f;  specular[1] = 0.95f;  specular[2] = 0.95f;
        shininess   = 96.0f;
    }

    if (selected && gState.activeMode == AppMode::EDIT_ORTHO)
    {
        emission[0] = 0.12f;
        emission[1] = 0.12f;
        emission[2] = 0.22f;
    }

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,   ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,   diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR,  specular);
    glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, shininess);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION,  emission);
}

void SetupLights()
{
    glEnable(GL_LIGHTING);

    if (gState.directionalLightEnabled)
        glEnable(GL_LIGHT0);
    else
        glDisable(GL_LIGHT0);
    glEnable(GL_LIGHT1);

    const GLfloat globalAmbient[] = {0.08f, 0.08f, 0.09f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

    const GLfloat dirAmbient[]   = {0.08f, 0.08f, 0.07f, 1.0f};
    const GLfloat dirDiffuse[]   = {0.75f, 0.72f, 0.66f, 1.0f};
    const GLfloat dirSpecular[]  = {0.40f, 0.38f, 0.34f, 1.0f};
    const GLfloat dirPosition[]  = {-0.45f, 1.0f, 0.25f, 0.0f};
    glLightfv(GL_LIGHT0, GL_AMBIENT,  dirAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  dirDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, dirSpecular);
    glLightfv(GL_LIGHT0, GL_POSITION, dirPosition);

    const GLfloat pointAmbient[]   = {0.05f, 0.04f, 0.03f, 1.0f};
    const GLfloat pointDiffuse[]   = {1.0f,  0.90f, 0.70f, 1.0f};
    const GLfloat pointSpecular[]  = {1.0f,  0.95f, 0.80f, 1.0f};
    const GLfloat pointPosition[]  = {gPointLightPos.x, gPointLightPos.y, gPointLightPos.z, 1.0f};
    glLightfv(GL_LIGHT1, GL_AMBIENT,  pointAmbient);
    glLightfv(GL_LIGHT1, GL_DIFFUSE,  pointDiffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, pointSpecular);
    glLightfv(GL_LIGHT1, GL_POSITION, pointPosition);
    glLightf (GL_LIGHT1, GL_CONSTANT_ATTENUATION,  1.0f);
    glLightf (GL_LIGHT1, GL_LINEAR_ATTENUATION,    0.03f);
    glLightf (GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.01f);
}

// ---------------------------------------------------------------------------
//  Primitives
// ---------------------------------------------------------------------------

void DrawCylinderPrimitive(const float radius, const float height)
{
    GLUquadric *quad = gluNewQuadric();
    if (quad == nullptr) return;

    gluQuadricNormals(quad, GLU_SMOOTH);
    gluQuadricTexture(quad, GL_TRUE);

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

// ---------------------------------------------------------------------------
//  Textured primitive helpers
// ---------------------------------------------------------------------------

static void DrawTexturedCube(const float size)
{
    const float h = size * 0.5f;
    glBegin(GL_QUADS);

    // Front
    glTexCoord2f(0, 0); glVertex3f(-h, -h,  h);
    glTexCoord2f(1, 0); glVertex3f( h, -h,  h);
    glTexCoord2f(1, 1); glVertex3f( h,  h,  h);
    glTexCoord2f(0, 1); glVertex3f(-h,  h,  h);

    // Back
    glTexCoord2f(0, 0); glVertex3f( h, -h, -h);
    glTexCoord2f(1, 0); glVertex3f(-h, -h, -h);
    glTexCoord2f(1, 1); glVertex3f(-h,  h, -h);
    glTexCoord2f(0, 1); glVertex3f( h,  h, -h);

    // Top
    glTexCoord2f(0, 0); glVertex3f(-h,  h,  h);
    glTexCoord2f(1, 0); glVertex3f( h,  h,  h);
    glTexCoord2f(1, 1); glVertex3f( h,  h, -h);
    glTexCoord2f(0, 1); glVertex3f(-h,  h, -h);

    // Bottom
    glTexCoord2f(0, 0); glVertex3f(-h, -h, -h);
    glTexCoord2f(1, 0); glVertex3f( h, -h, -h);
    glTexCoord2f(1, 1); glVertex3f( h, -h,  h);
    glTexCoord2f(0, 1); glVertex3f(-h, -h,  h);

    // Right
    glTexCoord2f(0, 0); glVertex3f( h, -h,  h);
    glTexCoord2f(1, 0); glVertex3f( h, -h, -h);
    glTexCoord2f(1, 1); glVertex3f( h,  h, -h);
    glTexCoord2f(0, 1); glVertex3f( h,  h,  h);

    // Left
    glTexCoord2f(0, 0); glVertex3f(-h, -h, -h);
    glTexCoord2f(1, 0); glVertex3f(-h, -h,  h);
    glTexCoord2f(1, 1); glVertex3f(-h,  h,  h);
    glTexCoord2f(0, 1); glVertex3f(-h,  h, -h);

    glEnd();
}

void DrawObjectGeometry(const SceneObject &obj)
{
    // Bind texture if available
    GLuint tex = GetTextureForSubType(obj.subType);
    if (tex != 0)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }

    switch (obj.type)
    {
    case ObjectType::CUBE:
        DrawTexturedCube(2.0f);
        break;

    case ObjectType::CYLINDER:
        DrawCylinderPrimitive(0.7f, 2.2f);
        break;

    case ObjectType::ROAD:
        glScalef(6.0f, 0.24f, 3.0f);
        DrawTexturedCube(1.0f);
        break;
    }

    if (tex != 0)
        glDisable(GL_TEXTURE_2D);
}

// ---------------------------------------------------------------------------
//  Grid
// ---------------------------------------------------------------------------

void DrawGrid()
{
    const GLboolean lightingEnabled = glIsEnabled(GL_LIGHTING);
    if (lightingEnabled) glDisable(GL_LIGHTING);

    glColor3f(0.35f, 0.35f, 0.35f);
    glBegin(GL_LINES);
    for (int i = -kGridHalfSize; i <= kGridHalfSize; ++i)
    {
        const float k = static_cast<float>(i) * kGridStep;
        glVertex3f( k, kGridY, -kGridHalfSize * kGridStep);
        glVertex3f( k, kGridY,  kGridHalfSize * kGridStep);
        glVertex3f(-kGridHalfSize * kGridStep, kGridY,  k);
        glVertex3f( kGridHalfSize * kGridStep, kGridY,  k);
    }
    glEnd();

    if (lightingEnabled) glEnable(GL_LIGHTING);
}

// ---------------------------------------------------------------------------
//  Scene rendering
// ---------------------------------------------------------------------------

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

    const GLfloat ambient[]   = {0.20f, 0.16f, 0.08f, 1.0f};
    const GLfloat diffuse[]   = {0.92f, 0.82f, 0.45f, 1.0f};
    const GLfloat specular[]  = {0.0f,  0.0f,  0.0f,  1.0f};
    const GLfloat emission[]  = {0.95f, 0.80f, 0.35f, 1.0f};
    const GLfloat noEmission[] = {0.0f, 0.0f, 0.0f, 1.0f};

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,   ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,   diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR,  specular);
    glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, 4.0f);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION,  emission);

    glutSolidSphere(0.35, 18, 18);

    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, noEmission);
    glPopMatrix();
}

// ---------------------------------------------------------------------------
//  UI overlay helpers
// ---------------------------------------------------------------------------

static GLboolean s_savedDepthTest  = GL_TRUE;
static GLboolean s_savedLighting   = GL_TRUE;

static void PushOverlayOrtho()
{
    // Save current state before drawing overlay
    s_savedDepthTest = glIsEnabled(GL_DEPTH_TEST);
    s_savedLighting  = glIsEnabled(GL_LIGHTING);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, static_cast<double>(gWindowWidth),
            static_cast<double>(gWindowHeight), 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
}

static void PopOverlayOrtho()
{
    // Restore matrix stack
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    // Restore saved state (not force-enable)
    if (s_savedDepthTest)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (s_savedLighting)
        glEnable(GL_LIGHTING);
    else
        glDisable(GL_LIGHTING);
}

void RenderHUD()
{
    PushOverlayOrtho();

    const LevelData &level = GetCurrentLevel();
    char buf[128];

    SetColor(1.0f, 1.0f, 1.0f);
    std::snprintf(buf, sizeof(buf), "Level %d: %s", level.levelNumber, level.roomName);
    RenderString(10, 20, GLUT_BITMAP_HELVETICA_18, buf);

    // Budget
    int spent = 0;
    for (const auto &obj : gSceneObjects)
        spent += obj.cost;

    if (spent <= level.budget)
        SetColor(0.2f, 1.0f, 0.2f);
    else
        SetColor(1.0f, 0.2f, 0.2f);

    std::snprintf(buf, sizeof(buf), "Budget: %d / %d", spent, level.budget);
    RenderString(10, 40, GLUT_BITMAP_HELVETICA_18, buf);

    // Checklist — item wajib
    SetColor(0.8f, 0.8f, 1.0f);
    int lineY = 64;
    for (const auto &req : level.requiredItems)
    {
        int count = 0;
        for (const auto &obj : gSceneObjects)
            if (obj.type == req.type) ++count;

        char check = (count >= req.count) ? 'v' : ' ';
        const char *label = nullptr;
        for (const auto &so : gSceneObjects)
        {
            if (so.type == req.type)
            {
                label = GetSubTypeLabel(so.subType);
                break;
            }
        }
        if (label == nullptr)
        {
            switch (req.type)
            {
            case ObjectType::CUBE:     label = "Kotak";    break;
            case ObjectType::CYLINDER: label = "Bulat";    break;
            case ObjectType::ROAD:     label = "Datar";    break;
            default:                   label = "Objek";    break;
            }
        }
        std::snprintf(buf, sizeof(buf), "[%c] %dx %s", check, req.count, label);
        SetColor((count >= req.count) ? 0.2f : 1.0f, (count >= req.count) ? 1.0f : 0.7f, 0.2f);
        RenderString(10, static_cast<float>(lineY), GLUT_BITMAP_HELVETICA_12, buf);
        lineY += 16;
    }

    // Petunjuk
    SetColor(0.6f, 0.6f, 0.6f);
    RenderString(10, static_cast<float>(gWindowHeight) - 32, GLUT_BITMAP_HELVETICA_12,
                 "Enter = Submit | Tab = Edit/View | Esc = Menu");

    PopOverlayOrtho();
}

void RenderOverlay()
{
    PushOverlayOrtho();

    // Semi-transparent dark overlay
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    SetColor(0.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2i(0, 0);
    glVertex2i(gWindowWidth, 0);
    glVertex2i(gWindowWidth, gWindowHeight);
    glVertex2i(0, gWindowHeight);
    glEnd();
    glDisable(GL_BLEND);

    const int cx = gWindowWidth / 2;
    const int cy = gWindowHeight / 2;
    char buf[256];

    if (gState.gameState == GameState::MENU)
    {
        const LevelData &level = GetCurrentLevel();

        SetColor(0.3f, 0.6f, 1.0f);
        std::snprintf(buf, sizeof(buf), "Interior Designer - Room %d", level.levelNumber);
        RenderString(static_cast<float>(cx) - 120, static_cast<float>(cy) - 80,
                     GLUT_BITMAP_TIMES_ROMAN_24, buf);

        SetColor(1.0f, 1.0f, 1.0f);
        RenderStringMultiline(static_cast<float>(cx) - 140, static_cast<float>(cy) - 40,
                              GLUT_BITMAP_HELVETICA_12, level.clientBrief);

        SetColor(0.2f, 1.0f, 0.2f);
        RenderString(static_cast<float>(cx) - 100, static_cast<float>(cy) + 60,
                     GLUT_BITMAP_HELVETICA_18, "[ Enter ] - Mulai");

        SetColor(0.6f, 0.6f, 0.6f);
        RenderString(static_cast<float>(cx) - 80, static_cast<float>(cy) + 85,
                     GLUT_BITMAP_HELVETICA_12, "Esc = Keluar");
    }
    else if (gState.gameState == GameState::WIN)
    {
        SetColor(0.2f, 1.0f, 0.2f);
        std::snprintf(buf, sizeof(buf), " SELESAI! ");
        RenderString(static_cast<float>(cx) - 60, static_cast<float>(cy) - 40,
                     GLUT_BITMAP_TIMES_ROMAN_24, buf);

        SetColor(1.0f, 1.0f, 1.0f);
        std::snprintf(buf, sizeof(buf), "Budget terpakai: %d / %d",
                      gState.totalSpent, GetCurrentLevel().budget);
        RenderString(static_cast<float>(cx) - 100, static_cast<float>(cy) + 20,
                     GLUT_BITMAP_HELVETICA_12, buf);

        if (gState.currentLevel + 1 < static_cast<int>(gLevels.size()))
        {
            SetColor(0.2f, 1.0f, 0.2f);
            RenderString(static_cast<float>(cx) - 100, static_cast<float>(cy) + 50,
                         GLUT_BITMAP_HELVETICA_18, "[ Enter ] - Lanjut Level");
        }
        else
        {
            SetColor(1.0f, 0.8f, 0.2f);
            RenderString(static_cast<float>(cx) - 120, static_cast<float>(cy) + 50,
                         GLUT_BITMAP_HELVETICA_18, "Semua Level Selesai!");
        }
        SetColor(0.6f, 0.6f, 0.6f);
        RenderString(static_cast<float>(cx) - 70, static_cast<float>(cy) + 75,
                     GLUT_BITMAP_HELVETICA_12, "Esc = Menu Utama");
    }
    else if (gState.gameState == GameState::LOSE)
    {
        SetColor(1.0f, 0.2f, 0.2f);
        RenderString(static_cast<float>(cx) - 50, static_cast<float>(cy) - 40,
                     GLUT_BITMAP_TIMES_ROMAN_24, " GAGAL ");

        SetColor(1.0f, 1.0f, 1.0f);
        if (gState.failReason[0] != '\0')
        {
            RenderString(static_cast<float>(cx) - 140, static_cast<float>(cy) + 20,
                         GLUT_BITMAP_HELVETICA_12, gState.failReason);
        }
        else
        {
            std::snprintf(buf, sizeof(buf), "Budget: %d / %d (melebihi!)",
                          gState.totalSpent, GetCurrentLevel().budget);
            RenderString(static_cast<float>(cx) - 100, static_cast<float>(cy) + 20,
                         GLUT_BITMAP_HELVETICA_12, buf);
        }

        SetColor(0.2f, 1.0f, 0.2f);
        RenderString(static_cast<float>(cx) - 80, static_cast<float>(cy) + 50,
                     GLUT_BITMAP_HELVETICA_18, "[ Enter ] - Coba Lagi");

        SetColor(0.6f, 0.6f, 0.6f);
        RenderString(static_cast<float>(cx) - 70, static_cast<float>(cy) + 75,
                     GLUT_BITMAP_HELVETICA_12, "Esc = Menu Utama");
    }

    PopOverlayOrtho();
}

// ---------------------------------------------------------------------------
//  Display entry point
// ---------------------------------------------------------------------------

void Display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Always render scene as background (even in menu, shows last view)
    ConfigureProjectionMatrix();
    ApplyCameraViewMatrix();
    SetupLights();

    DrawGrid();
    DrawSceneObjects();
    DrawPointLightMarker();

    // Overlays and HUD depend on game state
    if (gState.gameState == GameState::MENU ||
        gState.gameState == GameState::WIN ||
        gState.gameState == GameState::LOSE)
    {
        RenderOverlay();
    }
    else if (gState.gameState == GameState::PLAYING)
    {
        RenderHUD();
    }

    glutSwapBuffers();
}
