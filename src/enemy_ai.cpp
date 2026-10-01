#include "enemy_ai.h"
#include "app.h"
#include "physics_debug.h"

// Detour & Recast Navigation Headers
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>
#include <DetourCrowd.h>

// BehaviorTree.CPP 4.x Headers
#include <behaviortree_cpp/behavior_tree.h>
#include <behaviortree_cpp/bt_factory.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <cstring>
#include <vector>

// ----------------------------------------------------------------------------
// BehaviorTree.CPP Custom Nodes (Shooter AI Logic)
// ----------------------------------------------------------------------------
class IsPlayerDetected : public BT::ConditionNode {
public:
    IsPlayerDetected(const std::string& name, const BT::NodeConfig& config)
        : BT::ConditionNode(name, config) {}

    static BT::PortsList providedPorts() {
        return { BT::InputPort<float>("range") };
    }

    BT::NodeStatus tick() override {
        float range = 35.0f;
        getInput("range", range);
        float dist = 100.0f;
        (void)config().blackboard->get("distance_to_player", dist);
        return (dist <= range) ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    }
};

class IsPlayerInAttackRange : public BT::ConditionNode {
public:
    IsPlayerInAttackRange(const std::string& name, const BT::NodeConfig& config)
        : BT::ConditionNode(name, config) {}

    static BT::PortsList providedPorts() {
        return { BT::InputPort<float>("range") };
    }

    BT::NodeStatus tick() override {
        float range = 2.5f;
        getInput("range", range);
        float dist = 100.0f;
        (void)config().blackboard->get("distance_to_player", dist);
        return (dist <= range) ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    }
};

class ChasePlayer : public BT::SyncActionNode {
public:
    ChasePlayer(const std::string& name, const BT::NodeConfig& config)
        : BT::SyncActionNode(name, config) {}

    static BT::PortsList providedPorts() { return {}; }

    BT::NodeStatus tick() override {
        config().blackboard->set("ai_state", 1); // 1 = Chasing
        return BT::NodeStatus::SUCCESS;
    }
};

class MeleeAttack : public BT::SyncActionNode {
public:
    MeleeAttack(const std::string& name, const BT::NodeConfig& config)
        : BT::SyncActionNode(name, config) {}

    static BT::PortsList providedPorts() { return {}; }

    BT::NodeStatus tick() override {
        config().blackboard->set("ai_state", 2); // 2 = Attacking
        return BT::NodeStatus::SUCCESS;
    }
};

class Idle : public BT::SyncActionNode {
public:
    Idle(const std::string& name, const BT::NodeConfig& config)
        : BT::SyncActionNode(name, config) {}

    static BT::PortsList providedPorts() { return {}; }

    BT::NodeStatus tick() override {
        config().blackboard->set("ai_state", 0); // 0 = Idle
        return BT::NodeStatus::SUCCESS;
    }
};

// ----------------------------------------------------------------------------
// Behavior Tree XML Definition
// ----------------------------------------------------------------------------
static const char* enemyBtXml = R"(
<root BTCPP_format="4">
  <BehaviorTree ID="ShooterEnemyTree">
    <Fallback>
      <Sequence>
        <IsPlayerInAttackRange range="2.5"/>
        <MeleeAttack/>
      </Sequence>
      <Sequence>
        <IsPlayerDetected range="35.0"/>
        <ChasePlayer/>
      </Sequence>
      <Idle/>
    </Fallback>
  </BehaviorTree>
</root>
)";

// Navigation & AI State
static dtNavMesh* s_navMesh = nullptr;
static dtNavMeshQuery* s_navQuery = nullptr;
static dtCrowd* s_crowd = nullptr;

static std::vector<EnemyAgent> s_enemies;
static std::vector<BT::Tree> s_enemyTrees;
static std::vector<BT::Blackboard::Ptr> s_blackboards;

// ----------------------------------------------------------------------------
// Build Detour NavMesh for the 60x60m ground using quantized vertices
// ----------------------------------------------------------------------------
static bool buildGroundNavMesh(float halfSize = 30.0f) {
    const int N = 6;
    const float cs = 1.0f;
    const float ch = 0.2f;
    const float bmin[3] = { -halfSize, -1.0f, -halfSize };
    const float bmax[3] = {  halfSize,  5.0f,  halfSize };

    std::vector<unsigned short> verts;
    for (int z = 0; z <= N; ++z) {
        for (int x = 0; x <= N; ++x) {
            float vx = -halfSize + (2.0f * halfSize) * ((float)x / N);
            float vy = 0.0f;
            float vz = -halfSize + (2.0f * halfSize) * ((float)z / N);

            unsigned short qx = (unsigned short)std::round((vx - bmin[0]) / cs);
            unsigned short qy = (unsigned short)std::round((vy - bmin[1]) / ch);
            unsigned short qz = (unsigned short)std::round((vz - bmin[2]) / cs);

            verts.push_back(qx);
            verts.push_back(qy);
            verts.push_back(qz);
        }
    }

    std::vector<unsigned short> polys;
    std::vector<unsigned char> polyAreas;
    std::vector<unsigned short> polyFlags;

    for (int z = 0; z < N; ++z) {
        for (int x = 0; x < N; ++x) {
            unsigned short v0 = (unsigned short)(z * (N + 1) + x);
            unsigned short v1 = (unsigned short)(z * (N + 1) + x + 1);
            unsigned short v2 = (unsigned short)((z + 1) * (N + 1) + x + 1);
            unsigned short v3 = (unsigned short)((z + 1) * (N + 1) + x);

            // Triangle 1: v0 -> v1 -> v2
            polys.push_back(v0); polys.push_back(v1); polys.push_back(v2);
            polys.push_back(0xffff); polys.push_back(0xffff); polys.push_back(0xffff);
            polyAreas.push_back(1);
            polyFlags.push_back(1);

            // Triangle 2: v0 -> v2 -> v3
            polys.push_back(v0); polys.push_back(v2); polys.push_back(v3);
            polys.push_back(0xffff); polys.push_back(0xffff); polys.push_back(0xffff);
            polyAreas.push_back(1);
            polyFlags.push_back(1);
        }
    }

    dtNavMeshCreateParams params;
    memset(&params, 0, sizeof(params));
    params.verts = verts.data();
    params.vertCount = (int)(verts.size() / 3);
    params.polys = polys.data();
    params.polyAreas = polyAreas.data();
    params.polyFlags = polyFlags.data();
    params.polyCount = (int)(polyAreas.size());
    params.nvp = 3;
    params.bmin[0] = bmin[0]; params.bmin[1] = bmin[1]; params.bmin[2] = bmin[2];
    params.bmax[0] = bmax[0]; params.bmax[1] = bmax[1]; params.bmax[2] = bmax[2];
    params.walkableHeight = 2.0f;
    params.walkableRadius = 0.6f;
    params.walkableClimb = 0.5f;
    params.cs = cs;
    params.ch = ch;
    params.buildBvTree = true;

    unsigned char* navData = nullptr;
    int navDataSize = 0;
    if (!dtCreateNavMeshData(&params, &navData, &navDataSize)) {
        SDL_Log("Failed to create Detour NavMesh data!");
        return false;
    }

    s_navMesh = dtAllocNavMesh();
    if (!s_navMesh || dtStatusFailed(s_navMesh->init(navData, navDataSize, DT_TILE_FREE_DATA))) {
        SDL_Log("Failed to initialize Detour NavMesh!");
        return false;
    }

    s_navQuery = dtAllocNavMeshQuery();
    s_navQuery->init(s_navMesh, 2048);

    s_crowd = dtAllocCrowd();
    s_crowd->init(20, 0.6f, s_navMesh);

    return true;
}

void initEnemyAI(App* app, int numEnemies) {
    if (!buildGroundNavMesh(30.0f)) return;

    BT::BehaviorTreeFactory factory;
    factory.registerNodeType<IsPlayerDetected>("IsPlayerDetected");
    factory.registerNodeType<IsPlayerInAttackRange>("IsPlayerInAttackRange");
    factory.registerNodeType<ChasePlayer>("ChasePlayer");
    factory.registerNodeType<MeleeAttack>("MeleeAttack");
    factory.registerNodeType<Idle>("Idle");

    float spawnOffsets[4][2] = {
        { -12.0f, -8.0f },
        {  12.0f, -8.0f },
        { -15.0f,  12.0f },
        {  15.0f,  12.0f }
    };

    s_enemies.clear();
    s_enemyTrees.clear();
    s_blackboards.clear();

    for (int i = 0; i < numEnemies; ++i) {
        float startX = spawnOffsets[i % 4][0];
        float startZ = spawnOffsets[i % 4][1];

        EnemyAgent enemy;
        enemy.position = glm::vec3(startX, 1.0f, startZ);

        b3BodyDef bodyDef = b3DefaultBodyDef();
        bodyDef.type = b3_dynamicBody;
        bodyDef.position = (b3Vec3){ startX, 1.0f, startZ };

        b3MotionLocks locks = { 0 };
        locks.angularX = true;
        locks.angularY = false;
        locks.angularZ = true;
        locks.linearX = false;
        locks.linearY = false;
        locks.linearZ = false;
        bodyDef.motionLocks = locks;

        enemy.bodyId = b3CreateBody(app->worldId, &bodyDef);
        b3BoxHull boxHull = b3MakeBoxHull(app->cubeHalfExtent, app->cubeHalfExtent, app->cubeHalfExtent);
        b3ShapeDef shapeDef = b3DefaultShapeDef();
        shapeDef.density = 2.0f;
        shapeDef.baseMaterial.friction = 0.5f;
        b3CreateHullShape(enemy.bodyId, &shapeDef, &boxHull.base);

        dtCrowdAgentParams ap;
        memset(&ap, 0, sizeof(ap));
        ap.radius = 0.6f;
        ap.height = 1.8f;
        ap.maxAcceleration = 8.0f;
        ap.maxSpeed = 3.5f + (i * 0.3f);
        ap.collisionQueryRange = ap.radius * 6.0f;
        ap.pathOptimizationRange = ap.radius * 12.0f;
        ap.updateFlags = DT_CROWD_ANTICIPATE_TURNS | DT_CROWD_OPTIMIZE_VIS | DT_CROWD_OPTIMIZE_TOPO | DT_CROWD_OBSTACLE_AVOIDANCE | DT_CROWD_SEPARATION;
        ap.obstacleAvoidanceType = 0;
        ap.separationWeight = 2.5f;

        float startPos[3] = { startX, 0.0f, startZ };
        enemy.crowdAgentId = s_crowd->addAgent(startPos, &ap);

        auto blackboard = BT::Blackboard::create();
        blackboard->set("distance_to_player", 100.0f);
        blackboard->set("ai_state", 0);

        BT::Tree tree = factory.createTreeFromText(enemyBtXml, blackboard);

        s_enemies.push_back(enemy);
        s_enemyTrees.push_back(std::move(tree));
        s_blackboards.push_back(blackboard);
    }

    SDL_Log("Recast NavMesh and BehaviorTree initialized with %d enemies.", numEnemies);
}

void updateEnemyAI(App* app, float deltaTime) {
    if (!s_crowd || !s_navQuery) return;

    b3Vec3 playerCapPos = b3Body_GetPosition(app->capsuleId);
    glm::vec3 playerPos(playerCapPos.x, 0.0f, playerCapPos.z);

    dtQueryFilter filter;
    filter.setIncludeFlags(0xffff);
    filter.setExcludeFlags(0);

    float extents[3] = { 4.0f, 4.0f, 4.0f };
    dtPolyRef playerPolyRef = 0;
    float nearestTargetPos[3] = { 0 };
    float targetPos[3] = { playerPos.x, 0.0f, playerPos.z };

    s_navQuery->findNearestPoly(targetPos, extents, &filter, &playerPolyRef, nearestTargetPos);

    for (size_t i = 0; i < s_enemies.size(); ++i) {
        auto& enemy = s_enemies[i];
        float dist = glm::distance(enemy.position, playerPos);

        s_blackboards[i]->set("distance_to_player", dist);
        s_enemyTrees[i].tickOnce();

        int state = 0;
        (void)s_blackboards[i]->get("ai_state", state);
        enemy.aiState = state;

        if (state == 1 && playerPolyRef) {
            s_crowd->requestMoveTarget(enemy.crowdAgentId, playerPolyRef, nearestTargetPos);
        } else if (state == 2) {
            s_crowd->resetMoveTarget(enemy.crowdAgentId);
        }
    }

    s_crowd->update(deltaTime, nullptr);

    for (auto& enemy : s_enemies) {
        const dtCrowdAgent* ag = s_crowd->getAgent(enemy.crowdAgentId);
        if (ag && ag->active) {
            b3Vec3 curBodyPos = b3Body_GetPosition(enemy.bodyId);
            b3Vec3 curBodyVel = b3Body_GetLinearVelocity(enemy.bodyId);

            float vx = (ag->npos[0] - curBodyPos.x) / (deltaTime > 0.001f ? deltaTime : 0.016f);
            float vz = (ag->npos[2] - curBodyPos.z) / (deltaTime > 0.001f ? deltaTime : 0.016f);

            float vLen = std::sqrt(vx * vx + vz * vz);
            if (vLen > 8.0f) {
                vx = (vx / vLen) * 8.0f;
                vz = (vz / vLen) * 8.0f;
            }

            b3Body_SetLinearVelocity(enemy.bodyId, (b3Vec3){ vx, curBodyVel.y, vz });

            glm::vec3 toPlayer = playerPos - enemy.position;
            toPlayer.y = 0.0f;
            if (glm::length(toPlayer) > 0.1f) {
                toPlayer = glm::normalize(toPlayer);
                float angle = std::atan2(toPlayer.x, toPlayer.z);
                b3Quat targetRot = b3MakeQuatFromAxisAngle((b3Vec3){ 0.0f, 1.0f, 0.0f }, angle);
                b3Body_SetTransform(enemy.bodyId, curBodyPos, targetRot);
                enemy.rotation = glm::quat(targetRot.s, targetRot.v.x, targetRot.v.y, targetRot.v.z);
            }

            b3Vec3 finalPos = b3Body_GetPosition(enemy.bodyId);
            enemy.position = glm::vec3(finalPos.x, finalPos.y, finalPos.z);
        }
    }
}

void renderEnemyCubes(App* app) {
    for (const auto& enemy : s_enemies) {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), enemy.position);
        model = model * glm::mat4_cast(enemy.rotation);

        glm::mat4 mvp = app->projView3D * model;
        glUniformMatrix4fv(app->uMvpMatrixLocation, 1, GL_FALSE, glm::value_ptr(mvp));

        glBindVertexArray(app->cubeVao);
        glDrawElements(GL_TRIANGLES, app->cubeIndexCount, app->cubeIndexType, 0);
    }
    glBindVertexArray(0);
}

// ----------------------------------------------------------------------------
// Render Recast NavMesh & BehaviorTree Debug Overlay
// ----------------------------------------------------------------------------
void renderEnemyAIDebug(App* app) {
    if (!app) return;

    b3Vec3 playerCapPos = b3Body_GetPosition(app->capsuleId);
    glm::vec3 playerPos(playerCapPos.x, 0.05f, playerCapPos.z);

    // 1. Draw NavMesh Grid Wireframe (Use const dtNavMesh* to call public getTile)
    if (s_navMesh) {
        const dtNavMesh* navMesh = s_navMesh;
        std::vector<float> navLines;
        for (int i = 0; i < navMesh->getMaxTiles(); ++i) {
            const dtMeshTile* tile = navMesh->getTile(i);
            if (!tile || !tile->header) continue;

            for (int p = 0; p < tile->header->polyCount; ++p) {
                const dtPoly* poly = &tile->polys[p];
                for (int j = 0, k = (int)poly->vertCount - 1; j < (int)poly->vertCount; k = j++) {
                    const float* v0 = &tile->verts[poly->verts[k] * 3];
                    const float* v1 = &tile->verts[poly->verts[j] * 3];

                    navLines.push_back(v0[0]);
                    navLines.push_back(0.04f); // Slightly above ground
                    navLines.push_back(v0[2]);

                    navLines.push_back(v1[0]);
                    navLines.push_back(0.04f);
                    navLines.push_back(v1[2]);
                }
            }
        }
        drawDebugLines(app, navLines, glm::vec4(0.2f, 0.8f, 1.0f, 0.5f));
    }

    // 2. Draw Enemy AI Status Indicators
    std::vector<float> chaseLines;
    for (const auto& enemy : s_enemies) {
        glm::vec3 footPos = glm::vec3(enemy.position.x, 0.06f, enemy.position.z);

        // State Ring at feet:
        // Green (Idle = 0), Yellow (Chasing = 1), Red (Attacking = 2)
        glm::vec4 stateColor;
        if (enemy.aiState == 2) {
            stateColor = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f); // Red (Attack)
            // Draw Attack Range Circle (radius 2.5m)
            drawDebugCircle(app, footPos, 2.5f, glm::vec4(1.0f, 0.3f, 0.3f, 0.7f), 32);
        } else if (enemy.aiState == 1) {
            stateColor = glm::vec4(1.0f, 0.85f, 0.1f, 1.0f); // Yellow (Chase)
            // Draw Chase Ray from Enemy to Player
            chaseLines.push_back(enemy.position.x);
            chaseLines.push_back(enemy.position.y);
            chaseLines.push_back(enemy.position.z);

            chaseLines.push_back(playerPos.x);
            chaseLines.push_back(playerCapPos.y + 0.4f);
            chaseLines.push_back(playerPos.z);
        } else {
            stateColor = glm::vec4(0.2f, 1.0f, 0.3f, 1.0f); // Green (Idle)
        }

        drawDebugCircle(app, footPos, 0.9f, stateColor, 20);
    }

    if (!chaseLines.empty()) {
        drawDebugLines(app, chaseLines, glm::vec4(1.0f, 0.85f, 0.1f, 0.9f));
    }
}

void cleanupEnemyAI(App* app) {
    if (s_crowd) {
        dtFreeCrowd(s_crowd);
        s_crowd = nullptr;
    }
    if (s_navQuery) {
        dtFreeNavMeshQuery(s_navQuery);
        s_navQuery = nullptr;
    }
    if (s_navMesh) {
        dtFreeNavMesh(s_navMesh);
        s_navMesh = nullptr;
    }
    s_enemies.clear();
    s_enemyTrees.clear();
    s_blackboards.clear();
}
