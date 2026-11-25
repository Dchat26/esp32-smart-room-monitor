#pragma once

void netBegin();
void netLoop();                        // loop()에서 매번 호출
void netPublishState();
void netPublishEvent(const char* msg);
