#pragma once

#include <TFT_eSPI.h>
#include <time.h>
#include "config.h"
#include "storage.h"

struct RecordsUiState {
    RecordFilter filter = RecordFilter::Today;
    time_t dayAnchor = 0;
    AttendanceRecordView items[100];
    int itemCount = 0;
    RecordsSummary summary{};
    int scrollY = 0;
    float velocity = 0.0f;
    int expandedIndex = -1;
    int totalContentHeight = 0;
    TFT_eSprite listSprite;
    bool spriteReady = false;
    int lastTouchY = 0;
    bool dragging = false;
};

extern RecordsUiState gRecordsUi;

void recordsReload();
void drawRecordsScreen(RecordFilter filter);
void drawRecordRow(int y, const AttendanceRecordView &view, bool expanded);
void scrollRecordsList(int delta);
void recordsTickInertia();
void recordsHandleTouchDown(int x, int y);
void recordsHandleTouchMove(int x, int y);
void recordsHandleTouchUp(int x, int y);
/** @return true if tap was consumed (header/filter/export); false = list area */
bool recordsHandleChromeTap(int x, int y);
int recordsRowHeight(int index);
int recordsHitRow(int x, int y, int &outIndex);
