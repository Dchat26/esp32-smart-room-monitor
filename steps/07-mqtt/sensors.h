#pragma once

void sensorsBegin();
void sensorsReadInputs();   // PIR, 적외선 (디바운싱 + 이벤트)
void sensorsReadLight();    // 조도
void sensorsReadDht();      // 온습도

const char* lightLabel(int pct);
