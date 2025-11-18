# Web Interface REST API

_Last updated: 2025-11-16_

## 1. Conventions
1. **Transport** – All routes are served by the ESP32 on HTTP port 80. There is no authentication layer; the network perimeter must provide access control.
2. **Content types** – Endpoints return either `text/plain` for scalar values (`OK`, numeric strings) or `application/json` for structured payloads. Every JSON response sets `Cache-Control: no-store` to prevent stale data in browsers.
3. **Error codes** –
   - `400` → malformed or missing parameters / validation error.
   - `404` → SD path not found (where applicable).
   - `500` → internal failure (e.g., missing `index.html`, TodayContext init error).
   - `503` → SD card not ready / busy or TodayContext unavailable yet.
4. **SD bus guard** – Handlers that touch the SD card acquire the shared `SDBusyGuard` before opening files to ensure we never exceed the ESP32 FATFS limit of five concurrent file descriptors.

## 2. Static assets
1. `GET /` – streams `index.html` from SD. Returns `500` if SD unavailable or file missing.
2. `GET /styles.css`, `GET /kwal.js` – served via `AsyncWebServer::serveStatic` directly off SD. Update the files on SD and bump the `?v=` cache-buster query in `index.html` to deploy new UI bundles.

## 3. Brightness & audio controls
| # | Method & Path | Query/body | Response | Notes |
| --- | --- | --- | --- | --- |
| 1 | `GET /setBrightness` | `value=0..255` (int) | `"OK"` text | Clamped to `[0,255]`, forwards to `ConductManager::intentSetBrightness`.
| 2 | `GET /getBrightness` | none | numeric string such as `"128"` | Derived from `getWebBrightness()*255`.
| 3 | `GET /setWebAudioLevel` | `value=0.0..1.0` (float) | `"OK"` text | Calls `ConductManager::intentSetAudioLevel`.
| 4 | `GET /getWebAudioLevel` | none | stringified float with two decimals (e.g., `"0.42"`). | Reads `AudioManager::getWebLevel()`.

## 4. OTA helpers
| # | Method & Path | Purpose | Response |
| --- | --- | --- | --- |
| 1 | `GET /ota/arm` | Arms OTA window for 300 s. | `"OK"` text. |
| 2 | `POST /ota/confirm` | Confirms an already armed session. | `200` + guidance text or `400 EXPIRED`. |
| 3 | `POST /ota/start` | Convenience combo: arms + confirms immediately. | `200` success text or `500` on failure. |

## 5. Today context snapshot
- `GET /api/context/today`
  - **Success payload**:
    ```json
    {
      "valid": true,
      "date_iso": "2025-11-16",
      "calendar_entry": true,
      "note": "Optional calendar note",
      "pattern": {
        "id": "12",
        "label": "Evening Calm",
        "calendar_id": 42,
        "source": "calendar"
      },
      "color": {
        "id": "7",
        "label": "Warm Sunset",
        "rgb1_hex": "#FF7F00",
        "rgb2_hex": "#552200",
        "calendar_id": 101,
        "source": "calendar"
      }
    }
    ```
  - `503` if no valid context cached yet; `500` if `InitTodayContext` fails.

## 6. Pattern & color collections
1. `GET /api/patterns` → JSON array of patterns (mirrors `ColorsStore::buildPatternsJson`). Includes header `X-Pattern: <activeId>`.
2. `POST /api/patterns` → body must match `ColorsStore::updatePattern` schema (id, label, params). Returns the refreshed list plus `X-Pattern` header pointing at the affected/active pattern.
3. `POST /api/patterns/select` → `{ "id": "<patternId>" }` or `id` query param. Sets active pattern; response is the same JSON list.
4. `POST /api/patterns/delete` → `{ "id": "<patternId>" }`. Removes and returns remaining list.
5. `POST /api/patterns/preview` → transient preview (not persisted) with up to 2 KB JSON (params plus optional label). Returns `{ "status": "ok" }` on success.
6. Color endpoints mirror the pattern routes: replace `/api/patterns` with `/api/colors`, `X-Pattern` header with `X-Color`, and payload schema with the color store contract (`rgb1_hex`, `rgb2_hex`, etc.).

_All payloads are validated server-side; failures return `400` plus a plain-text reason._

## 7. SD maintenance endpoints
| # | Method & Path | Payload | Success response | Error cases |
| --- | --- | --- | --- | --- |
| 1 | `GET /api/sd/status` | none | `{"ready":true,"busy":false,"hasIndex":true}` | `503` if SD not ready. |
| 2 | `GET /api/sd/list` | `path` query (optional, defaults to `/`) | Directory listing JSON:<br>`{"path":"/","parent":"/","ready":true,"busy":false,"entryCount":12,"truncated":false,"entries":[{"name":"index.html","type":"file","size":4096},…]}` | `400` invalid path, `404` missing directory, `503` busy. |
| 3 | `POST /api/sd/upload` | Multipart form: `path` (dir) + file | `{"status":"ok","path":"/uploaded/file.txt"}` | Streaming handler enforces SD bus guard; returns `400` on validation failure. |
| 4 | `POST /api/sd/delete` | JSON `{ "path": "/foo/bar" }` | `{"status":"ok"}` | `400` invalid/missing path, `503` busy. |

## 8. Voting and auxiliary routes
1. `POST /api/sd/delete` – documented above; attached here for completeness.
2. `SDVoting::attachVoteRoute(server)` currently exposes `/api/vote` (see `lib/SDManager20251111/SDVoting.*`) for fragment scoring. Payload: `{ "dir": <int>, "file": <int>, "delta": <int> }`.
3. Additional static assets (`/sdroot/*`) can be listed or uploaded through the SD endpoints; no extra routes exist today.

## 9. Change checklist
1. Update this document whenever a route, payload field, or status code changes.
2. When altering front-end code that calls these APIs, double-check both the new UI logic and this reference so they stay in sync.
3. Before flashing firmware, verify SD availability (shared `SDBusyGuard` in place) so the handler count stays below the ESP32 FATFS 5-handle limit.
