#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int AdaptiveTestEncryption(bool audio, bool video);
void AdaptiveTestBegin(bool enabled, bool sunshine);
void AdaptiveTestEnd(void);
void AdaptiveTestRestartQueueWithoutNegotiation(void);
void AdaptiveTestTime(uint64_t microseconds);
int AdaptiveTestBlock(uint32_t frame, unsigned block, unsigned blocks, unsigned data, unsigned fec, unsigned receivedMask);
int AdaptiveTestPacket(uint32_t frame, unsigned block, unsigned blocks, unsigned data, unsigned fec, unsigned index, unsigned sequence);
int AdaptiveTestDrain(void);
int AdaptiveTestWire(unsigned index, unsigned char* output, int capacity);
int AdaptiveTestDecrypt(unsigned index, unsigned char* output, int capacity);
unsigned AdaptiveTestSubmitted(void);
unsigned AdaptiveTestNextSequence(void);
#ifdef __cplusplus
}
#endif
