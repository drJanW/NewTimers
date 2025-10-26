#define DEBUG_OUTPUT 1

#if DEBUG_OUTPUT
  #define PP(x)      Serial.print(x)
  #define PL(x)      Serial.println(x)
  #define PF(...)    Serial.printf(__VA_ARGS__)
#else
  #define PP(x)
  #define PL(x)
  #define PF(...)
#endif
/*
#define DBG_TIMED_BLOCK(label, code) { \
    uint32_t __t0 = millis(); \
    code \
    uint32_t __dt = millis() - __t0; \
    if (__dt > 100) PF("[SLOW] %s: %lu ms\n", label, __dt); \
}
*/