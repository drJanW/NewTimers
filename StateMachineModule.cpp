#include "StateMachineModule.h"

StateMachineModule::StateMachineModule()
    : ModuleBase("StateMachineModule"),
      _currentState(IDLE),
      _operationCount(0) {
}

void StateMachineModule::begin() {
    timerManager().registerTimer(&_stateTimer);
    timerManager().registerTimer(&_timeoutTimer);
    
    Serial.print(_name);
    Serial.println(": Module initialized in IDLE state");
}

void StateMachineModule::update() {
    if (!isEnabled()) {
        return;
    }

    // State machine implementation using timers instead of millis()
    switch (_currentState) {
        case IDLE:
            updateIdle();
            break;
        case INITIALIZING:
            updateInitializing();
            break;
        case RUNNING:
            updateRunning();
            break;
        case PAUSED:
            updatePaused();
            break;
        case ERROR:
            updateError();
            break;
    }
}

StateMachineModule::State StateMachineModule::getState() const {
    return _currentState;
}

const char* StateMachineModule::getStateName() const {
    switch (_currentState) {
        case IDLE: return "IDLE";
        case INITIALIZING: return "INITIALIZING";
        case RUNNING: return "RUNNING";
        case PAUSED: return "PAUSED";
        case ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void StateMachineModule::start() {
    if (_currentState == IDLE || _currentState == ERROR) {
        setState(INITIALIZING);
    }
}

void StateMachineModule::pause() {
    if (_currentState == RUNNING) {
        setState(PAUSED);
    }
}

void StateMachineModule::resume() {
    if (_currentState == PAUSED) {
        setState(RUNNING);
    }
}

void StateMachineModule::reset() {
    setState(IDLE);
    _operationCount = 0;
}

void StateMachineModule::setState(State newState) {
    if (_currentState != newState) {
        Serial.print(_name);
        Serial.print(": State transition ");
        Serial.print(getStateName());
        Serial.print(" -> ");
        
        _currentState = newState;
        
        Serial.println(getStateName());
        
        // Reset timers on state change
        _stateTimer.stop();
        _timeoutTimer.stop();
    }
}

void StateMachineModule::updateIdle() {
    // In idle state, waiting for start command
    // No timer needed here
}

void StateMachineModule::updateInitializing() {
    // Start initialization with 2 second timeout
    if (!_timeoutTimer.isRunning()) {
        _timeoutTimer.start(2000);
        Serial.print(_name);
        Serial.println(": Starting initialization (2s timeout)");
    }

    // Use timer-based delay for initialization
    if (timerManager().delay(_stateTimer, 1000)) {
        // Initialization complete
        setState(RUNNING);
        _stateTimer.stop();
    }
    
    // Check for timeout
    if (_timeoutTimer.expired()) {
        Serial.print(_name);
        Serial.println(": Initialization timeout!");
        setState(ERROR);
    }
}

void StateMachineModule::updateRunning() {
    // Perform periodic operations using timer
    if (timerManager().periodic(_stateTimer, 500)) {
        _operationCount++;
        
        Serial.print(_name);
        Serial.print(": Operation #");
        Serial.print(_operationCount);
        Serial.println(" completed");
        
        // After 5 operations, go back to idle
        if (_operationCount >= 5) {
            Serial.print(_name);
            Serial.println(": All operations complete");
            reset();
        }
    }
}

void StateMachineModule::updatePaused() {
    // In paused state, timer is stopped
    // Waiting for resume command
}

void StateMachineModule::updateError() {
    // Auto-recovery after 5 seconds
    if (!_stateTimer.isRunning()) {
        _stateTimer.start(5000);
        Serial.print(_name);
        Serial.println(": Error state - attempting recovery in 5s");
    }
    
    if (_stateTimer.expired()) {
        Serial.print(_name);
        Serial.println(": Recovering from error state");
        reset();
    }
}
