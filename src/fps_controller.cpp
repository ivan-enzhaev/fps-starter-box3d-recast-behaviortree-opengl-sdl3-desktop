#include "fps_controller.h"
#include "app.h"
#include <cmath>

void handleFpsEvents(App *app, SDL_Event *event)
{
    // Mouse Look
    if (event->type == SDL_EVENT_MOUSE_MOTION)
    {
        if (app->mouseCaptured)
        {
            app->yaw += event->motion.xrel * app->mouseSensitivity;
            app->pitch -= event->motion.yrel * app->mouseSensitivity;

            if (app->pitch > 89.0f)
                app->pitch = 89.0f;
            if (app->pitch < -89.0f)
                app->pitch = -89.0f;
        }
    }

    // Click to capture mouse
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        if (!app->mouseCaptured)
        {
            SDL_SetWindowRelativeMouseMode(app->window, true);
            app->mouseCaptured = true;
        }
    }

    // Key presses
    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat)
    {
        switch (event->key.scancode)
        {
            case SDL_SCANCODE_SPACE:
                app->jumpRequested = true;
                break;

            // Toggle Physics & AI Debug Lines with 'P' or 'F1'
            case SDL_SCANCODE_P:
            case SDL_SCANCODE_F1:
                app->showPhysicsDebug = !app->showPhysicsDebug;
                SDL_Log("Debug Visualization: %s", app->showPhysicsDebug ? "ON" : "OFF");
                break;

            // Toggle Ground Texture/Plane visibility with 'G'
            case SDL_SCANCODE_G:
                app->showGround = !app->showGround;
                SDL_Log("Ground Plane: %s", app->showGround ? "SHOWN" : "HIDDEN");
                break;

            case SDL_SCANCODE_ESCAPE:
                app->mouseCaptured = false;
                SDL_SetWindowRelativeMouseMode(app->window, false);
                break;

            default:
                break;
        }
    }
}

void updateFpsController(App *app, float deltaTime)
{
    // 1. Calculate Camera Front Vector
    glm::vec3 front;
    front.x = std::cos(glm::radians(app->yaw)) * std::cos(glm::radians(app->pitch));
    front.y = std::sin(glm::radians(app->pitch));
    front.z = std::sin(glm::radians(app->yaw)) * std::cos(glm::radians(app->pitch));
    app->cameraFront = glm::normalize(front);

    // 2. Horizontal Movement Directions (flattened on ground plane)
    glm::vec3 forwardDir = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
    glm::vec3 rightDir = glm::normalize(glm::cross(forwardDir, glm::vec3(0.0f, 1.0f, 0.0f)));

    // 3. Direct Keyboard Polling (WASD)
    const bool *keys = SDL_GetKeyboardState(nullptr);
    glm::vec3 moveDir(0.0f);

    if (keys)
    {
        if (keys[SDL_SCANCODE_W])
            moveDir += forwardDir;
        if (keys[SDL_SCANCODE_S])
            moveDir -= forwardDir;
        if (keys[SDL_SCANCODE_D])
            moveDir += rightDir;
        if (keys[SDL_SCANCODE_A])
            moveDir -= rightDir;
    }

    // 4. Apply Velocity to Character Capsule
    float moveSpeed = 6.0f;
    b3Vec3 curVel = b3Body_GetLinearVelocity(app->capsuleId);
    float targetVx = 0.0f;
    float targetVz = 0.0f;

    if (glm::length(moveDir) > 0.001f)
    {
        moveDir = glm::normalize(moveDir);
        targetVx = moveDir.x * moveSpeed;
        targetVz = moveDir.z * moveSpeed;
    }

    float vy = curVel.y;
    // Jump if on the ground
    if (app->jumpRequested && std::abs(curVel.y) < 0.2f)
    {
        vy = 5.5f;
        app->jumpRequested = false;
    }

    b3Body_SetLinearVelocity(app->capsuleId, (b3Vec3) { targetVx, vy, targetVz });
}
