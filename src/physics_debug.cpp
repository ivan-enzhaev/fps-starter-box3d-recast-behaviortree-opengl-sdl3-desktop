#include "physics_debug.h"
#include "app.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

typedef struct
{
    b3ShapeType type;
    b3Vec3 extents;
    float radius;
    b3Vec3 p1, p2;
} MyDebugShape;

int b3InternalAssert(const char *condition, const char *fileName, int lineNumber)
{
    printf("Box3D Assertion Failed: %s, file %s, line %d\n", condition, fileName, lineNumber);
    fflush(stdout);
    abort();
    return 1;
}

void initLineRenderer(App *app)
{
    glGenVertexArrays(1, &app->lineVao);
    glGenBuffers(1, &app->lineVbo);
    glBindVertexArray(app->lineVao);
    glBindBuffer(GL_ARRAY_BUFFER, app->lineVbo);
    glBufferData(GL_ARRAY_BUFFER, 4096 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glBindVertexArray(0);
}

void drawDebugLines(App *app, const std::vector<float> &lineVertices, const glm::vec4 &color)
{
    if (lineVertices.empty() || !app)
        return;

    glUseProgram(app->lineShaderProgram);
    glUniformMatrix4fv(app->uLineMvpMatrixLocation, 1, GL_FALSE, glm::value_ptr(app->projView3D));
    glUniform4f(app->uLineColorLocation, color.r, color.g, color.b, color.a);

    glBindVertexArray(app->lineVao);
    glBindBuffer(GL_ARRAY_BUFFER, app->lineVbo);
    glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(float), lineVertices.data(), GL_DYNAMIC_DRAW);

    glDrawArrays(GL_LINES, 0, (GLsizei)(lineVertices.size() / 3));
    glBindVertexArray(0);
}

void drawDebugCircle(App *app, const glm::vec3 &center, float radius, const glm::vec4 &color, int segments)
{
    std::vector<float> lines;
    lines.reserve(segments * 6);
    const float twoPi = glm::two_pi<float>();

    for (int i = 0; i < segments; ++i)
    {
        float a1 = ((float)i / segments) * twoPi;
        float a2 = ((float)(i + 1) / segments) * twoPi;

        lines.push_back(center.x + std::cos(a1) * radius);
        lines.push_back(center.y);
        lines.push_back(center.z + std::sin(a1) * radius);

        lines.push_back(center.x + std::cos(a2) * radius);
        lines.push_back(center.y);
        lines.push_back(center.z + std::sin(a2) * radius);
    }

    drawDebugLines(app, lines, color);
}

void *createDebugShape(const b3DebugShape *debugShape, void *context)
{
    MyDebugShape *myShape = (MyDebugShape *)malloc(sizeof(MyDebugShape));
    myShape->type = debugShape->type;

    if (debugShape->type == b3_hullShape)
    {
        b3AABB aabb = debugShape->hull->aabb;
        myShape->extents.x = (aabb.upperBound.x - aabb.lowerBound.x) * 0.5f;
        myShape->extents.y = (aabb.upperBound.y - aabb.lowerBound.y) * 0.5f;
        myShape->extents.z = (aabb.upperBound.z - aabb.lowerBound.z) * 0.5f;
    }
    else if (debugShape->type == b3_sphereShape)
    {
        myShape->radius = debugShape->sphere->radius;
    }
    else if (debugShape->type == b3_capsuleShape)
    {
        myShape->radius = debugShape->capsule->radius;
        myShape->p1 = debugShape->capsule->center1;
        myShape->p2 = debugShape->capsule->center2;
    }

    return (void *)myShape;
}

void destroyDebugShape(void *userShape, void *context)
{
    free(userShape);
}

static void getWireframeBoxLines(const glm::vec3 &extents, std::vector<float> &outLines)
{
    float x = extents.x, y = extents.y, z = extents.z;

    glm::vec3 corners[8] = {
        { -x, -y, -z }, { x, -y, -z }, { x, y, -z }, { -x, y, -z },
        { -x, -y, z }, { x, -y, z }, { x, y, z }, { -x, y, z }
    };

    int indices[24] = {
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7
    };

    outLines.reserve(outLines.size() + 24 * 3);
    for (int i = 0; i < 24; ++i)
    {
        outLines.push_back(corners[indices[i]].x);
        outLines.push_back(corners[indices[i]].y);
        outLines.push_back(corners[indices[i]].z);
    }
}

bool drawShape(void *userShape, b3WorldTransform transform, b3HexColor color, void *context)
{
    if (!context || !userShape)
        return false;
    App *app = (App *)context;

    MyDebugShape *myShape = (MyDebugShape *)userShape;
    b3Vec3 pos = b3ToVec3(transform.p);

    glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, pos.y, pos.z));
    glm::quat q(transform.q.s, transform.q.v.x, transform.q.v.y, transform.q.v.z);
    modelMatrix = modelMatrix * glm::mat4_cast(q);

    glm::mat4 mvp = app->projView3D * modelMatrix;

    std::vector<float> lineVertices;
    if (myShape->type == b3_hullShape)
    {
        glm::vec3 extents(myShape->extents.x, myShape->extents.y, myShape->extents.z);
        getWireframeBoxLines(extents, lineVertices);
    }
    else if (myShape->type == b3_sphereShape)
    {
        const int segments = 24;
        float r = myShape->radius;
        const float twoPi = glm::two_pi<float>();

        for (int i = 0; i < segments; ++i)
        {
            float a1 = ((float)i / segments) * twoPi;
            float a2 = ((float)(i + 1) / segments) * twoPi;

            float c1 = std::cos(a1) * r, s1 = std::sin(a1) * r;
            float c2 = std::cos(a2) * r, s2 = std::sin(a2) * r;

            lineVertices.push_back(c1); lineVertices.push_back(s1); lineVertices.push_back(0.0f);
            lineVertices.push_back(c2); lineVertices.push_back(s2); lineVertices.push_back(0.0f);

            lineVertices.push_back(c1); lineVertices.push_back(0.0f); lineVertices.push_back(s1);
            lineVertices.push_back(c2); lineVertices.push_back(0.0f); lineVertices.push_back(s2);

            lineVertices.push_back(0.0f); lineVertices.push_back(c1); lineVertices.push_back(s1);
            lineVertices.push_back(0.0f); lineVertices.push_back(c2); lineVertices.push_back(s2);
        }
    }
    else if (myShape->type == b3_capsuleShape)
    {
        const int segments = 16;
        float r = myShape->radius;
        b3Vec3 p1 = myShape->p1;
        b3Vec3 p2 = myShape->p2;
        const float twoPi = glm::two_pi<float>();
        const float pi = glm::pi<float>();

        for (int i = 0; i < segments; ++i)
        {
            float a1 = ((float)i / segments) * twoPi;
            float a2 = ((float)(i + 1) / segments) * twoPi;

            float c1 = std::cos(a1) * r, s1 = std::sin(a1) * r;
            float c2 = std::cos(a2) * r, s2 = std::sin(a2) * r;

            lineVertices.push_back(p1.x + c1); lineVertices.push_back(p1.y); lineVertices.push_back(p1.z + s1);
            lineVertices.push_back(p1.x + c2); lineVertices.push_back(p1.y); lineVertices.push_back(p1.z + s2);

            lineVertices.push_back(p2.x + c1); lineVertices.push_back(p2.y); lineVertices.push_back(p2.z + s1);
            lineVertices.push_back(p2.x + c2); lineVertices.push_back(p2.y); lineVertices.push_back(p2.z + s2);
        }

        float offsets[4][2] = { { r, 0.0f }, { -r, 0.0f }, { 0.0f, r }, { 0.0f, -r } };
        for (int i = 0; i < 4; ++i)
        {
            lineVertices.push_back(p1.x + offsets[i][0]); lineVertices.push_back(p1.y); lineVertices.push_back(p1.z + offsets[i][1]);
            lineVertices.push_back(p2.x + offsets[i][0]); lineVertices.push_back(p2.y); lineVertices.push_back(p2.z + offsets[i][1]);
        }

        for (int i = 0; i < segments / 2; ++i)
        {
            float a1 = ((float)i / (segments / 2)) * pi;
            float a2 = ((float)(i + 1) / (segments / 2)) * pi;

            float c1 = std::cos(a1) * r, s1 = std::sin(a1) * r;
            float c2 = std::cos(a2) * r, s2 = std::sin(a2) * r;

            lineVertices.push_back(p2.x + c1); lineVertices.push_back(p2.y + s1); lineVertices.push_back(p2.z);
            lineVertices.push_back(p2.x + c2); lineVertices.push_back(p2.y + s2); lineVertices.push_back(p2.z);

            lineVertices.push_back(p1.x + c1); lineVertices.push_back(p1.y - s1); lineVertices.push_back(p1.z);
            lineVertices.push_back(p1.x + c2); lineVertices.push_back(p1.y - s2); lineVertices.push_back(p1.z);
        }
    }

    if (lineVertices.empty())
        return true;

    glUseProgram(app->lineShaderProgram);
    glUniformMatrix4fv(app->uLineMvpMatrixLocation, 1, GL_FALSE, glm::value_ptr(mvp));

    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = (color & 0xFF) / 255.0f;
    glUniform4f(app->uLineColorLocation, r, g, b, 1.0f);

    glBindVertexArray(app->lineVao);
    glBindBuffer(GL_ARRAY_BUFFER, app->lineVbo);
    glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(float), lineVertices.data(), GL_DYNAMIC_DRAW);

    glDrawArrays(GL_LINES, 0, (GLsizei)(lineVertices.size() / 3));

    glBindVertexArray(0);
    return true;
}
