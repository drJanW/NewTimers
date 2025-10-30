#include "HeartbeatBoot.h"

#include "Globals.h"

HeartbeatBoot heartbeatBoot;

void HeartbeatBoot::plan() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    setLedStatus(false);
    PL("[Conduct][Plan] Heartbeat LED initialized");
}
