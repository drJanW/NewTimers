#define LOG_LEVEL_ERROR 0
#define LOG_LEVEL_WARN  1
#define LOG_LEVEL_INFO  2
#define LOG_LEVEL_DEBUG 3

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#ifndef LOG_HEARTBEAT
#define LOG_HEARTBEAT 0
#endif

#define LOG_LEVEL_ACTIVE(lvl) ((lvl) <= LOG_LEVEL)

#define LOG_PRINT(level, ...)   do { if (LOG_LEVEL_ACTIVE(level)) Serial.printf(__VA_ARGS__); } while (0)
#define LOG_PRINTLN(level, msg) do { if (LOG_LEVEL_ACTIVE(level)) Serial.println(msg); } while (0)
#define LOG_WRITE(level, ch)    do { if (LOG_LEVEL_ACTIVE(level)) Serial.write(ch); } while (0)

#define LOG_ERROR(...) LOG_PRINT(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_WARN(...)  LOG_PRINT(LOG_LEVEL_WARN,  __VA_ARGS__)
#define LOG_INFO(...)  LOG_PRINT(LOG_LEVEL_INFO,  __VA_ARGS__)
#define LOG_DEBUG(...) LOG_PRINT(LOG_LEVEL_DEBUG, __VA_ARGS__)

#define LOG_ERROR_LN(msg) LOG_PRINTLN(LOG_LEVEL_ERROR, msg)
#define LOG_WARN_LN(msg)  LOG_PRINTLN(LOG_LEVEL_WARN,  msg)
#define LOG_INFO_LN(msg)  LOG_PRINTLN(LOG_LEVEL_INFO,  msg)
#define LOG_DEBUG_LN(msg) LOG_PRINTLN(LOG_LEVEL_DEBUG, msg)

#define PP(x) do { if (LOG_LEVEL_ACTIVE(LOG_LEVEL_INFO)) Serial.print(x); } while (0)
#define PL(x) LOG_INFO_LN(x)
#define PF(...) LOG_INFO(__VA_ARGS__)

static inline void LOG_HEARTBEAT_TICK(char c = '.') {
#if LOG_HEARTBEAT
    Serial.write(c);
#endif
}

/*
#define DBG_TIMED_BLOCK(label, code) { \
    uint32_t __t0 = millis(); \
    code \
    uint32_t __dt = millis() - __t0; \
    if (__dt > 100) LOG_WARN("[SLOW] %s: %lu ms\n", label, __dt); \
}
*/