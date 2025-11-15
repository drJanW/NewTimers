#include "HeartbeatBoot.h"

#include "Globals.h"

HeartbeatBoot heartbeatBoot;

void HeartbeatBoot::plan() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    PL("[Conduct][Plan] Heartbeat LED initialized");
}
