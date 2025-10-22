#include "ModuleBase.h"

ModuleBase::ModuleBase(const char* name) 
    : _name(name), _enabled(true) {
}

ModuleBase::~ModuleBase() {
}

void ModuleBase::begin() {
    // Default implementation - override in derived classes
}

const char* ModuleBase::getName() const {
    return _name;
}

bool ModuleBase::isEnabled() const {
    return _enabled;
}

void ModuleBase::enable() {
    _enabled = true;
}

void ModuleBase::disable() {
    _enabled = false;
}

TimerManager& ModuleBase::timerManager() {
    return TimerManager::getInstance();
}
