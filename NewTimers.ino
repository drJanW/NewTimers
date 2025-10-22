/**
 * NewTimers - Refactored timer-based architecture
 * 
 * This sketch demonstrates a layered managed module architecture
 * where timing operations (millis(), now, attempts, delayMs) are
 * handled through software timers in TimerManager instead of
 * being used locally in modules.
 * 
 * Architecture features:
 * - TimerManager: Centralized timer management
 * - Timer: Individual software timer instances
 * - ModuleBase: Base class for all managed modules
 * - Example modules: BlinkModule, RetryModule
 */

#include "TimerManager.h"
#include "BlinkModule.h"
#include "RetryModule.h"

// Module instances
BlinkModule ledBlink(LED_BUILTIN, 500);  // Blink built-in LED every 500ms
RetryModule retryExample(5, 2000);       // Max 5 attempts, 2 second retry delay

// Timer for periodic status reporting
Timer statusTimer;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000); // Wait for Serial, but not forever
  
  Serial.println("=== NewTimers - Timer-Managed Architecture Demo ===");
  Serial.println();
  Serial.println("This demo showcases:");
  Serial.println("- Centralized timing through TimerManager");
  Serial.println("- No direct millis() calls in modules");
  Serial.println("- Layered managed module architecture");
  Serial.println();

  // Initialize timer manager
  TimerManager& tm = TimerManager::getInstance();
  
  // Initialize modules
  ledBlink.begin();
  retryExample.begin();
  
  // Register status timer
  tm.registerTimer(&statusTimer);
  statusTimer.start(10000); // Status every 10 seconds
  
  Serial.println("Setup complete. Modules initialized.");
  Serial.println();
  
  // Start retry operation after 3 seconds
  Timer startDelay;
  tm.registerTimer(&startDelay);
  startDelay.start(3000);
  
  // Wait for start delay using timer (not delay())
  while (!startDelay.expired()) {
    tm.update();
  }
  
  Serial.println("Starting retry operation...");
  retryExample.startOperation();
}

void loop() {
  TimerManager& tm = TimerManager::getInstance();
  
  // Update timer manager
  tm.update();
  
  // Update all modules
  ledBlink.update();
  retryExample.update();
  
  // Periodic status reporting using timer
  if (statusTimer.expired()) {
    statusTimer.reset();
    
    Serial.println();
    Serial.println("--- Status Report ---");
    Serial.print("Uptime: ");
    Serial.print(tm.now() / 1000);
    Serial.println(" seconds");
    Serial.print("LED Blinks: ");
    Serial.println(ledBlink.getBlinkCount());
    
    if (retryExample.isComplete()) {
      Serial.print("Retry operation: ");
      Serial.println(retryExample.wasSuccessful() ? "SUCCESS" : "FAILED");
      Serial.print("Attempts used: ");
      Serial.println(retryExample.getCurrentAttempt());
      
      // Start another operation
      Serial.println("Starting new retry operation...");
      retryExample.startOperation();
    } else {
      Serial.print("Retry operation in progress (attempt ");
      Serial.print(retryExample.getCurrentAttempt());
      Serial.println(")");
    }
    Serial.println("---------------------");
    Serial.println();
  }
}
