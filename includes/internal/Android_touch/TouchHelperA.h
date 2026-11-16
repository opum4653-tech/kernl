#pragma once

bool Touch_Init(int w, int h, uint32_t orientation_, bool readOnly = false);
void UpdateScreenData(int w, int h, uint32_t orientation_);

void Touch_SetUiRect(float x, float y, float w, float h, bool active);
bool Touch_IsPointInUi(float x, float y);

// Multi-region API: clear once per frame, register every live UI panel,
// then commit so the touch thread sees the whole set at once.
void Touch_ClearUiRects();
void Touch_AddUiRect(float x, float y, float w, float h);
void Touch_CommitUiRects();

void Touch_Close();
void Touch_Down(float x, float y);
void Touch_Move(float x, float y);
void Touch_Up();


