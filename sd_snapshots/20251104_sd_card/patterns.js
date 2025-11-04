(function () {
    'use strict';

    const app = window.kwalApp;
    if (!app) {
        throw new Error('[patterns] kwalApp is not initialised');
    }

    const { utils, state } = app;
    const light = app.light;

    const PATTERN_CONTEXT = '__context__';
    const MAX_LIGHT_PATTERNS = 100;

    const patternSettingConfig = [
        { key: 'color_cycle_sec', label: 'Kleurcyclus (s)', min: 1, max: 120, step: 1 },
        { key: 'bright_cycle_sec', label: 'Helderheidscyclus (s)', min: 1, max: 120, step: 1 },
        { key: 'fade_width', label: 'Fadebreedte', min: 0, max: 40, step: 0.1 },
        { key: 'min_brightness', label: 'Minimum helderheid', min: 0, max: 255, step: 1 },
        { key: 'gradient_speed', label: 'Gradient snelheid', min: 0, max: 1, step: 0.01 },
        { key: 'center_x', label: 'Centrum X', min: -32, max: 32, step: 0.5 },
        { key: 'center_y', label: 'Centrum Y', min: -32, max: 32, step: 0.5 },
        { key: 'radius', label: 'Radius', min: 0, max: 64, step: 0.5 },
        { key: 'window_width', label: 'Vensterbreedte', min: 0, max: 64, step: 0.5 },
        { key: 'radius_osc', label: 'Radiusoscillatie', min: 0, max: 16, step: 0.5 },
        { key: 'x_amp', label: 'X amplitude', min: 0, max: 16, step: 0.5 },
        { key: 'y_amp', label: 'Y amplitude', min: 0, max: 16, step: 0.5 },
        { key: 'x_cycle_sec', label: 'X cyclus (s)', min: 1, max: 120, step: 1 },
        { key: 'y_cycle_sec', label: 'Y cyclus (s)', min: 1, max: 120, step: 1 }
    ];

    const patternSettingMap = new Map(patternSettingConfig.map((cfg) => [cfg.key, cfg]));

    const patternState = utils.createCollectionState('Patroon geladen');
    state.pattern = patternState;

    let lightRangeInputs = [];

    const moduleApi = {
        PATTERN_CONTEXT,
        MAX_LIGHT_PATTERNS,
        patternSettingConfig
    };

    const normalizePattern = (source) => {
        const base = {
            params: {
                color_cycle_sec: 18,
                bright_cycle_sec: 14,
                fade_width: 9,
                min_brightness: 12,
                gradient_speed: 0.6,
                center_x: 0,
                center_y: 0,
                radius: 22,
                window_width: 24,
                radius_osc: 4,
                x_amp: 3.5,
                y_amp: 2.5,
                x_cycle_sec: 22,
                y_cycle_sec: 20
            }
        };
        if (!source || typeof source !== 'object') {
            return base;
        }
        patternSettingConfig.forEach((cfg) => {
            const raw = Number(source.params && source.params[cfg.key]);
            if (Number.isFinite(raw)) {
                base.params[cfg.key] = utils.clamp(raw, cfg.min, cfg.max);
            }
        });
        return base;
    };

    const buildPatternControls = () => {
        const list = app.dom.lightSettingsList;
        if (!list) {
            return;
        }
        list.innerHTML = '';
        const fragment = document.createDocumentFragment();
        patternSettingConfig.forEach((cfg) => {
            const item = document.createElement('div');
            item.className = 'settings-item';
            item.dataset.key = cfg.key;
            item.innerHTML = `
                <div class="settings-item-header">
                    <span>${cfg.label}</span>
                    <span class="settings-value" data-value="${cfg.key}">0</span>
                </div>
                <div class="range-wrapper">
                    <div class="range-marker" data-range-marker="${cfg.key}"></div>
                    <input type="range" class="settings-range" data-setting-range="${cfg.key}" min="${cfg.min}" max="${cfg.max}" step="${cfg.step}">
                </div>
            `;
            fragment.appendChild(item);
        });
        list.appendChild(fragment);
        lightRangeInputs = Array.from(list.querySelectorAll('[data-setting-range]'));
    };

    const formatSettingValue = (key, value) => {
        const cfg = patternSettingMap.get(key);
        if (!cfg) {
            return String(value);
        }
        if (cfg.step >= 1) {
            return String(Math.round(value));
        }
        if (cfg.step >= 0.1) {
            return value.toFixed(1);
        }
        return value.toFixed(2);
    };

    const updateLightSettingDisplay = (key, value) => {
        const list = app.dom.lightSettingsList;
        if (!list) {
            return;
        }
        const label = list.querySelector(`[data-value="${key}"]`);
        const input = list.querySelector(`[data-setting-range="${key}"]`);
        if (input) {
            input.value = String(value);
        }
        if (label) {
            label.textContent = formatSettingValue(key, value);
        }
    };

    const setRangeMarkerPosition = (key, value) => {
        const list = app.dom.lightSettingsList;
        if (!list) {
            return;
        }
        const marker = list.querySelector(`[data-range-marker="${key}"]`);
        const cfg = patternSettingMap.get(key);
        if (!marker || !cfg) {
            return;
        }
        if (!Number.isFinite(value)) {
            marker.classList.add('range-marker-hidden');
            return;
        }
        const numericValue = utils.clamp(value, cfg.min, cfg.max);
        const span = cfg.max - cfg.min;
        const ratio = span <= 0 ? 0 : (numericValue - cfg.min) / span;
        const clamped = Math.max(0, Math.min(1, ratio));
        marker.style.left = `${(clamped * 100).toFixed(2)}%`;
        marker.classList.remove('range-marker-hidden');
    };

    const renderActiveMarkers = () => {
        patternSettingConfig.forEach((cfg) => {
            const snapshot = patternState.activeSnapshot;
            const value = snapshot ? snapshot.params[cfg.key] : Number.NaN;
            setRangeMarkerPosition(cfg.key, value);
        });
    };

    const syncActiveSnapshot = () => {
        if (patternState.activeId && patternState.activeId !== PATTERN_CONTEXT) {
            const activePattern = patternState.map.get(patternState.activeId);
            patternState.activeSnapshot = activePattern ? normalizePattern(activePattern) : null;
        } else {
            patternState.activeSnapshot = null;
        }
        renderActiveMarkers();
    };

    const applyPatternDraftToUI = (draft) => {
        if (!draft) {
            return;
        }
        patternSettingConfig.forEach((cfg) => {
            updateLightSettingDisplay(cfg.key, draft.params[cfg.key]);
        });
        patternState.dirty = false;
    };

    const refreshPatternControls = () => {
        const select = app.dom.patternSelect;
        if (!select) {
            return;
        }
        const currentId = patternState.selectedId || PATTERN_CONTEXT;
        const activeId = patternState.activeId || '';
        select.innerHTML = '';

        const contextOption = document.createElement('option');
        contextOption.value = PATTERN_CONTEXT;
        contextOption.textContent = activeId ? 'Context (automatisch)' : 'Context (automatisch) •';
        contextOption.selected = currentId === PATTERN_CONTEXT;
        select.appendChild(contextOption);

        patternState.items.forEach((pattern) => {
            if (!pattern || !pattern.id) {
                return;
            }
            const option = document.createElement('option');
            option.value = pattern.id;
            const label = pattern.label && pattern.label.trim() ? pattern.label : pattern.id;
            option.textContent = pattern.id === activeId ? `${label} •` : label;
            option.selected = pattern.id === currentId;
            select.appendChild(option);
        });

        if (select.selectedIndex < 0 && select.options.length > 0) {
            select.options[0].selected = true;
        }

        const isContext = currentId === PATTERN_CONTEXT;
        const hasPattern = !isContext && patternState.map.has(currentId);

        if (app.dom.patternName) {
            app.dom.patternName.disabled = isContext;
            if (!patternState.labelDirty) {
                const item = patternState.map.get(currentId);
                const label = item && item.label && item.label.trim() ? item.label : item ? item.id : '';
                app.dom.patternName.value = isContext ? '' : (label || '');
            }
        }
        if (app.dom.patternSave) {
            app.dom.patternSave.disabled = isContext || (!patternState.dirty && !patternState.labelDirty);
        }
        if (app.dom.patternSaveAs) {
            app.dom.patternSaveAs.disabled = patternState.items.length >= MAX_LIGHT_PATTERNS;
        }
        if (app.dom.patternDelete) {
            app.dom.patternDelete.disabled = isContext || !hasPattern;
        }

        light.updatePreviewState();
        light.updateActivateButton();
    };

    const processPatternStore = (store, options = {}) => {
        const patterns = Array.isArray(store.patterns) ? store.patterns.filter((item) => item && item.id) : [];
        patternState.store = store;
        patternState.items = patterns;
        patternState.map = new Map(patterns.map((item) => [item.id, item]));
        patternState.activeId = typeof store.active_pattern === 'string' ? store.active_pattern : '';

        syncActiveSnapshot();

        let nextSelected = options.selectedId || patternState.selectedId;
        if (nextSelected && nextSelected !== PATTERN_CONTEXT && !patternState.map.has(nextSelected)) {
            nextSelected = '';
        }
        if (!nextSelected) {
            if (patternState.activeId && patternState.map.has(patternState.activeId)) {
                nextSelected = patternState.activeId;
            } else if (patterns.length > 0) {
                nextSelected = patterns[0].id;
            } else {
                nextSelected = PATTERN_CONTEXT;
            }
        }

        patternState.selectedId = nextSelected;
        patternState.draft = normalizePattern(nextSelected === PATTERN_CONTEXT ? null : patternState.map.get(nextSelected));
        patternState.dirty = false;
        patternState.labelDirty = false;
        patternState.originalLabel = '';
        if (nextSelected !== PATTERN_CONTEXT) {
            const entry = patternState.map.get(nextSelected);
            if (entry && typeof entry.label === 'string') {
                patternState.originalLabel = entry.label.trim();
            }
        }

        applyPatternDraftToUI(patternState.draft);

        const statusText = options.statusText || patternState.lastLoadStatus.text;
        const statusTone = options.statusTone || patternState.lastLoadStatus.tone;
        patternState.lastLoadStatus = { text: statusText, tone: statusTone };
        if (options.skipStatus !== true) {
            light.setSettingsStatus(statusText, statusTone);
        }

        refreshPatternControls();
        renderActiveMarkers();
    };

    const ensurePatterns = async () => {
        if (patternState.loaded && patternState.store) {
            processPatternStore(patternState.store, {
                selectedId: patternState.selectedId || patternState.activeId || PATTERN_CONTEXT,
                skipStatus: true
            });
            light.setSettingsStatus(patternState.lastLoadStatus.text, patternState.lastLoadStatus.tone);
            return;
        }
        if (!patternState.loadingPromise) {
            patternState.loadingPromise = (async () => {
                light.setSettingsStatus('Patronen laden…', 'pending');
                try {
                    const response = await fetch('/api/light/patterns');
                    if (!response.ok) {
                        throw new Error(response.statusText);
                    }
                    const data = await response.json();
                    patternState.loaded = true;
                    processPatternStore(data, { statusText: 'Patronen geladen', statusTone: 'success' });
                } catch (error) {
                    patternState.loaded = false;
                    patternState.store = { patterns: [], active_pattern: '' };
                    processPatternStore(patternState.store, { statusText: 'Patronen laden mislukt', statusTone: 'error' });
                    throw error;
                } finally {
                    patternState.loadingPromise = null;
                }
            })();
        }
        try {
            await patternState.loadingPromise;
        } catch (error) {
            // status reeds gezet
        }
    };

    const markPatternDirty = () => {
        if (!patternState.draft) {
            return;
        }
        patternState.dirty = true;
        light.clearPreview();
        light.setSettingsStatus('Wijzigingen niet opgeslagen', 'pending');
        refreshPatternControls();
    };

    const persistPattern = async ({ createNew = false } = {}) => {
        if (!patternState.draft) {
            return false;
        }
        if (createNew && patternState.items.length >= MAX_LIGHT_PATTERNS) {
            light.setSettingsStatus('Maximaal aantal patronen bereikt', 'error');
            return false;
        }
        const body = {
            label: app.dom.patternName ? app.dom.patternName.value.trim() : '',
            select: createNew || patternState.selectedId === patternState.activeId,
            params: patternState.draft.params
        };
        if (!createNew) {
            if (!patternState.selectedId || patternState.selectedId === PATTERN_CONTEXT) {
                light.setSettingsStatus('Kies een patroon om te bewaren', 'error');
                return false;
            }
            body.id = patternState.selectedId;
        }
        light.setSettingsStatus('Patroon opslaan…', 'pending');
        try {
            const response = await fetch('/api/light/patterns', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(body)
            });
            if (!response.ok) {
                const message = await response.text();
                throw new Error(message || response.statusText);
            }
            const store = await response.json();
            const headerId = response.headers.get('X-Light-Pattern') || body.id || '';
            patternState.loaded = true;
            patternState.store = store;
            processPatternStore(store, {
                selectedId: headerId || PATTERN_CONTEXT,
                statusText: 'Patroon opgeslagen',
                statusTone: 'success'
            });
            if (app.light.colors && typeof app.light.colors.ensure === 'function') {
                await app.light.colors.ensure().catch(() => {});
            }
            return true;
        } catch (error) {
            light.setSettingsStatus(error.message || 'Opslaan mislukt', 'error');
            return false;
        }
    };

    const deletePattern = async () => {
        if (!patternState.selectedId || patternState.selectedId === PATTERN_CONTEXT) {
            light.setSettingsStatus('Selecteer een patroon om te verwijderen', 'error');
            return false;
        }
        const entry = patternState.map.get(patternState.selectedId);
        const label = entry ? (entry.label || entry.id) : patternState.selectedId;
        if (!window.confirm(`Patroon "${label}" verwijderen?`)) {
            return false;
        }
        light.setSettingsStatus('Patroon verwijderen…', 'pending');
        try {
            const response = await fetch('/api/light/patterns/delete', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ id: patternState.selectedId })
            });
            if (!response.ok) {
                const message = await response.text();
                throw new Error(message || response.statusText);
            }
            const store = await response.json();
            const headerId = response.headers.get('X-Light-Pattern') || store.active_pattern || '';
            patternState.loaded = true;
            patternState.store = store;
            processPatternStore(store, {
                selectedId: headerId || PATTERN_CONTEXT,
                statusText: 'Patroon verwijderd',
                statusTone: 'info'
            });
            if (app.light.colors && typeof app.light.colors.ensure === 'function') {
                await app.light.colors.ensure().catch(() => {});
            }
            return true;
        } catch (error) {
            light.setSettingsStatus(error.message || 'Verwijderen mislukt', 'error');
            return false;
        }
    };

    const restoreDrafts = () => {
        if (!patternState.store) {
            return;
        }
        processPatternStore(patternState.store, {
            selectedId: patternState.activeId || PATTERN_CONTEXT,
            statusText: patternState.lastLoadStatus.text,
            statusTone: patternState.lastLoadStatus.tone,
            skipStatus: true
        });
        light.setSettingsStatus(patternState.lastLoadStatus.text, patternState.lastLoadStatus.tone);
    };

    const applyActivePayload = (payload) => {
        if (!payload || typeof payload !== 'object') {
            return null;
        }
        const manual = payload.manual_override === true;
        const rawPatternId = typeof payload.pattern_id === 'string' ? payload.pattern_id.trim() : '';
        const patternIsContext = !manual && (!rawPatternId || rawPatternId.toLowerCase() === 'context');

        patternState.lastAppliedId = rawPatternId;
        patternState.activeId = patternIsContext ? '' : rawPatternId;

        syncActiveSnapshot();
        refreshPatternControls();

        const label = (() => {
            if (rawPatternId && patternState.map.has(rawPatternId)) {
                const item = patternState.map.get(rawPatternId);
                if (item && typeof item.label === 'string' && item.label.trim()) {
                    return item.label.trim();
                }
                return item ? item.id : utils.formatIdLabel(rawPatternId);
            }
            if (rawPatternId) {
                return utils.formatIdLabel(rawPatternId);
            }
            return 'Context';
        })();

        return {
            label,
            manual,
            source: typeof payload.source === 'string' ? payload.source.toLowerCase() : 'context'
        };
    };

    moduleApi.ensure = () => ensurePatterns();
    moduleApi.processStore = (store, options) => processPatternStore(store, options || {});
    moduleApi.restoreDrafts = restoreDrafts;
    moduleApi.applyActivePayload = applyActivePayload;
    moduleApi.renderActiveMarkers = renderActiveMarkers;
    moduleApi.syncActiveSnapshot = syncActiveSnapshot;
    moduleApi.markDirty = markPatternDirty;

    app.light.patterns = moduleApi;

    app.ready(() => {
        buildPatternControls();
        renderActiveMarkers();

        lightRangeInputs.forEach((input) => {
            input.addEventListener('input', () => {
                const key = input.getAttribute('data-setting-range');
                const cfg = patternSettingMap.get(key);
                if (!patternState.draft || !cfg) {
                    return;
                }
                const value = utils.clamp(parseFloat(input.value), cfg.min, cfg.max);
                patternState.draft.params[key] = value;
                updateLightSettingDisplay(key, value);
                markPatternDirty();
            });
        });

        if (app.dom.patternSelect) {
            app.dom.patternSelect.addEventListener('change', () => {
                const value = app.dom.patternSelect.value;
                patternState.selectedId = value;
                light.clearPreview();

                if (value === PATTERN_CONTEXT) {
                    patternState.labelDirty = false;
                    patternState.originalLabel = '';
                    patternState.draft = normalizePattern(null);
                    applyPatternDraftToUI(patternState.draft);
                    light.setSettingsStatus('Contextmodus geselecteerd', 'info');
                } else {
                    const pattern = patternState.map.get(value);
                    if (!pattern) {
                        light.setSettingsStatus('Patroon niet gevonden', 'error');
                        refreshPatternControls();
                        return;
                    }
                    patternState.draft = normalizePattern(pattern);
                    patternState.originalLabel = pattern.label && typeof pattern.label === 'string' ? pattern.label.trim() : '';
                    patternState.labelDirty = false;
                    applyPatternDraftToUI(patternState.draft);
                    light.setSettingsStatus('Patroon geladen', 'info');
                }
                refreshPatternControls();
            });
        }

        if (app.dom.patternName) {
            app.dom.patternName.addEventListener('input', () => {
                if (patternState.selectedId === PATTERN_CONTEXT) {
                    app.dom.patternName.value = '';
                    return;
                }
                const value = app.dom.patternName.value.trim();
                const original = (patternState.originalLabel || '').trim();
                patternState.labelDirty = value !== original;
                light.clearPreview();
                refreshPatternControls();
            });
        }

        if (app.dom.patternSave) {
            app.dom.patternSave.addEventListener('click', () => {
                persistPattern({ createNew: false }).catch(() => {});
            });
        }
        if (app.dom.patternSaveAs) {
            app.dom.patternSaveAs.addEventListener('click', () => {
                persistPattern({ createNew: true }).catch(() => {});
            });
        }
        if (app.dom.patternDelete) {
            app.dom.patternDelete.addEventListener('click', () => {
                deletePattern().catch(() => {});
            });
        }
    });
})();
