#pragma once

#include <TFT_eSPI.h>
#include "config.h"
#include "settings_manager.h"

enum class SettingRowType : uint8_t {
    Text = 0,
    Toggle,
    Arrow,
    Badge,
};

enum class SettingRowId : int8_t {
    None = -1,
    DeviceName,
    CurrentTime,
    AutoNtp,
    CalibrateTouch,
    WifiEnable,
    WifiNetwork,
    WifiStatus,
    WorkStart,
    WorkEnd,
    LateThreshold,
    CheckInMode,
    TotalEnrolled,
    ViewUsers,
    DeleteAllUsers,
    ExportData,
    ClearRecords,
    StorageUsage,
    FactoryReset,
};

struct SettingsUiState {
    AppSettings settings{};
    int scrollY = 0;
    float velocity = 0.0f;
    int totalContentHeight = 0;
    TFT_eSprite listSprite;
    bool spriteReady = false;
    bool dragging = false;
    int lastTouchY = 0;

    enum class Dialog : uint8_t {
        None,
        ConfirmDeleteUsers,
        ConfirmClearRecords,
        ConfirmFactoryReset,
        ConfirmDeleteUsers2,
        ConfirmClearRecords2,
        ConfirmFactoryReset2,
    } dialog = Dialog::None;
};

extern SettingsUiState gSettingsUi;

constexpr int SETTINGS_HEADER_H = 36;
constexpr int SETTINGS_LIST_Y = SETTINGS_HEADER_H;
constexpr int SETTINGS_LIST_H = SCREEN_HEIGHT - SETTINGS_HEADER_H;

void settingsUiInit();
void drawSettingRow(TFT_eSPI &tft, int y, const char *icon, const char *label, const char *value,
                    SettingRowType type, bool toggleOn = false, bool danger = false);
void drawSettingsScreen();
void settingsScrollList(int delta);
void settingsTickInertia();
void settingsHandleTouchDown(int x, int y);
void settingsHandleTouchMove(int x, int y);
void settingsHandleTouchUp(int x, int y);
bool settingsHandleTap(int x, int y);
bool settingsHandleDialogTap(int x, int y);
SettingRowId settingsRowAt(int x, int y);

void drawUsersListScreen();
