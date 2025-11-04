# SD Card Snapshot — 2025-11-04

1. **Origin:** Dumped from the physical SD card (label "git_sucks") immediately after removal on 2025-11-04.
2. **Firmware expectation:** Web UI reports `kwal versie 110308D` with `APP_BUILD_INFO.version = "web-split-110308D"`.
3. **Contents:** Includes modular web bundle (`index.html`, `kwal.js`, `light.js`, `audio.js`, `patterns.js`, `colors.js`, `sd.js`), JSON stores, CSV schedules, `ledmap.bin`, `ping.wav`, and diagnostic files (`SDkaart-analyse.txt`, `last_sync.txt`).
4. **Checksums:** Generated SHA-256 hashes are stored in `checksums.sha256` in the same directory.
5. **Restore procedure:** Copy the entire directory tree onto a freshly formatted SD card, preserving case-sensitive filenames and directory structure.
