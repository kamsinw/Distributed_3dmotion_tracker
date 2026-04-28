#pragma once

void  filterInit();
float filterUpdate(float raw_cm);
float getRelativeDistance();
float getSpeed(float filtered_cm, float dt);
