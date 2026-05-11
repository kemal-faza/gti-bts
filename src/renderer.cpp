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

// ---------------------------------------------------------------------------
//  Furniture composite drawing (minimalist modern)
// ---------------------------------------------------------------------------

static void DrawTable()
{
    // Table top: 2.0 x 0.1 x 1.2
    glPushMatrix();
    glTranslatef(0.0f, 0.95f, 0.0f);
    glScalef(2.0f, 0.1f, 1.2f);
    DrawTexturedCube(1.0f);
    glPopMatrix();

    // 4 legs
    const float legH = 0.95f;
    const float legR = 0.05f;
    const float lx = 0.85f;
    const float lz = 0.50f;
    for (int i = 0; i < 4; ++i)
    {
        float x = (i == 0 || i == 3) ? lx : -lx;
        float z = (i < 2) ? -lz : lz;
        glPushMatrix();
        glTranslatef(x, 0.0f, z);
        DrawCylinderPrimitive(legR, legH);
        glPopMatrix();
    }
}

static void DrawSofa()
{
    // Seat: 2.2 x 0.35 x 0.9
    glPushMatrix();
    glTranslatef(0.0f, 0.175f, 0.0f);
    glScalef(2.2f, 0.35f, 0.9f);
    DrawTexturedCube(1.0f);
    glPopMatrix();

    // Backrest: 2.2 x 0.55 x 0.15
    glPushMatrix();
    glTranslatef(0.0f, 0.525f, -0.375f);
    glScalef(2.2f, 0.55f, 0.15f);
    DrawTexturedCube(1.0f);
    glPopMatrix();

    // Armrests
    for (int side = -1; side <= 1; side += 2)
    {
        glPushMatrix();
        glTranslatef(side * 1.025f, 0.275f, 0.0f);
        glScalef(0.15f, 0.45f, 0.9f);
        DrawTexturedCube(1.0f);
        glPopMatrix();
    }

    // Legs
    const float legH = 0.10f;
    const float legR = 0.04f;
    const float lx = 0.95f;
    const float lz = 0.375f;
    for (int i = 0; i < 4; ++i)
    {
        float x = (i == 0 || i == 3) ? lx : -lx;
        float z = (i < 2) ? -lz : lz;
        glPushMatrix();
        glTranslatef(x, 0.0f, z);
        DrawCylinderPrimitive(legR, legH);
        glPopMatrix();
    }
}

static void DrawChair()
{
    // Seat: 1.0 x 0.08 x 1.0
    glPushMatrix();
    glTranslatef(0.0f, 0.50f, 0.0f);
    glScalef(1.0f, 0.08f, 1.0f);
    DrawTexturedCube(1.0f);
    glPopMatrix();

    // Backrest: 1.0 x 0.50 x 0.08
    glPushMatrix();
    glTranslatef(0.0f, 0.75f, -0.46f);
    glScalef(1.0f, 0.50f, 0.08f);
    DrawTexturedCube(1.0f);
    glPopMatrix();

    // Legs
    const float legH = 0.50f;
    const float legR = 0.04f;
    const float l = 0.40f;
    for (int i = 0; i < 4; ++i)
    {
        float x = (i == 0 || i == 3) ? l : -l;
        float z = (i < 2) ? -l : l;
        glPushMatrix();
        glTranslatef(x, 0.0f, z);
        DrawCylinderPrimitive(legR, legH);
        glPopMatrix();
    }
}

static void DrawWardrobe()
{
    // Body: 1.4 x 2.4 x 0.55
    glPushMatrix();
    glTranslatef(0.0f, 1.20f, 0.0f);
    glScalef(1.4f, 2.4f, 0.55f);
    DrawTexturedCube(1.0f);
    glPopMatrix();

    // Doors
    for (int side = -1; side <= 1; side += 2)
    {
        glPushMatrix();
        glTranslatef(side * 0.33f, 1.20f, 0.295f);
        glScalef(0.65f, 2.2f, 0.04f);
        DrawTexturedCube(1.0f);
        glPopMatrix();
    }

    // Handles
    for (int side = -1; side <= 1; side += 2)
    {
        glPushMatrix();
        glTranslatef(side * 0.15f, 1.20f, 0.33f);
        glutSolidSphere(0.04f, 12, 12);
        glPopMatrix();
    }
}

static void DrawShelf()
{
    // Vertical frames
    for (int side = -1; side <= 1; side += 2)
    {
        glPushMatrix();
        glTranslatef(side * 0.65f, 0.70f, 0.0f);
        glScalef(0.05f, 1.4f, 0.05f);
        DrawTexturedCube(1.0f);
        glPopMatrix();
    }

    // Shelves
    const float shelfY[3] = {0.30f, 0.70f, 1.10f};
    for (int i = 0; i < 3; ++i)
    {
        glPushMatrix();
        glTranslatef(0.0f, shelfY[i], 0.0f);
        glScalef(1.3f, 0.04f, 0.35f);
        DrawTexturedCube(1.0f);
        glPopMatrix();
    }
}

static void DrawRoundTable()
{
    // Top
    glPushMatrix();
    glTranslatef(0.0f, 0.96f, 0.0f);
    DrawCylinderPrimitive(0.75f, 0.08f);
    glPopMatrix();

    // Pedestal
    glPushMatrix();
    glTranslatef(0.0f, 0.48f, 0.0f);
    DrawCylinderPrimitive(0.12f, 0.96f);
    glPopMatrix();

    // Base
    glPushMatrix();
    glTranslatef(0.0f, 0.02f, 0.0f);
    DrawCylinderPrimitive(0.40f, 0.04f);
    glPopMatrix();
}

static void DrawLamp()
{
    // Base
    glPushMatrix();
    glTranslatef(0.0f, 0.03f, 0.0f);
    DrawCylinderPrimitive(0.22f, 0.06f);
    glPopMatrix();

    // Stand
    glPushMatrix();
    glTranslatef(0.0f, 0.53f, 0.0f);
    DrawCylinderPrimitive(0.035f, 1.00f);
    glPopMatrix();

    // Shade
    glPushMatrix();
    glTranslatef(0.0f, 1.13f, 0.0f);
    DrawCylinderPrimitive(0.18f, 0.20f);
    glPopMatrix();
}

static void DrawCarpet()
{
    glPushMatrix();
    glTranslatef(0.0f, 0.02f, 0.0f);
    glScalef(3.0f, 0.04f, 2.0f);
    DrawTexturedCube(1.0f);
    glPopMatrix();
}

static void DrawMat()
{
    glPushMatrix();
    glTranslatef(0.0f, 0.01f, 0.0f);
    glScalef(2.0f, 0.02f, 1.2f);
    DrawTexturedCube(1.0f);
    glPopMatrix();
}

static void DrawCeilingFan()
{
    // Motor housing
    glPushMatrix();
    glTranslatef(0.0f, -0.075f, 0.0f);
    DrawCylinderPrimitive(0.12f, 0.15f);
    glPopMatrix();

    // Blades
    for (int i = 0; i < 3; ++i)
    {
        float angle = static_cast<float>(i) * 120.0f;
        glPushMatrix();
        glRotatef(angle, 0.0f, 1.0f, 0.0f);
        glTranslatef(0.5f, -0.02f, 0.0f);
        glScalef(1.0f, 0.02f, 0.12f);
        DrawTexturedCube(1.0f);
        glPopMatrix();
    }
}

static bool s_shadowPass = false;

void DrawObjectGeometry(const SceneObject &obj)
{
    // Bind texture if available (skip during shadow pass)
    GLuint tex = 0;
    if (!s_shadowPass)
        tex = GetTextureForSubType(obj.subType);

    if (tex != 0)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

        // Override material to prevent texture darkening
        GLfloat whiteAmb[] = {0.6f, 0.6f, 0.6f, 1.0f};
        GLfloat whiteDif[] = {1.0f, 1.0f, 1.0f, 1.0f};
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,  whiteAmb);
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,  whiteDif);
    }

    switch (obj.subType)
    {
    case ObjectSubType::MEJA:        DrawTable();       break;
    case ObjectSubType::SOFA:        DrawSofa();        break;
    case ObjectSubType::KURSI:       DrawChair();       break;
    case ObjectSubType::LEMARI:      DrawWardrobe();    break;
    case ObjectSubType::RAK:         DrawShelf();       break;
    case ObjectSubType::MEJA_BUNDAR: DrawRoundTable();  break;
    case ObjectSubType::LAMPU:       DrawLamp();        break;
    case ObjectSubType::KARPET:      DrawCarpet();      break;
    case ObjectSubType::TIKAR:       DrawMat();         break;
    case ObjectSubType::KIPAS:       DrawCeilingFan();  break;
    default:
        switch (obj.type)
        {
        case ObjectType::CUBE:     DrawTexturedCube(2.0f); break;
        case ObjectType::CYLINDER: DrawCylinderPrimitive(0.7f, 2.2f); break;
        case ObjectType::ROAD:
            glScalef(6.0f, 0.24f, 3.0f);
            DrawTexturedCube(1.0f);
            break;
        }
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
//  Room visuals (walls + floor)
// ---------------------------------------------------------------------------

void DrawRoom()
{
    const GLboolean wasLighting = glIsEnabled(GL_LIGHTING);
    const GLboolean wasTexture  = glIsEnabled(GL_TEXTURE_2D);
    const GLboolean wasBlend    = glIsEnabled(GL_BLEND);

    glDisable(GL_LIGHTING);
    glDisable(GL_BLEND);

    // ── Floor ──
    const float texRepeat = kRoomSize * 0.5f;  // repeat ~ setiap 2 unit
    GLuint floorTex = GetFloorTexture();

    if (floorTex != 0)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, floorTex);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glColor3f(1.0f, 1.0f, 1.0f);
    }
    else
    {
        glColor3f(0.30f, 0.32f, 0.35f);
    }

    glBegin(GL_QUADS);
    if (floorTex != 0)
        glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-kRoomSize, 0.0f, -kRoomSize);

    if (floorTex != 0)
        glTexCoord2f(texRepeat, 0.0f);
    glVertex3f( kRoomSize, 0.0f, -kRoomSize);

    if (floorTex != 0)
        glTexCoord2f(texRepeat, texRepeat);
    glVertex3f( kRoomSize, 0.0f,  kRoomSize);

    if (floorTex != 0)
        glTexCoord2f(0.0f, texRepeat);
    glVertex3f(-kRoomSize, 0.0f,  kRoomSize);
    glEnd();

    if (floorTex != 0)
        glDisable(GL_TEXTURE_2D);

    // ── Textured walls ──
    GLuint wallTex = GetWallTexture();
    if (wallTex != 0)
    {
        glEnable(GL_LIGHTING);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, wallTex);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

        // Material putih agar texture terlihat natural
        GLfloat white[] = {1.0f, 1.0f, 1.0f, 1.0f};
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,  white);
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,  white);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, white);
        glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, 0.0f);

        const float h = 3.0f;
        glBegin(GL_QUADS);
        // Back (z = -kRoomSize)
        glTexCoord2f(0.0f, 0.0f);      glVertex3f(-kRoomSize, 0.0f, -kRoomSize);
        glTexCoord2f(kRoomSize, 0.0f); glVertex3f( kRoomSize, 0.0f, -kRoomSize);
        glTexCoord2f(kRoomSize, h);    glVertex3f( kRoomSize, h, -kRoomSize);
        glTexCoord2f(0.0f, h);         glVertex3f(-kRoomSize, h, -kRoomSize);
        // Front (z = +kRoomSize)
        glTexCoord2f(0.0f, 0.0f);      glVertex3f(-kRoomSize, 0.0f, kRoomSize);
        glTexCoord2f(kRoomSize, 0.0f); glVertex3f( kRoomSize, 0.0f, kRoomSize);
        glTexCoord2f(kRoomSize, h);    glVertex3f( kRoomSize, h, kRoomSize);
        glTexCoord2f(0.0f, h);         glVertex3f(-kRoomSize, h, kRoomSize);
        // Left (x = -kRoomSize)
        glTexCoord2f(0.0f, 0.0f);      glVertex3f(-kRoomSize, 0.0f, -kRoomSize);
        glTexCoord2f(kRoomSize, 0.0f); glVertex3f(-kRoomSize, 0.0f, kRoomSize);
        glTexCoord2f(kRoomSize, h);    glVertex3f(-kRoomSize, h, kRoomSize);
        glTexCoord2f(0.0f, h);         glVertex3f(-kRoomSize, h, -kRoomSize);
        // Right (x = +kRoomSize)
        glTexCoord2f(0.0f, 0.0f);      glVertex3f( kRoomSize, 0.0f, -kRoomSize);
        glTexCoord2f(kRoomSize, 0.0f); glVertex3f( kRoomSize, 0.0f, kRoomSize);
        glTexCoord2f(kRoomSize, h);    glVertex3f( kRoomSize, h, kRoomSize);
        glTexCoord2f(0.0f, h);         glVertex3f( kRoomSize, h, -kRoomSize);
        glEnd();

        glDisable(GL_TEXTURE_2D);
    }
    else
    {
        // Fallback: wireframe outline
        glDisable(GL_LIGHTING);
        const float h = 3.0f;
        glColor3f(0.50f, 0.55f, 0.65f);
        glBegin(GL_LINE_LOOP);
        glVertex3f(-kRoomSize, 0.0f, -kRoomSize);
        glVertex3f( kRoomSize, 0.0f, -kRoomSize);
        glVertex3f( kRoomSize, h, -kRoomSize);
        glVertex3f(-kRoomSize, h, -kRoomSize);
        glEnd();
        glBegin(GL_LINE_LOOP);
        glVertex3f(-kRoomSize, 0.0f, kRoomSize);
        glVertex3f( kRoomSize, 0.0f, kRoomSize);
        glVertex3f( kRoomSize, h, kRoomSize);
        glVertex3f(-kRoomSize, h, kRoomSize);
        glEnd();
        glBegin(GL_LINE_LOOP);
        glVertex3f(-kRoomSize, 0.0f, -kRoomSize);
        glVertex3f(-kRoomSize, 0.0f, kRoomSize);
        glVertex3f(-kRoomSize, h, kRoomSize);
        glVertex3f(-kRoomSize, h, -kRoomSize);
        glEnd();
        glBegin(GL_LINE_LOOP);
        glVertex3f(kRoomSize, 0.0f, -kRoomSize);
        glVertex3f(kRoomSize, 0.0f, kRoomSize);
        glVertex3f(kRoomSize, h, kRoomSize);
        glVertex3f(kRoomSize, h, -kRoomSize);
        glEnd();
    }

    // Reset lighting ke state awal (sebelum wall) untuk restore yang konsisten
    glDisable(GL_LIGHTING);

    // Restore state
    if (wasTexture)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);

    if (wasBlend)
        glEnable(GL_BLEND);

    if (wasLighting)
        glEnable(GL_LIGHTING);
}
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
//  Shadow rendering (projective shadow + stencil)
// ---------------------------------------------------------------------------

static void ApplyShadowMatrix(const float lx, const float ly, const float lz)
{
    // Directional light projection onto plane y=0
    const float il = 1.0f / ly;
    GLfloat shadowMat[16] = {
        1.0f,    0.0f,    0.0f,    0.0f,
       -lx*il,   0.0f,   -lz*il,   0.0f,
        0.0f,    0.0f,    1.0f,    0.0f,
        0.0f,    0.0f,    0.0f,    1.0f
    };
    glMultMatrixf(shadowMat);
}

void RenderShadows()
{
    // Skip shadows in edit mode — mengganggu visibilitas
    if (gState.activeMode == AppMode::EDIT_ORTHO) return;

    // Arah directional light (LIGHT0) — dinormalisasi
    float lx = -0.45f;
    float ly =  1.0f;
    float lz =  0.25f;
    const float len = std::sqrt(lx * lx + ly * ly + lz * lz);
    if (len > 0.0001f) { lx /= len; ly /= len; lz /= len; }

    const GLboolean wasLighting  = glIsEnabled(GL_LIGHTING);
    const GLboolean wasBlend     = glIsEnabled(GL_BLEND);
    const GLboolean wasDepthTest = glIsEnabled(GL_DEPTH_TEST);

    glEnable(GL_STENCIL_TEST);

    // ── Pass 1: render lantai ke stencil buffer ──
    glClear(GL_STENCIL_BUFFER_BIT);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);   // lantai flat — tidak perlu depth
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);

    glBegin(GL_QUADS);
    glVertex3f(-kRoomSize, 0.0f, -kRoomSize);
    glVertex3f( kRoomSize, 0.0f, -kRoomSize);
    glVertex3f( kRoomSize, 0.0f,  kRoomSize);
    glVertex3f(-kRoomSize, 0.0f,  kRoomSize);
    glEnd();

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);

    // ── Pass 2: gambar shadow di area stencil ──
    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);    // shadow di y=0 sama dengan lantai
    glDepthMask(GL_FALSE);     // jangan tulis depth shadow

    glColor4f(0.0f, 0.0f, 0.0f, 0.10f);

    s_shadowPass = true;

    for (const auto &obj : gSceneObjects)
    {
        glPushMatrix();
        glTranslatef(obj.position.x, obj.position.y, obj.position.z);
        glRotatef(obj.rotationY, 0.0f, 1.0f, 0.0f);
        ApplyShadowMatrix(lx, ly, lz);
        // GL_LEQUAL + depthMask(GL_FALSE) = no z-fighting, no bias needed
        DrawObjectGeometry(obj);
        glPopMatrix();
    }

    s_shadowPass = false;

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);       // restore default

    if (!wasDepthTest) glDisable(GL_DEPTH_TEST);
    if (!wasBlend) glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);

    if (wasLighting) glEnable(GL_LIGHTING);
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

    // ── Background semi-transparan untuk readability ──
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.55f);
    glBegin(GL_QUADS);
    glVertex2i(0, 0);
    glVertex2i(350, 0);
    glVertex2i(350, 500);
    glVertex2i(0, 500);
    glEnd();
    glDisable(GL_BLEND);

    // Auto-scale font based on window height
    const float fontScale = static_cast<float>(gWindowHeight) / 480.0f;
    glPixelZoom(fontScale, fontScale);

    const LevelData &level = GetCurrentLevel();
    char buf[128];

    SetColor(1.0f, 1.0f, 1.0f);
    std::snprintf(buf, sizeof(buf), "Level %d: %s", level.levelNumber, level.roomName);
    RenderString(10, 20, GLUT_BITMAP_TIMES_ROMAN_24, buf);

    // Budget
    int spent = 0;
    for (const auto &obj : gSceneObjects)
        spent += obj.cost;

    if (spent <= level.budget)
        SetColor(0.2f, 1.0f, 0.2f);
    else
        SetColor(1.0f, 0.2f, 0.2f);

    std::snprintf(buf, sizeof(buf), "Budget: %d / %d", spent, level.budget);
    RenderString(10, 40, GLUT_BITMAP_TIMES_ROMAN_24, buf);

    // Checklist — item wajib
    SetColor(0.8f, 0.8f, 1.0f);
    int lineY = 80;
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

        char check = (count >= req.count) ? 'v' : ' ';
        const char *label = GetSubTypeLabel(req.subType);
        std::snprintf(buf, sizeof(buf), "[%c] %dx %s", check, req.count, label);
        SetColor((count >= req.count) ? 0.2f : 1.0f, (count >= req.count) ? 1.0f : 0.7f, 0.2f);
        RenderString(10, static_cast<float>(lineY), GLUT_BITMAP_HELVETICA_18, buf);
        lineY += 24;
    }

    // ── Furniture Selection ──
    lineY += 8;
    SetColor(0.55f, 0.55f, 0.55f);
    RenderString(10, static_cast<float>(lineY), GLUT_BITMAP_HELVETICA_18,
                 "--- Furniture (1-9) ---");
    lineY += 24;

    const char *furnitureNames[] = {
        "1. Meja", "2. Sofa", "3. Kursi",
        "4. Meja Bundar", "5. Lemari", "6. Rak",
        "7. Karpet", "8. Lampu", "9. Tikar"
    };
    for (int i = 0; i < 9; ++i)
    {
        if (i == gState.selectedFurnitureType)
            SetColor(1.0f, 1.0f, 1.0f);
        else
            SetColor(0.65f, 0.65f, 0.65f);

        RenderString(10, static_cast<float>(lineY), GLUT_BITMAP_HELVETICA_18,
                     furnitureNames[i]);
        lineY += 20;
    }

    // Petunjuk
    SetColor(0.6f, 0.6f, 0.6f);
    RenderString(10, static_cast<float>(gWindowHeight) - 32, GLUT_BITMAP_HELVETICA_18,
                 "Enter = Submit | Tab = Edit/View | Esc = Menu");

    glPixelZoom(1.0f, 1.0f);
    PopOverlayOrtho();
}

void RenderOverlay()
{
    PushOverlayOrtho();

    // Semi-transparent dark overlay
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.70f);
    glBegin(GL_QUADS);
    glVertex2i(0, 0);
    glVertex2i(gWindowWidth, 0);
    glVertex2i(gWindowWidth, gWindowHeight);
    glVertex2i(0, gWindowHeight);
    glEnd();
    glDisable(GL_BLEND);

    // Auto-scale font based on window height
    const float fontScale = static_cast<float>(gWindowHeight) / 480.0f;
    glPixelZoom(fontScale, fontScale);

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
                              GLUT_BITMAP_HELVETICA_18, level.clientBrief);

        SetColor(0.2f, 1.0f, 0.2f);
        RenderString(static_cast<float>(cx) - 100, static_cast<float>(cy) + 60,
                     GLUT_BITMAP_TIMES_ROMAN_24, "[ Enter ] - Mulai");

        SetColor(0.6f, 0.6f, 0.6f);
        RenderString(static_cast<float>(cx) - 80, static_cast<float>(cy) + 85,
                     GLUT_BITMAP_HELVETICA_18, "Esc = Keluar");
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
                     GLUT_BITMAP_HELVETICA_18, buf);

        if (gState.currentLevel + 1 < static_cast<int>(gLevels.size()))
        {
            SetColor(0.2f, 1.0f, 0.2f);
            RenderString(static_cast<float>(cx) - 100, static_cast<float>(cy) + 50,
                         GLUT_BITMAP_TIMES_ROMAN_24, "[ Enter ] - Lanjut Level");
        }
        else
        {
            SetColor(1.0f, 0.8f, 0.2f);
            RenderString(static_cast<float>(cx) - 120, static_cast<float>(cy) + 50,
                         GLUT_BITMAP_TIMES_ROMAN_24, "Semua Level Selesai!");
        }
        SetColor(0.6f, 0.6f, 0.6f);
        RenderString(static_cast<float>(cx) - 70, static_cast<float>(cy) + 75,
                     GLUT_BITMAP_HELVETICA_18, "Esc = Menu Utama");
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
                         GLUT_BITMAP_HELVETICA_18, gState.failReason);
        }
        else
        {
            std::snprintf(buf, sizeof(buf), "Budget: %d / %d (melebihi!)",
                          gState.totalSpent, GetCurrentLevel().budget);
            RenderString(static_cast<float>(cx) - 100, static_cast<float>(cy) + 20,
                         GLUT_BITMAP_HELVETICA_18, buf);
        }

        SetColor(0.2f, 1.0f, 0.2f);
        RenderString(static_cast<float>(cx) - 80, static_cast<float>(cy) + 50,
                     GLUT_BITMAP_TIMES_ROMAN_24, "[ Enter ] - Coba Lagi");

        SetColor(0.6f, 0.6f, 0.6f);
        RenderString(static_cast<float>(cx) - 70, static_cast<float>(cy) + 75,
                      GLUT_BITMAP_HELVETICA_18, "Esc = Menu Utama");
    }

    glPixelZoom(1.0f, 1.0f);
    PopOverlayOrtho();
}

// ---------------------------------------------------------------------------
//  Display entry point
// ---------------------------------------------------------------------------

void Display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Always render scene as background (even in menu, shows last view)
    ConfigureProjectionMatrix();
    ApplyCameraViewMatrix();
    SetupLights();

    DrawRoom();
    RenderShadows();
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
