#pragma once
typedef struct { int x; } mutex_t;
inline void mutex_enter_blocking(mutex_t*){}
inline void mutex_exit(mutex_t*){}
