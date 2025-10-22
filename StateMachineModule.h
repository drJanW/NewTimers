#ifndef STATE_MACHINE_MODULE_H
#define STATE_MACHINE_MODULE_H

#include "ModuleBase.h"
#include "Timer.h"

/**
 * @brief Example state machine module using timer-based architecture
 * 
 * Demonstrates how to implement state machines without local timing variables.
 * All state transitions and timeouts are managed through TimerManager.
 */
class StateMachineModule : public ModuleBase {
public:
    /**
     * @brief Module states
     */
    enum State {
        IDLE,
        INITIALIZING,
        RUNNING,
        PAUSED,
        ERROR
    };

    /**
     * @brief Constructor
     */
    StateMachineModule();

    /**
     * @brief Initialize the module
     */
    void begin() override;

    /**
     * @brief Update the module (called in main loop)
     */
    void update() override;

    /**
     * @brief Get current state
     * @return Current state
     */
    State getState() const;

    /**
     * @brief Get state name as string
     * @return State name
     */
    const char* getStateName() const;

    /**
     * @brief Start the state machine
     */
    void start();

    /**
     * @brief Pause the state machine
     */
    void pause();

    /**
     * @brief Resume from pause
     */
    void resume();

    /**
     * @brief Reset to idle state
     */
    void reset();

private:
    State _currentState;
    Timer _stateTimer;        // Timer for state transitions
    Timer _timeoutTimer;      // Timeout for operations
    int _operationCount;

    /**
     * @brief Transition to a new state
     * @param newState State to transition to
     */
    void setState(State newState);

    /**
     * @brief Update idle state
     */
    void updateIdle();

    /**
     * @brief Update initializing state
     */
    void updateInitializing();

    /**
     * @brief Update running state
     */
    void updateRunning();

    /**
     * @brief Update paused state
     */
    void updatePaused();

    /**
     * @brief Update error state
     */
    void updateError();
};

#endif // STATE_MACHINE_MODULE_H
