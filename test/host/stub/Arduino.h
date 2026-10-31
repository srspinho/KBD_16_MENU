#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>
extern uint32_t fake_ms;
inline uint32_t millis() { return fake_ms; }
inline uint32_t micros() { return fake_ms * 1000u; }
inline void delay(uint32_t) {}
using std::min; using std::max;
#define constrain(x,a,b) ((x)<(a)?(a):((x)>(b)?(b):(x)))
#define OUTPUT 1
inline void pinMode(int,int){}
inline void digitalWrite(int,int){}
struct FakeSerial { template<class...A> void printf(A...){} };
extern FakeSerial Serial;
