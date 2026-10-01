#pragma once

#include <box3d/box3d.h>
#include <glm/glm.hpp>
#include <vector>

struct App;

int b3InternalAssert(const char *condition, const char *fileName, int lineNumber);
void *createDebugShape(const b3DebugShape *debugShape, void *context);
void destroyDebugShape(void *userShape, void *context);
bool drawShape(void *userShape, b3WorldTransform transform, b3HexColor color, void *context);

void initLineRenderer(App *app);

// General debug drawing helpers
void drawDebugLines(App *app, const std::vector<float> &lineVertices, const glm::vec4 &color);
void drawDebugCircle(App *app, const glm::vec3 &center, float radius, const glm::vec4 &color, int segments = 24);
