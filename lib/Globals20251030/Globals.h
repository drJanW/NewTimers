
#pragma once

#include <Arduino.h>
#include "MathUtils.h"
#include "macros.inc"
#include "HWconfig.h"
#include <atomic>
#include <type_traits>

#if __cplusplus >= 201703L
  #define MYKWAL_TRIV_COPY(T) std::is_trivially_copyable_v<T>
#else
  #define MYKWAL_TRIV_COPY(T) std::is_trivially_copyable<T>::value
#endif

template <typename T>
inline void setMux(T value, std::atomic<T>* ptr) {
    static_assert(MYKWAL_TRIV_COPY(T), "T moet trivially copyable zijn");
    ptr->store(value, std::memory_order_relaxed);
}

template <typename T>
inline T getMux(const std::atomic<T>* ptr) {
    static_assert(MYKWAL_TRIV_COPY(T), "T moet trivially copyable zijn");
    return ptr->load(std::memory_order_relaxed);
}

using MathUtils::clamp;
using MathUtils::map;


// === Existing Globals ===

#define LOOPCYCLE 10
#define SECONDS_TICK 1000
#define MAX_LOOP_SILENCE 5000 // max 5 sec blocking
void bootRandomSeed();
