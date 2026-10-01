#pragma once

#include <SDL3/SDL.h>
#include <box3d/box3d.h>
#include <glad/glad.h>
#include <glm/glm.hpp>

struct App
{
    SDL_Window *window = nullptr;
    SDL_GLContext glContext = nullptr;

    // Textured Cube Rendering
    GLuint shaderProgram = 0;
    GLint uMvpMatrixLocation = -1;
    GLint uTextureLocation = -1;
    GLuint cubeVao = 0;
    GLuint cubeVbo = 0;
    GLuint cubeUvVbo = 0;
    GLuint cubeEbo = 0;
    GLsizei cubeIndexCount = 0;
    GLenum cubeIndexType = GL_UNSIGNED_SHORT;
    GLuint cubeTextureID = 0;
    float cubeHalfExtent = 1.0f;

    // Ground Plane Rendering
    GLuint groundVao = 0;
    GLuint groundVbo = 0;

    // Camera & First Person Controller
    glm::mat4 projView3D = glm::mat4(1.0f);
    glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    float yaw = -90.0f;
    float pitch = 0.0f;
    float mouseSensitivity = 0.1f;
    bool mouseCaptured = true;

    // Movement Inputs
    bool inputW = false;
    bool inputS = false;
    bool inputA = false;
    bool inputD = false;
    bool jumpRequested = false;

    // Debug Display Toggles
    bool showPhysicsDebug = true; // Toggle with 'P' or 'F1'
    bool showGround = false;      // Toggle with 'G' to hide floor texture for clear lines

    // Box3D Debug Line Rendering
    GLuint lineShaderProgram = 0;
    GLint uLineMvpMatrixLocation = -1;
    GLint uLineColorLocation = -1;
    GLuint lineVao = 0;
    GLuint lineVbo = 0;

    // Physics Engine Bodies & World
    b3WorldId worldId;
    b3BodyId groundId;
    b3BodyId boxId;
    b3BodyId capsuleId;
    b3DebugDraw dd;

    Uint64 lastTicks = 0;
};
