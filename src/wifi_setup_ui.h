#pragma once

#include "display.h"

void wifiSetupEnter();
void wifiSetupExit();
void wifiSetupDraw();
void wifiSetupUpdate();
bool wifiSetupHandleTouch(const TouchPoint &tp);
void wifiSetupHandleTouchDown(int x, int y);
void wifiSetupHandleTouchMove(int x, int y);
void wifiSetupHandleTouchUp(int x, int y);
