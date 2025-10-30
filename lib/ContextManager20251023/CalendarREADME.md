## Calendar concept (2025-10-30)

1. The context manager reads calendar metadata from CSV files stored in `sdroot/` on boot and then once per hour via `cb_loadCalendar()`. If the clock is not seeded yet (no valid date), the loader retries after one minute until time is available. The files are never modified on-device; they are maintained offline (Excel generator to follow).
2. `cb_loadCalendar()` scans only for today’s entry, caches the matching data, and fires the actions on every pass. Duplicate triggers within the same day are acceptable for now.
3. No dedicated `cb_calendarDaily()` timer remains. The hourly loader handles both detection and triggering, offering a safer recovery path if the SD content changes or the device reboots mid-day.

### CSV layout

1. `calendar.csv` columns: `year;month;day;tts_sentence;tts_interval_min;theme_box_id;light_show_id;color_range_id;note`. The `tts_sentence` field contains the literal text string to feed into TTS. Only the first matching row for today is used; overlapping dates (e.g. birthday on Christmas) accept this limitation.
2. `theme_boxes.csv` columns: `theme_box_id;entries;note`. `entries` is a comma-separated list of MP3 subdirectory numbers. Existing weighted-random logic remains the only selection policy, so no extra weight columns are required.
3. `light_shows.csv` columns mirror `LightShowParams`: `light_show_id;rgb1_hex;rgb2_hex;color_cycle_sec;bright_cycle_sec;fade_width;min_brightness;gradient_speed;center_x;center_y;radius;window_width;radius_osc;x_amp;y_amp;x_cycle_sec;y_cycle_sec;note`. These placeholders already use the real parameter set so LightManager can apply them directly when the implementation lands.
4. `color_ranges.csv` columns: `color_range_id;start_hex;end_hex;note`. These values align with the color pairings referenced by the light show entries.

### Runtime flow

1. Boot sequence schedules `cb_loadCalendar()` immediately; success re-arms the hourly cadence, while missing clock data causes one-minute retries until the date resolves.
2. When the callback finds today’s row, it caches the parsed calendar data and resolves referenced IDs by scanning their respective CSVs for a single matching entry.
3. The callback then cues the TTS sentence with the specified interval and forwards the resolved theme box, light show, and color range identifiers to the appropriate managers. Actual light-show rendering remains to be implemented; current focus is on defining the data contract.

### Maintenance notes

1. Past calendar rows can optionally be removed offline to keep files short, but the loader stops scanning as soon as it passes today’s date, so history entries cause no runtime penalty.
2. Keep all CSV files strictly ASCII and semicolon-separated to align with the existing SD parsing utilities.
3. Any future extensions should preserve the current column order to avoid breaking the offline tooling.