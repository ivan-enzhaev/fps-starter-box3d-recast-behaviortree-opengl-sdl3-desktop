#define SDL_MAIN_USE_CALLBACKS 1

#include "app.h"
#include "fps_controller.h"
#include "model_utils.h"
#include "physics_debug.h"
#include "shader_utils.h"
#include "texture_utils.h"
#include "enemy_ai.h"

#include <SDL3/SDL_main.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO))
        return SDL_APP_FAILURE;

    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    App *app = new App();
    *appstate = app;

    app->window = SDL_CreateWindow("SDL3 Box3D FPS + Recast + BT.CPP", 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    app->glContext = SDL_GL_CreateContext(app->window);
    SDL_GL_MakeCurrent(app->window, app->glContext);
    SDL_SetWindowRelativeMouseMode(app->window, true);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
        return SDL_APP_FAILURE;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 1. Shaders & Debug Line Buffers
    app->shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    app->uMvpMatrixLocation = glGetUniformLocation(app->shaderProgram, "uMVP");
    app->uTextureLocation = glGetUniformLocation(app->shaderProgram, "ourTexture");

    app->lineShaderProgram = createShaderProgram(lineVertexShaderSource, lineFragmentShaderSource);
    app->uLineMvpMatrixLocation = glGetUniformLocation(app->lineShaderProgram, "uMvpMatrix");
    app->uLineColorLocation = glGetUniformLocation(app->lineShaderProgram, "uColor");
    initLineRenderer(app);

    // 2. Load Model, Texture & Ground Plane
    if (!loadCubeModel(app, "assets/models/cube/check-grid-cube.glb"))
        return SDL_APP_FAILURE;
    app->cubeTextureID = createTexture("assets/models/cube/check-grid-512.jpg");
    if (!app->cubeTextureID)
        return SDL_APP_FAILURE;

    initGroundPlane(app, 30.0f, 30.0f);

    // 3. Physics World & Debug Callbacks
    app->dd = b3DefaultDebugDraw();
    app->dd.DrawShapeFcn = drawShape;
    app->dd.drawShapes = true;
    app->dd.context = app;
    app->dd.drawingBounds = (b3AABB) { { -100.0f, -100.0f, -100.0f }, { 100.0f, 100.0f, 100.0f } };

    b3WorldDef worldDef = b3DefaultWorldDef();
    worldDef.gravity = (b3Vec3) { 0.0f, -9.8f, 0.0f };
    worldDef.createDebugShape = createDebugShape;
    worldDef.destroyDebugShape = destroyDebugShape;
    app->worldId = b3CreateWorld(&worldDef);

    // Static Ground Body (60x60m platform)
    b3BodyDef groundBodyDef = b3DefaultBodyDef();
    groundBodyDef.position = (b3Vec3) { 0.0f, -0.2f, 0.0f };
    app->groundId = b3CreateBody(app->worldId, &groundBodyDef);
    b3BoxHull groundBox = b3MakeBoxHull(30.0f, 0.2f, 30.0f);
    b3ShapeDef groundShapeDef = b3DefaultShapeDef();
    b3CreateHullShape(app->groundId, &groundShapeDef, &groundBox.base);

    // Falling Cube Body
    b3BodyDef boxBodyDef = b3DefaultBodyDef();
    boxBodyDef.type = b3_dynamicBody;
    boxBodyDef.position = (b3Vec3) { 0.0f, 6.0f, 0.0f };
    b3Vec3 rotAxis = b3Normalize((b3Vec3) { 1.0f, 1.0f, 1.0f });
    boxBodyDef.rotation = b3MakeQuatFromAxisAngle(rotAxis, 25.0f * (3.14159265f / 180.0f));

    app->boxId = b3CreateBody(app->worldId, &boxBodyDef);
    b3BoxHull boxHull = b3MakeBoxHull(app->cubeHalfExtent, app->cubeHalfExtent, app->cubeHalfExtent);
    b3ShapeDef boxShapeDef = b3DefaultShapeDef();
    boxShapeDef.density = 1.0f;
    boxShapeDef.baseMaterial.friction = 0.5f;
    boxShapeDef.baseMaterial.restitution = 0.3f;
    b3CreateHullShape(app->boxId, &boxShapeDef, &boxHull.base);

    // Player Capsule Body
    b3BodyDef capsuleBodyDef = b3DefaultBodyDef();
    capsuleBodyDef.type = b3_dynamicBody;
    capsuleBodyDef.position = (b3Vec3) { 0.0f, 1.5f, 6.0f };

    b3MotionLocks locks = { 0 };
    locks.angularX = true;
    locks.angularY = true;
    locks.angularZ = true;
    locks.linearX = false;
    locks.linearY = false;
    locks.linearZ = false;
    capsuleBodyDef.motionLocks = locks;

    app->capsuleId = b3CreateBody(app->worldId, &capsuleBodyDef);
    b3Capsule capsule = { { 0.0f, -0.5f, 0.0f }, { 0.0f, 0.5f, 0.0f }, 0.4f };
    b3ShapeDef capsuleShapeDef = b3DefaultShapeDef();
    capsuleShapeDef.density = 1.0f;
    capsuleShapeDef.baseMaterial.friction = 0.4f;
    capsuleShapeDef.baseMaterial.restitution = 0.0f;
    b3CreateCapsuleShape(app->capsuleId, &capsuleShapeDef, &capsule);

    // 4. Initialize Recast/Detour Navigation & BehaviorTree Enemy AI (4 Follower Cubes)
    initEnemyAI(app, 4);

    app->lastTicks = SDL_GetTicksNS();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    auto *app = static_cast<App *>(appstate);
    if (event->type == SDL_EVENT_QUIT)
        return SDL_APP_SUCCESS;

    handleFpsEvents(app, event);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    auto *app = static_cast<App *>(appstate);

    Uint64 currentTicks = SDL_GetTicksNS();
    float deltaTime = static_cast<float>(currentTicks - app->lastTicks) / 1e9f;
    app->lastTicks = currentTicks;

    // 1. Controller, Enemy AI & Physics Step
    updateFpsController(app, deltaTime);
    updateEnemyAI(app, deltaTime);
    b3World_Step(app->worldId, 1.0f / 60.0f, 5);

    // 2. Camera View-Projection
    b3Vec3 capPos = b3Body_GetPosition(app->capsuleId);
    glm::vec3 cameraPos(capPos.x, capPos.y + 0.7f, capPos.z);

    int winW = 800, winH = 450;
    SDL_GetWindowSizeInPixels(app->window, &winW, &winH);
    glViewport(0, 0, winW, winH);

    float aspect = (float)winW / (float)(winH == 0 ? 1 : winH);
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + app->cameraFront, glm::vec3(0.0f, 1.0f, 0.0f));
    app->projView3D = proj * view;

    // 3. Clear Buffers
    glClearColor(0.14f, 0.15f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 4. Debug Overlays (Rendered when toggled via P or F1)
    if (app->showPhysicsDebug)
    {
        // 4a. Box3D Physics Wireframes (Capsule, falling box, enemy colliders, ground box)
        b3World_Draw(app->worldId, &app->dd, B3_DEFAULT_MASK_BITS);

        // 4b. Recast NavMesh Grid & BehaviorTree State rings / chase rays
        renderEnemyAIDebug(app);
    }

    // 5. Setup Texture Shader for Ground, Falling Cube, and Enemy Cubes
    glUseProgram(app->shaderProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, app->cubeTextureID);
    glUniform1i(app->uTextureLocation, 0);

    // 5a. Draw Textured Ground Plane (Only if toggled ON via G)
    if (app->showGround)
    {
        glm::mat4 groundModel = glm::mat4(1.0f);
        glm::mat4 groundMvp = app->projView3D * groundModel;
        glUniformMatrix4fv(app->uMvpMatrixLocation, 1, GL_FALSE, glm::value_ptr(groundMvp));

        glBindVertexArray(app->groundVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    // 5b. Draw Original Falling Textured Cube
    b3Vec3 boxPos = b3Body_GetPosition(app->boxId);
    b3Quat boxRot = b3Body_GetRotation(app->boxId);

    glm::mat4 cubeModel = glm::translate(glm::mat4(1.0f), glm::vec3(boxPos.x, boxPos.y, boxPos.z));
    glm::quat q(boxRot.s, boxRot.v.x, boxRot.v.y, boxRot.v.z);
    cubeModel = cubeModel * glm::mat4_cast(q);

    glm::mat4 cubeMvp = app->projView3D * cubeModel;
    glUniformMatrix4fv(app->uMvpMatrixLocation, 1, GL_FALSE, glm::value_ptr(cubeMvp));

    glBindVertexArray(app->cubeVao);
    glDrawElements(GL_TRIANGLES, app->cubeIndexCount, app->cubeIndexType, 0);

    // 5c. Draw AI Follower Enemy Cubes
    renderEnemyCubes(app);

    SDL_GL_SwapWindow(app->window);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    if (appstate)
    {
        auto *app = static_cast<App *>(appstate);

        cleanupEnemyAI(app);
        b3DestroyWorld(app->worldId);

        glDeleteProgram(app->lineShaderProgram);
        glDeleteVertexArrays(1, &app->lineVao);
        glDeleteBuffers(1, &app->lineVbo);

        glDeleteTextures(1, &app->cubeTextureID);
        glDeleteVertexArrays(1, &app->cubeVao);
        glDeleteBuffers(1, &app->cubeVbo);
        glDeleteBuffers(1, &app->cubeUvVbo);
        glDeleteBuffers(1, &app->cubeEbo);

        glDeleteVertexArrays(1, &app->groundVao);
        glDeleteBuffers(1, &app->groundVbo);

        glDeleteProgram(app->shaderProgram);

        SDL_GL_DestroyContext(app->glContext);
        SDL_DestroyWindow(app->window);
        delete app;
    }
    SDL_Quit();
}
