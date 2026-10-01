#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <box3d/box3d.h>

struct App;

struct EnemyAgent {
    b3BodyId bodyId;
    int crowdAgentId = -1;
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    int aiState = 0; // 0 = Idle, 1 = Chasing, 2 = Attacking
};

void initEnemyAI(App* app, int numEnemies = 3);
void updateEnemyAI(App* app, float deltaTime);
void renderEnemyCubes(App* app);
void renderEnemyAIDebug(App* app);
void cleanupEnemyAI(App* app);
