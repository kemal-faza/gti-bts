#include "renderer.h"

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <cmath>

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
//  Display entry point
// ---------------------------------------------------------------------------

void Display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ConfigureProjectionMatrix();
    ApplyCameraViewMatrix();
    SetupLights();

    DrawGrid();
    DrawSceneObjects();
    DrawPointLightMarker();

    glutSwapBuffers();
}
