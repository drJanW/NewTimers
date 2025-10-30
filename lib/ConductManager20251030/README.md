# Conduct Manager Architecture Notes

This module wires together end-to-end input→output chains. To keep responsibilities clear:

- **Sensor side** modules (e.g. `SensorManager`, `SensorsPolicy`) acquire and normalise raw values only. They must not decide how the system reacts.
- **Conduct** modules schedule work and pass normalised signals onward. They coordinate modules but avoid embedding behavioural rules.
- **Output policies** (e.g. `HeartbeatPolicy`) own the response logic for a specific actuator. They translate high-level cues into concrete actions such as LED cadence or brightness profiles.

For any new chain:

1. Capture raw input in the appropriate manager.
2. Normalise or filter it in the matching input policy.
3. Route the processed value through the conduct coordinator.
4. Apply the behavioural rules inside a dedicated output policy that drives the actual hardware.

Following this flow keeps input processing, orchestration, and output behaviour isolated, so we can iterate on the “feel” of an output without touching sensor plumbing or timer wiring.
