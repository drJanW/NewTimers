# Time-of-Day Context Plan

_Last updated: 2025-11-16_

## 1. Scope
1. Capture a reliable boot-time timestamp even when Wi-Fi is unavailable, so other subsystems can fall back to a recent, persisted clock value.
2. Build the infrastructure to evaluate named time-of-day periods (night, dawn, morning, etc.) and drive policy changes for brightness, audio, and theme boxes using TimerManager exclusively.

## 2. Key Assumptions (KEEP unless noted)
1. **Clock hierarchy**: `PRTClock` remains the single source of truth once Wi-Fi or RTC provides a valid date; when neither is available we fall back to the last persisted timestamp or, as a last resort, seed to 04:00.
2. **Sunrise/Sunset source**: Values come from FetchManager; if the fetch fails we reuse the previously cached values.
3. **Static anchors**: `startday = 07:00`, `endday = 18:00`, `endevening = 23:00` stay constants (future configurability out of scope for Milestone 0).
4. **Update interval**: Period evaluation only needs to run every 10 minutes; there is no requirement for event-per-boundary precision.
5. **Data exposure**: TodayContext remains the authoritative snapshot that other modules read. Additional APIs are optional and will only be added if TodayContext proves insufficient.
6. **Logging**: Default is silent. Any temporary tracing must be guarded behind a local `CONTEXT_PERIOD_DEBUG` (or similar) macro so production builds remain log-free, and modules that should stay quiet can now set `LOCAL_LOG_LEVEL` (e.g., `LOG_LEVEL_NONE`) before including `Globals.h` to mute their own `PF/PL/PP` traffic.

## 3. Milestones
### Milestone 0 – Boot Timestamp Persistence (current focus)
1. Document the write trigger (post-clock-valid or midnight) and confirm the storage medium (SD file, e.g., `sdroot/system_state.csv`).
2. Define the record format (single-line CSV: `last_boot_iso,last_boot_epoch`) and retention policy (overwrite the single row; no history for now).
3. Implement a TimerManager callback that writes the timestamp once the clock is valid; guard writes with `SDBusyGuard` and handle failures via a retry timer.
4. Provide a small verification helper (e.g., `tools/print_boot_timestamp.py`) and update the README with inspection steps.

### Milestone 1 – Period Policy CSV
1. Introduce `sdroot/time_period_policies.csv` describing each period (`isNight`, `isDawn`, …) plus desired brightness/audio/theme overrides.
2. Build a loader inside ContextManager (or a helper) that reads the CSV, applies defaults when missing, and surfaces validation errors.
3. Document the CSV format so Jan can edit it without recompiling.

### Milestone 2 – Daily Boundary Computation
1. Compute derived timestamps (startmorning, endafternoon, etc.) at midnight (or immediately when the clock becomes valid) using sunrise/sunset plus the constants above.
2. Cache the computed structure so the 10-minute evaluator can reference it without re-reading SD.
3. Ensure the cache gracefully survives missing sun-data (fall back to defaults from Milestone 0 timestamp).

### Milestone 3 – Period Evaluation Timer
1. Add a TimerManager task (600,000 ms) that compares the current time to the cached boundaries, determines active periods, and detects transitions.
2. On changes, update TodayContext with the new period flags and notify ConductManager so policies can react.
3. Gate any diagnostics behind `CONTEXT_PERIOD_DEBUG`.

### Milestone 4 – Policy Application
1. Map the active period flags to brightness/audio/theme adjustments (stacking allowed) using the CSV-defined values.
2. Integrate with LightPolicy and AudioPolicy so the new targets are enforced automatically.
3. Update documentation (README and CSV description) and add a regression checklist so we can verify behavior quickly.

## 4. Milestone 0 – Next Actions

### 4.1 Storage helpers + concurrency (inspection ✅)
- `FetchManager` already persists `/last_sync.txt` via `SDManager::writeTextFile` once an NTP result succeeds, proving the SD stack can handle single-line text snapshots without extra buffering.
- Multiple modules (e.g., `ColorsStore`, `PatternStore`) define a local `SDBusyGuard` wrapper that simply checks `SDManager::isBusy()` and toggles it for the lifetime of the guard. We will promote that helper to `lib/Common/SDBusyGuard.h` so the new timestamp writer can reuse the same pattern instead of copy/pasting yet another anonymous class.
- Decision: store the boot timestamp in a dedicated CSV at `sdroot/system_state.csv` (SD path string `"/system_state.csv"`). File will sit beside `SDversion.txt` and is small enough (<128 bytes) that we can overwrite it atomically with a single `String` write.
- Failure handling: if the guard cannot acquire the SD bus (rare, SD already busy) we retry through TimerManager (see §4.2). If `writeTextFile` itself fails, we emit a one-time `PL` and enqueue another retry instead of blocking BootMaster.

### 4.2 Write trigger + retry timeline (decision ✅)
| Phase | Trigger | Action | Notes |
| --- | --- | --- | --- |
| A. Post-clock-valid | `BootMaster::bootstrapTick` detects `PRTClock::isTimeFetched()` becoming true. We enqueue a one-shot TimerManager task (run after ~2 seconds) that calls `persistBootTimestamp("clock-valid")`. | We intentionally decouple the write from the boot-time callback so BootMaster stays lean; TimerManager already runs in the main loop so SD writes happen outside interrupt-ish contexts. |
| B. Midnight maintenance | Reuse the existing Conduct midnight tick (or add a `TimerManager` repeat set to 24h, scheduled off `PRTClock::secondsUntilMidnight()`) so we always refresh the CSV once per day even if the device never reboots. | Midnight task will stamp `persistBootTimestamp("midnight-roll")` so other subsystems know the record is recent even without a reboot. |
| Retry | Any write failure reuses the same callback but reschedules itself with an exponential backoff capped at 60s. All retries reset once a write succeeds. | Ensures SD hiccups or temporary BUSY collisions do not leave stale data. |

### 4.3 CSV prototype + parsing notes (decision ✅)
```
path: /system_state.csv
schema version: 1 (implicit, add header comment when we rev)
columns: last_boot_iso,last_boot_epoch_ms,source,notes
example line: 2025-11-16T07:34:12Z,1763278452000,clock-valid,"synced via Wi-Fi"
```
- `last_boot_iso`: fixed 24-char ISO8601 (`YYYY-MM-DDThh:mm:ssZ`). Always UTC. We leverage `PRTClock::toIso8601()` helper (if missing, add a small formatter near the persistence code).
- `last_boot_epoch_ms`: 64-bit epoch in milliseconds so we can quickly compute deltas without parsing strings. When RTC lacks millis precision we multiply seconds by 1000.
- `source`: short token describing why we wrote the row (`clock-valid`, `midnight-roll`, `manual`). Useful for debugging repeated writes.
- `notes`: optional text (free form). Empty by default, but we can stash hints like `fallback-cache` if we persisted after seeding from `/last_sync.txt`.
- Parsing path: we will use `CsvUtils::splitLine` to break the row into exactly four cells, then (a) prefer `last_boot_epoch_ms` when valid, (b) fall back to parsing `last_boot_iso` via `CsvUtils::parseDateTime` (or a bespoke helper) if epoch fails. Any malformed file triggers an on-boot warning and the system simply deletes `system_state.csv` so we can recreate it.
- Retention policy remains one row: we always rewrite the file with a single line so no compaction logic is needed.

### 4.4 Verification helper + README updates (queued ✅)
- Helper: `tools/print_boot_timestamp.py` will accept an optional `--path` (default `../sdroot/system_state.csv`), read the CSV, and emit a human-friendly summary plus exit codes (`0` = ok, `1` = file missing, `2` = parse error). Implementation sketch:
	1. Use Python’s `csv` module to read the first non-comment row.
	2. Parse the ISO string with `datetime.fromisoformat` (force `tzinfo=timezone.utc`).
	3. Compare the epoch column against the parsed ISO to highlight skew >2s.
	4. Print a short table (`ISO / epoch ms / age / source / notes`).
- README: add a “Boot timestamp snapshot” subsection under `README_PORT.md` explaining (a) how SD cards receive the file, (b) how to run the helper script, and (c) how to fall back manually (delete the file and reboot). Include a reminder that `sdroot/system_state.csv` must be writable and ships empty.

*No code will be written until the documentation items above are checked in; next step is to implement the persistence callback per this plan.*
