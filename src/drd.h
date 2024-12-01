#ifndef DRD_H
#define DRD_H

void drdTaskInit(uint32_t timeoutMs);
bool isDoubleResetDetected();

#endif // DRD_H