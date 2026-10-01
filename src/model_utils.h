#pragma once

struct App;

bool loadCubeModel(App *app, const char *modelPath);
void initGroundPlane(App *app, float halfSize = 10.0f, float uvRepeat = 10.0f);
