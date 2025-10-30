
#include <Arduino.h>

#pragma once
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


template <typename T,
          typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
inline T map(T x, T xmin, T xmax, T ymin, T ymax) {
    if (xmin == xmax) {
        return ymin;
    }
    const T t = (x - xmin) / (xmax - xmin);
    return ymin + t * (ymax - ymin);
}

template <typename T,
          typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
inline T clamp(T x, T xmin, T xmax) {
    if (xmin > xmax) {
        const T tmp = xmin;
        xmin = xmax;
        xmax = tmp;
    }

    if (x < xmin) {
        return xmin;
    }
    if (x > xmax) {
        return xmax;
    }
    return x;
}


// === Existing Globals ===

#define LOOPCYCLE 10
#define SECONDS_TICK 1000
#define MAX_LOOP_SILENCE 5000 // max 5 sec blocking
void bootRandomSeed();
