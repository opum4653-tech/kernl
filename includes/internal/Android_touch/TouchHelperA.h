#pragma once

bool Touch_Init(int w, int h, uint32_t orientation_, bool readOnly = false);
void UpdateScreenData(int w, int h, uint32_t orientation_);
void Touch_SyncToImGui();

bool Touch_IsPointInUi(float x, float y);

// Multi-region API: clear once per frame, register every live UI panel,
// then commit so the touch thread sees the whole set at once.
void Touch_ClearUiRects();
void Touch_AddUiRect(float x, float y, float w, float h);
void Touch_AddUiCircle(float cx, float cy, float radius);
void Touch_CommitUiRects();

// Slot-addressed injection used by non-gesture callers.
bool IsFingerDown(int slot);
bool GetFingerScreenPos(int slot, float& outX, float& outY);
void Touch_Down_Slot(int slot, float xt, float yt);
void Touch_Move_Slot(int slot, float x, float y);
void Touch_Up_Slot(int slot);

void Touch_Close();
void Touch_Down(float x, float y);
void Touch_Move(float x, float y);
void Touch_Up();


