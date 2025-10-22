now: 20250916

contains "all" parameters used by more than one module or file
Globals.cpp contains important macros:

// Overloads voor atomics; lock-free loads/stores.
template <typename T>
inline void setMux(T value, std::atomic<T>* ptr) {
    ptr->store(value, std::memory_order_relaxed);
}
template <typename T>
inline T getMux(const std::atomic<T>* ptr) {
    return ptr->load(std::memory_order_relaxed);
}

implementations like:

// --- Year ---
static std::atomic<uint8_t> _valYear{0};
void     setYear(uint8_t v) { setMux(v, &_valYear); }
uint8_t  getYear()          { return getMux(&_valYear); }

are collected in Globals.cpp, but most should be refactored 

Also included in Globals are macros.inc and HWconfig (all hardware-related settings);
adc_port is only to initialize randomization, needs be refactored out
