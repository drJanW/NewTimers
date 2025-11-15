# Dynamic Light Backbone Plan

## Goals
1. Ensure the dome keeps glowing even when the calendar CSV does not list a show for today (the common case).
2. Generate all light parameters dynamically from context rather than static lookup tables or theme IDs.
3. Provide a straight path for upgrading the pseudo sensor inputs to real hardware data when it arrives.

## Trigger Point
1. `CalendarConduct::cb_loadCalendar` (or `CalendarPolicy::evaluate`) detects that today has no calendar row and flips into a **DefaultProfile** branch instead of exiting early.
2. The DefaultProfile produces `LightShowParams` and optional palette hints which can be handed straight to `PlayLightShow`, preserving the existing logging/timer flow.
3. If the calendar does have an entry, normal processing continues unchanged.

## Dynamic Parameter Rules
1. **Date-driven base:** Map month, day-of-week, or season buckets straight into colour temperature, palette spread, and motion cadence using deterministic formulas (no tables).
2. **Pseudo environment:** Until real sensors land, feed the formulas with stable pseudo readings (e.g. seeded randoms or heuristics for temperature, humidity, ambient light).
3. **Adjustments:**
   - Cooler pseudo temperature → shift palette towards blues/cyans.
   - Higher pseudo humidity → increase fade width to soften transitions.
   - Lower ambient light → raise minimum brightness, within `MAX_BRIGHTNESS`.
4. Package these computed values into the existing `LightShowParams` structure together with any palette selection hints.

## Sensor Upgrade Path
1. Keep the pseudo generators isolated behind lightweight helpers (e.g. `DefaultEnvironment::temperatureC()`); they will be replaced with real sensor reads later.
2. When hardware values become available, drop them into the same helpers without touching the fallback logic or calendar flow.
3. Recalibrate the formulas (colour/brightness mapping) using real-world testing, but keep the deterministic pipeline intact.

## TODO
1. Implement the backbone branch and deterministic parameter formulas inside the Conduct/Policy layer.
2. Add pseudo environment helpers and ensure outputs remain stable across reboots unless intentionally re-seeded.
3. Document the mapping formulas once finalised, so future tuning sessions have a reference point.
