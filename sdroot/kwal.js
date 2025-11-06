(function () {
    'use strict';

    const fetchText = async (url, options = {}) => {
        const response = await fetch(url, options);
        if (!response.ok) {
            throw new Error(response.statusText);
        }
        return (await response.text()).trim();
    };

    const sleep = (ms) => new Promise((resolve) => window.setTimeout(resolve, ms));

    const setStatus = (id, text, tone = 'info') => {
        const node = document.getElementById(id);
        if (!node) {
            return;
        }
        node.className = `status status-${tone}`;
        node.textContent = text;
    };

    const clamp = (value, min, max) => Math.min(max, Math.max(min, value));
    const deepClone = (value) => JSON.parse(JSON.stringify(value));

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

    // Build sentinel: update version string whenever web assets change so the device/browser can verify freshness.
    window.APP_BUILD_INFO = Object.freeze({
        version: 'appjs-preview-fix-20251103T2305Z',
        features: ['previewFallback', 'lastAppliedTracking']
    });

    const defaultPattern = {
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

    const defaultColor = {
        rgb1_hex: '#ff7f00',
        rgb2_hex: '#552200'
    };

    const dom = {
        audioCurrent: document.getElementById('audioCurrent'),
        audioVolume: document.getElementById('audioVolume'),
        audioVolumeLabel: document.getElementById('audioVolumeLabel'),
        lightSlider: document.getElementById('lightSlider'),
        lightValue: document.getElementById('lightValue'),
        lightModalPercent: document.getElementById('lightModalPercent'),
        lightSettingsStatus: document.getElementById('lightSettingsStatus'),
        lightSettingsList: document.getElementById('lightSettingsList'),
        marker: document.querySelector('[data-marker="light"]'),
        lightModal: document.getElementById('lightModal'),
        otaModal: document.getElementById('otaModal'),
        patternSelect: document.getElementById('patternSelect'),
        patternName: document.getElementById('patternName'),
        patternSave: document.getElementById('patternSave'),
        patternSaveAs: document.getElementById('patternSaveAs'),
        patternDelete: document.getElementById('patternDelete'),
        colorSelect: document.getElementById('colorSelect'),
        colorName: document.getElementById('colorName'),
        colorSave: document.getElementById('colorSave'),
        colorSaveAs: document.getElementById('colorSaveAs'),
        colorDelete: document.getElementById('colorDelete'),
        presetPreview: document.getElementById('presetPreview'),
        presetActivate: document.getElementById('presetActivate'),
        colorInputs: {
            rgb1_hex: document.querySelector('input[data-color="rgb1_hex"]'),
            rgb2_hex: document.querySelector('input[data-color="rgb2_hex"]')
        },
        sdModal: document.getElementById('sdModal'),
        sdStatus: document.getElementById('sdStatus'),
        sdPath: document.getElementById('sdPath'),
        sdEntries: document.getElementById('sdEntries'),
        sdUploadForm: document.getElementById('sdUploadForm'),
        sdUploadStatus: document.getElementById('sdUploadStatus'),
        sdUploadButton: document.getElementById('sdUploadButton'),
        sdFile: document.getElementById('sdFile'),
        sdRefresh: document.getElementById('sdRefresh'),
        sdUp: document.getElementById('sdUp'),
        openSdManager: document.getElementById('openSdManager')
    };

    const buildPatternControls = () => {
        if (!dom.lightSettingsList) {
            return;
        }
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
        dom.lightSettingsList.appendChild(fragment);
    };

    buildPatternControls();

    const lightRangeInputs = Array.from(dom.lightSettingsList.querySelectorAll('[data-setting-range]'));

    const PATTERN_CONTEXT = '__context__';
    const COLOR_DEFAULT = '__default__';
    const MAX_LIGHT_PATTERNS = 100;
    const MAX_LIGHT_COLORS = 100;

    const createCollectionState = (defaultText) => ({
        store: null,
        items: [],
        map: new Map(),
        draft: null,
        loaded: false,
        loadingPromise: null,
        dirty: false,
        labelDirty: false,
        activeId: '',
        lastAppliedId: '',
        selectedId: '',
        originalLabel: '',
        activeSnapshot: null,
        lastLoadStatus: { text: defaultText, tone: 'info' }
    });

    const state = {
        brightnessLive: 0,
        brightnessDraft: 0,
        audioVolume: 0,
        audio: { dir: 0, file: 0 },
        pattern: createCollectionState('Patroon geladen'),
        color: createCollectionState('Kleurset geladen'),
        previewActive: false,
        sd: {
            status: null,
            loadingPromise: null,
            path: '/',
            parent: '/',
            entries: [],
            truncated: false
        }
    };

    const percentFrom255 = (value) => Math.round((value / 255) * 100);
    const valueFromPercent = (value) => Math.round((value / 100) * 255);

    const formatIdLabel = (id) => {
        if (typeof id !== 'string' || id.length === 0) {
            return '';
        }
        return id
            .replace(/_/g, ' ')
            .replace(/\b\w/g, (ch) => ch.toUpperCase());
    };

    const setMarker = (value255) => {
        if (!dom.marker) {
            return;
        }
        const pos = Math.max(0, Math.min(255, value255)) / 255;
        dom.marker.style.left = `${(pos * 100).toFixed(2)}%`;
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
        const label = dom.lightSettingsList.querySelector(`[data-value="${key}"]`);
        const input = dom.lightSettingsList.querySelector(`[data-setting-range="${key}"]`);
        if (input) {
            input.value = String(value);
        }
        if (label) {
            label.textContent = formatSettingValue(key, value);
        }
    };

    const setRangeMarkerPosition = (key, value) => {
        const marker = dom.lightSettingsList.querySelector(`[data-range-marker="${key}"]`);
        const cfg = patternSettingMap.get(key);
        if (!marker || !cfg) {
            return;
        }
        if (!Number.isFinite(value)) {
            marker.classList.add('range-marker-hidden');
            return;
        }
        const numericValue = clamp(value, cfg.min, cfg.max);
        const span = cfg.max - cfg.min;
        const ratio = span <= 0 ? 0 : (numericValue - cfg.min) / span;
        const clamped = Math.max(0, Math.min(1, ratio));
        marker.style.left = `${(clamped * 100).toFixed(2)}%`;
        marker.classList.remove('range-marker-hidden');
    };

    function renderActiveMarkers() {
        patternSettingConfig.forEach((cfg) => {
            const snapshot = state.pattern.activeSnapshot;
            const value = snapshot ? snapshot.params[cfg.key] : Number.NaN;
            setRangeMarkerPosition(cfg.key, value);
        });
    }

    const syncActiveSnapshot = () => {
        const activeId = state.pattern.activeId;
        if (activeId && activeId !== PATTERN_CONTEXT) {
            const activePattern = state.pattern.map.get(activeId);
            state.pattern.activeSnapshot = activePattern ? normalizePattern(activePattern) : null;
        } else {
            state.pattern.activeSnapshot = null;
        }
        renderActiveMarkers();
    };

    renderActiveMarkers();

    const validHex = (value) => typeof value === 'string' && /^#([0-9a-f]{6})$/i.test(value.trim());

    const normalizePattern = (source) => {
        const base = deepClone(defaultPattern);
        const rawParams = source && typeof source.params === 'object' ? source.params : {};
        patternSettingConfig.forEach((cfg) => {
            const raw = Number(rawParams[cfg.key]);
            if (Number.isFinite(raw)) {
                base.params[cfg.key] = clamp(raw, cfg.min, cfg.max);
            }
        });
        return base;
    };

    const normalizeColor = (source) => {
        const base = deepClone(defaultColor);
        if (!source || typeof source !== 'object') {
            return base;
        }
        if (validHex(source.rgb1_hex)) {
            base.rgb1_hex = source.rgb1_hex;
        }
        if (validHex(source.rgb2_hex)) {
            base.rgb2_hex = source.rgb2_hex;
        }
        return base;
    };

    const setLightSettingsStatus = (text, tone = 'info') => {
        if (!dom.lightSettingsStatus) {
            return;
        }
        dom.lightSettingsStatus.className = `status status-${tone}`;
        dom.lightSettingsStatus.textContent = text;
    };

    const setSdControlsEnabled = (enabled) => {
        if (dom.sdUploadButton) {
            dom.sdUploadButton.disabled = !enabled;
        }
        if (dom.sdFile) {
            dom.sdFile.disabled = !enabled;
        }
        if (dom.sdUp) {
            dom.sdUp.disabled = !enabled;
        }
        if (dom.sdRefresh) {
            dom.sdRefresh.disabled = !enabled;
        }
    };

    const applySdStatus = (payload) => {
        const ready = payload && payload.ready === true;
        const busy = payload && payload.busy === true;
        const hasIndex = payload && payload.hasIndex === true;
        state.sd.status = { ready, busy, hasIndex };

        let message;
        let tone;
        if (busy) {
            message = 'SD-kaart bezig';
            tone = 'pending';
        } else if (!ready) {
            message = 'SD-kaart niet beschikbaar';
            tone = 'error';
        } else if (!hasIndex) {
            message = 'SD-kaart gereed, index.html ontbreekt';
            tone = 'info';
        } else {
            message = 'SD-kaart gereed';
            tone = 'success';
        }

        setStatus('sdStatus', message, tone);

        if (dom.sdPath) {
            dom.sdPath.value = ready ? state.sd.path : '';
        }
        if (dom.sdUploadStatus) {
            if (!ready) {
                dom.sdUploadStatus.textContent = 'Geen kaart beschikbaar';
            } else if (busy) {
                dom.sdUploadStatus.textContent = 'Bezig, wacht even...';
            } else if (!dom.sdUploadStatus.textContent
                || dom.sdUploadStatus.textContent === 'Geen kaart beschikbaar'
                || dom.sdUploadStatus.textContent.startsWith('Bezig')) {
                dom.sdUploadStatus.textContent = 'Geen bestand gekozen';
            }
        }
        if (dom.sdEntries && !ready) {
            dom.sdEntries.innerHTML = '';
        }

        setSdControlsEnabled(ready && !busy);
        if (dom.sdUp) {
            dom.sdUp.disabled = !ready || busy || state.sd.path === '/';
        }
    };

    const fetchSdStatus = async (options = {}) => {
        if (state.sd.loadingPromise) {
            return state.sd.loadingPromise;
        }
        if (options.showPending !== false) {
            setStatus('sdStatus', 'Status ophalen…', 'pending');
        }
        const task = (async () => {
            try {
                const response = await fetch('/api/sd/status', { cache: 'no-store' });
                if (!response.ok) {
                    throw new Error(response.statusText);
                }
                const payload = await response.json();
                applySdStatus(payload);
                return payload;
            } catch (error) {
                setStatus('sdStatus', 'Status ophalen mislukt', 'error');
                throw error;
            } finally {
                state.sd.loadingPromise = null;
            }
        })();
        state.sd.loadingPromise = task;
        return task;
    };

    const formatBytes = (value) => {
        const units = ['B', 'KB', 'MB', 'GB'];
        let result = Number(value);
        let unitIndex = 0;
        while (result >= 1024 && unitIndex < units.length - 1) {
            result /= 1024;
            unitIndex += 1;
        }
        if (unitIndex === 0) {
            return `${Math.round(result)} ${units[unitIndex]}`;
        }
        return `${result.toFixed(result < 10 ? 1 : 0)} ${units[unitIndex]}`;
    };

    const renderSdEntries = () => {
        if (!dom.sdEntries) {
            return;
        }
        const fragment = document.createDocumentFragment();
        const entries = Array.isArray(state.sd.entries) ? state.sd.entries : [];
        if (entries.length === 0) {
            const row = document.createElement('tr');
            const cell = document.createElement('td');
            cell.colSpan = 4;
            cell.className = 'sd-empty';
            cell.textContent = 'Geen bestanden';
            row.appendChild(cell);
            fragment.appendChild(row);
        } else {
            entries.forEach((entry) => {
                const row = document.createElement('tr');
                row.className = 'sd-entry-row';
                row.dataset.name = entry.name;
                row.dataset.type = entry.type;

                const nameCell = document.createElement('td');
                if (entry.type === 'dir') {
                    const button = document.createElement('button');
                    button.type = 'button';
                    button.className = 'link-button sd-entry-nav';
                    button.dataset.name = entry.name;
                    button.textContent = entry.name || '(naamloos)';
                    nameCell.appendChild(button);
                } else {
                    nameCell.textContent = entry.name || '(naamloos)';
                }
                row.appendChild(nameCell);

                const typeCell = document.createElement('td');
                typeCell.textContent = entry.type === 'dir' ? 'Map' : 'Bestand';
                row.appendChild(typeCell);

                const sizeCell = document.createElement('td');
                sizeCell.textContent = entry.type === 'dir' ? '-' : formatBytes(entry.size || 0);
                row.appendChild(sizeCell);

                const actionsCell = document.createElement('td');
                actionsCell.className = 'sd-actions';
                const deleteButton = document.createElement('button');
                deleteButton.type = 'button';
                deleteButton.className = 'btn btn-small sd-entry-delete';
                deleteButton.dataset.name = entry.name;
                deleteButton.dataset.type = entry.type;
                deleteButton.textContent = 'Verwijder';
                if (entry.name === '' || entry.name === '.' || entry.name === '..') {
                    deleteButton.disabled = true;
                }
                actionsCell.appendChild(deleteButton);
                row.appendChild(actionsCell);

                fragment.appendChild(row);
            });
        }

        dom.sdEntries.innerHTML = '';
        dom.sdEntries.appendChild(fragment);
    };

    const applySdList = (payload) => {
        if (!payload || typeof payload !== 'object') {
            return;
        }
        state.sd.path = typeof payload.path === 'string' && payload.path.length > 0 ? payload.path : '/';
        state.sd.parent = typeof payload.parent === 'string' && payload.parent.length > 0 ? payload.parent : '/';
        state.sd.entries = Array.isArray(payload.entries) ? payload.entries : [];
        state.sd.truncated = payload.truncated === true;

        if (dom.sdPath) {
            dom.sdPath.value = state.sd.path;
        }

        renderSdEntries();

        if (dom.sdUp) {
            dom.sdUp.disabled = !state.sd.status || !state.sd.status.ready || state.sd.status.busy || state.sd.path === '/';
        }

        if (state.sd.truncated) {
            setStatus('sdStatus', 'Lijst ingekort (te veel items)', 'info');
        } else {
            setStatus('sdStatus', 'Map geladen', 'success');
        }
    };

    const joinSdPath = (basePath, name) => {
        if (!name) {
            return basePath || '/';
        }
        const trimmedName = name.replace(/\/+$/, '');
        if (basePath === '/' || !basePath) {
            return `/${trimmedName}`;
        }
        return `${basePath.replace(/\/+$/, '')}/${trimmedName}`;
    };

    const fetchSdList = async (targetPath, options = {}) => {
        if (!state.sd.status || state.sd.status.ready !== true) {
            setStatus('sdStatus', 'SD niet beschikbaar', 'error');
            return null;
        }

        const desiredPath = typeof targetPath === 'string' && targetPath.length > 0 ? targetPath : state.sd.path || '/';
        if (state.sd.loadingPromise) {
            return state.sd.loadingPromise;
        }
        if (options.showPending !== false) {
            setStatus('sdStatus', 'Map laden…', 'pending');
        }

        const task = (async () => {
            try {
                const response = await fetch(`/api/sd/list?path=${encodeURIComponent(desiredPath)}`, { cache: 'no-store' });
                if (!response.ok) {
                    throw new Error(response.statusText);
                }
                const payload = await response.json();
                applySdList(payload);
                return payload;
            } catch (error) {
                setStatus('sdStatus', error.message || 'Map laden mislukt', 'error');
                throw error;
            } finally {
                state.sd.loadingPromise = null;
            }
        })();
        state.sd.loadingPromise = task;
        return task;
    };

    const uploadSdFile = async () => {
        if (!state.sd.status || !state.sd.status.ready) {
            setStatus('sdStatus', 'SD niet beschikbaar', 'error');
            return false;
        }
        if (!dom.sdFile || !dom.sdFile.files || dom.sdFile.files.length === 0) {
            setStatus('sdStatus', 'Geen bestand gekozen', 'error');
            return false;
        }
        const file = dom.sdFile.files[0];
        const formData = new FormData();
        formData.append('file', file, file.name);
        formData.append('path', state.sd.path || '/');

        if (dom.sdUploadButton) {
            dom.sdUploadButton.disabled = true;
        }
        if (dom.sdFile) {
            dom.sdFile.disabled = true;
        }

        setStatus('sdStatus', `Uploaden ${file.name}…`, 'pending');
        try {
            const response = await fetch('/api/sd/upload', {
                method: 'POST',
                body: formData
            });
            if (!response.ok) {
                const text = await response.text();
                throw new Error(text || response.statusText);
            }
            await response.json().catch(() => ({}));
            setStatus('sdStatus', 'Upload geslaagd', 'success');
            if (dom.sdUploadStatus) {
                dom.sdUploadStatus.textContent = 'Geen bestand gekozen';
            }
            if (dom.sdFile) {
                dom.sdFile.value = '';
            }
            await fetchSdList(state.sd.path || '/', { showPending: false }).catch(() => {});
            return true;
        } catch (error) {
            setStatus('sdStatus', error.message || 'Upload mislukt', 'error');
            return false;
        } finally {
            if (dom.sdUploadButton) {
                dom.sdUploadButton.disabled = !state.sd.status || !state.sd.status.ready || state.sd.status.busy;
            }
            if (dom.sdFile) {
                dom.sdFile.disabled = !state.sd.status || !state.sd.status.ready || state.sd.status.busy;
            }
            setSdControlsEnabled(state.sd.status && state.sd.status.ready && !state.sd.status.busy);
        }
    };

    const deleteSdEntry = async (name, type) => {
        if (!name) {
            return false;
        }
        const targetPath = joinSdPath(state.sd.path || '/', name);
        if (!window.confirm(`Verwijder ${type === 'dir' ? 'map' : 'bestand'} "${name}"?`)) {
            return false;
        }
        setStatus('sdStatus', `Verwijderen ${name}…`, 'pending');
        try {
            const response = await fetch('/api/sd/delete', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ path: targetPath })
            });
            if (!response.ok) {
                const text = await response.text();
                throw new Error(text || response.statusText);
            }
            await response.json().catch(() => ({}));
            setStatus('sdStatus', 'Verwijderd', 'success');
            await fetchSdList(state.sd.path || '/', { showPending: false }).catch(() => {});
            return true;
        } catch (error) {
            setStatus('sdStatus', error.message || 'Verwijderen mislukt', 'error');
            return false;
        }
        finally {
            setSdControlsEnabled(state.sd.status && state.sd.status.ready && !state.sd.status.busy);
        }
    };

    setSdControlsEnabled(false);

    const applyPatternDraftToUI = (draft) => {
        if (!draft) {
            return;
        }
        patternSettingConfig.forEach((cfg) => {
            updateLightSettingDisplay(cfg.key, draft.params[cfg.key]);
        });
        state.pattern.dirty = false;
    };

    const applyColorDraftToUI = (draft) => {
        if (!draft) {
            return;
        }
        if (dom.colorInputs.rgb1_hex) {
            dom.colorInputs.rgb1_hex.value = draft.rgb1_hex;
        }
        if (dom.colorInputs.rgb2_hex) {
            dom.colorInputs.rgb2_hex.value = draft.rgb2_hex;
        }
        state.color.dirty = false;
    };

    const updatePreviewState = () => {
        if (!dom.presetPreview) {
            return;
        }
        dom.presetPreview.disabled = !state.pattern.draft || !state.color.draft;
        dom.presetPreview.textContent = state.previewActive ? 'Voorbeeld actief' : 'Voorbeeld';
    };

    const updatePatternControls = () => {
        if (!dom.patternSelect) {
            return;
        }
        const select = dom.patternSelect;
        const currentId = state.pattern.selectedId || PATTERN_CONTEXT;
        const activeId = state.pattern.activeId || '';
        select.innerHTML = '';

        const contextOption = document.createElement('option');
        contextOption.value = PATTERN_CONTEXT;
        contextOption.textContent = activeId ? 'Context (automatisch)' : 'Context (automatisch) •';
        contextOption.selected = currentId === PATTERN_CONTEXT;
        select.appendChild(contextOption);

        state.pattern.items.forEach((pattern) => {
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
        const hasPattern = !isContext && state.pattern.map.has(currentId);

        if (dom.patternName) {
            dom.patternName.disabled = isContext;
        }
        if (dom.patternSave) {
            dom.patternSave.disabled = isContext || (!state.pattern.dirty && !state.pattern.labelDirty);
        }
        if (dom.patternSaveAs) {
            dom.patternSaveAs.disabled = state.pattern.items.length >= MAX_LIGHT_PATTERNS;
        }
        if (dom.patternDelete) {
            dom.patternDelete.disabled = isContext || !hasPattern;
        }

        const selectedPatternLabel = (() => {
            if (isContext) {
                return '';
            }
            const item = state.pattern.map.get(currentId);
            if (!item) {
                return '';
            }
            return item.label && item.label.trim() ? item.label : item.id;
        })();
        if (dom.patternName && !state.pattern.labelDirty) {
            dom.patternName.value = selectedPatternLabel;
        }

        updatePreviewState();
        updateActivateButton();
    };

    const updateColorControls = () => {
        if (!dom.colorSelect) {
            return;
        }
        const select = dom.colorSelect;
        const currentId = state.color.selectedId || COLOR_DEFAULT;
        const activeId = state.color.activeId || '';
        select.innerHTML = '';

        const defaultOption = document.createElement('option');
        defaultOption.value = COLOR_DEFAULT;
        defaultOption.textContent = activeId ? 'Standaard kleuren' : 'Standaard kleuren •';
        defaultOption.selected = currentId === COLOR_DEFAULT;
        select.appendChild(defaultOption);

        state.color.items.forEach((entry) => {
            if (!entry || !entry.id) {
                return;
            }
            const option = document.createElement('option');
            option.value = entry.id;
            const label = entry.label && entry.label.trim() ? entry.label : entry.id;
            option.textContent = entry.id === activeId ? `${label} •` : label;
            option.selected = entry.id === currentId;
            select.appendChild(option);
        });

        if (select.selectedIndex < 0 && select.options.length > 0) {
            select.options[0].selected = true;
        }

        const isDefault = currentId === COLOR_DEFAULT;
        const hasColor = !isDefault && state.color.map.has(currentId);

        if (dom.colorName) {
            dom.colorName.disabled = isDefault;
        }
        if (dom.colorSave) {
            dom.colorSave.disabled = isDefault || (!state.color.dirty && !state.color.labelDirty);
        }
        if (dom.colorSaveAs) {
            dom.colorSaveAs.disabled = state.color.items.length >= MAX_LIGHT_COLORS;
        }
        if (dom.colorDelete) {
            dom.colorDelete.disabled = isDefault || !hasColor;
        }

        if (dom.colorName && !state.color.labelDirty) {
            const entry = state.color.map.get(currentId);
            dom.colorName.value = !isDefault && entry ? (entry.label && entry.label.trim() ? entry.label : entry.id) : '';
        }

        updatePreviewState();
        updateActivateButton();
    };

    const applyActiveLightStatus = (payload) => {
        if (!payload || typeof payload !== 'object') {
            return '';
        }

    const manual = payload.manual_override === true;
    const rawPatternId = typeof payload.pattern_id === 'string' ? payload.pattern_id.trim() : '';
    const rawColorId = typeof payload.color_id === 'string' ? payload.color_id.trim() : '';
    const colorOverride = payload.color_overridden === true;

    const patternIsContext = !manual && (!rawPatternId || rawPatternId.toLowerCase() === 'context');
    const colorIsDefault = !colorOverride || !rawColorId || rawColorId.toLowerCase() === 'default';

    state.pattern.lastAppliedId = rawPatternId;
    state.color.lastAppliedId = rawColorId;

    state.pattern.activeId = patternIsContext ? '' : rawPatternId;
    state.color.activeId = colorIsDefault ? '' : rawColorId;

        syncActiveSnapshot();
        updatePatternControls();
        updateColorControls();
        renderActiveMarkers();
        state.previewActive = false;
        updatePreviewState();

        const patternLabel = (() => {
            if (rawPatternId && state.pattern.map.has(rawPatternId)) {
                const item = state.pattern.map.get(rawPatternId);
                if (item && typeof item.label === 'string' && item.label.trim().length > 0) {
                    return item.label.trim();
                }
                return item ? item.id : formatIdLabel(rawPatternId);
            }
            if (rawPatternId) {
                return formatIdLabel(rawPatternId);
            }
            return 'Context';
        })();

        const colorLabel = (() => {
            if (colorOverride && rawColorId && state.color.map.has(rawColorId)) {
                const item = state.color.map.get(rawColorId);
                if (item && typeof item.label === 'string' && item.label.trim().length > 0) {
                    return item.label.trim();
                }
                return item ? item.id : formatIdLabel(rawColorId);
            }
            if (colorOverride && rawColorId) {
                return formatIdLabel(rawColorId);
            }
            return 'Standaard';
        })();

        const source = typeof payload.source === 'string' ? payload.source.toLowerCase() : 'context';
        let message;
        if (manual || source === 'manual') {
            message = `Patroon ${patternLabel} actief`;
        } else if (source === 'calendar') {
            message = `Kalenderpatroon ${patternLabel}`;
        } else if (source === 'default') {
            message = `Standaardpatroon ${patternLabel}`;
        } else {
            message = patternLabel === 'Context' ? 'Context actief' : `Context ${patternLabel}`;
        }

        if (colorOverride) {
            message += ` • kleuren ${colorLabel}`;
        } else {
            message += ' • standaardkleuren';
        }

        return message;
    };

    const updateActivateButton = () => {
        if (!dom.presetActivate) {
            return;
        }
        const selectedPattern = state.pattern.selectedId || PATTERN_CONTEXT;
        const activePattern = state.pattern.activeId || PATTERN_CONTEXT;
        const selectedColor = state.color.selectedId || COLOR_DEFAULT;
        const activeColor = state.color.activeId || COLOR_DEFAULT;

        const patternMatches = selectedPattern === activePattern;
        const colorMatches = selectedColor === activeColor;
        const anyDirty = state.pattern.dirty || state.pattern.labelDirty || state.color.dirty || state.color.labelDirty;

        if (selectedPattern === PATTERN_CONTEXT) {
            dom.presetActivate.textContent = activePattern === PATTERN_CONTEXT ? 'Context actief' : 'Activeer context';
            dom.presetActivate.disabled = activePattern === PATTERN_CONTEXT && !anyDirty && selectedColor === activeColor;
        } else {
            dom.presetActivate.textContent = 'Activeer';
            dom.presetActivate.disabled = patternMatches && colorMatches && !anyDirty;
        }
    };

    const processPatternStore = (store, options = {}) => {
        const patterns = Array.isArray(store.patterns) ? store.patterns.filter((item) => item && item.id) : [];
        state.pattern.store = store;
        state.pattern.items = patterns;
        state.pattern.map = new Map(patterns.map((item) => [item.id, item]));
        state.pattern.activeId = typeof store.active_pattern === 'string' ? store.active_pattern : '';
        state.previewActive = false;

        syncActiveSnapshot();

        let nextSelected = options.selectedId || state.pattern.selectedId;
        if (nextSelected && nextSelected !== PATTERN_CONTEXT && !state.pattern.map.has(nextSelected)) {
            nextSelected = '';
        }
        if (!nextSelected) {
            if (state.pattern.activeId && state.pattern.map.has(state.pattern.activeId)) {
                nextSelected = state.pattern.activeId;
            } else if (patterns.length > 0) {
                nextSelected = patterns[0].id;
            } else {
                nextSelected = PATTERN_CONTEXT;
            }
        }
        state.pattern.selectedId = nextSelected;

        const draft = normalizePattern(nextSelected === PATTERN_CONTEXT ? null : state.pattern.map.get(nextSelected));
        state.pattern.draft = draft;
        state.pattern.dirty = false;
        state.pattern.labelDirty = false;
        state.pattern.originalLabel = '';
        if (nextSelected !== PATTERN_CONTEXT) {
            const entry = state.pattern.map.get(nextSelected);
            if (entry && typeof entry.label === 'string') {
                state.pattern.originalLabel = entry.label.trim();
            }
        }

        applyPatternDraftToUI(draft);

        const statusText = options.statusText || state.pattern.lastLoadStatus.text;
        const statusTone = options.statusTone || state.pattern.lastLoadStatus.tone;
        if (options.skipStatus !== true) {
            state.pattern.lastLoadStatus = { text: statusText, tone: statusTone };
            setLightSettingsStatus(statusText, statusTone);
        }

        updatePatternControls();
    };

    const processColorStore = (store, options = {}) => {
        const colors = Array.isArray(store.colors) ? store.colors.filter((item) => item && item.id) : [];
        state.color.store = store;
        state.color.items = colors;
        state.color.map = new Map(colors.map((item) => [item.id, item]));
        state.color.activeId = typeof store.active_color === 'string' ? store.active_color : '';

        let nextSelected = options.selectedId || state.color.selectedId;
        if (nextSelected && nextSelected !== COLOR_DEFAULT && !state.color.map.has(nextSelected)) {
            nextSelected = '';
        }
        if (!nextSelected) {
            if (state.color.activeId && state.color.map.has(state.color.activeId)) {
                nextSelected = state.color.activeId;
            } else if (colors.length > 0) {
                nextSelected = colors[0].id;
            } else {
                nextSelected = COLOR_DEFAULT;
            }
        }
        state.color.selectedId = nextSelected || COLOR_DEFAULT;

        const source = nextSelected === COLOR_DEFAULT ? null : state.color.map.get(nextSelected);
        const draft = normalizeColor(source);
        state.color.draft = draft;
        state.color.dirty = false;
        state.color.labelDirty = false;
        state.color.originalLabel = source && typeof source.label === 'string' ? source.label.trim() : '';

        applyColorDraftToUI(draft);

        if (options.statusText && options.skipStatus !== true) {
            state.color.lastLoadStatus = { text: options.statusText, tone: options.statusTone || 'info' };
            setLightSettingsStatus(options.statusText, options.statusTone || 'info');
        }

        updateColorControls();
    };

    const ensurePatterns = async () => {
        if (state.pattern.loaded && state.pattern.store) {
            processPatternStore(state.pattern.store, { selectedId: state.pattern.selectedId || state.pattern.activeId || PATTERN_CONTEXT, skipStatus: true });
            setLightSettingsStatus(state.pattern.lastLoadStatus.text, state.pattern.lastLoadStatus.tone);
            return;
        }
        if (!state.pattern.loadingPromise) {
            state.pattern.loadingPromise = (async () => {
                setLightSettingsStatus('Patronen laden…', 'pending');
                try {
                    const response = await fetch('/api/light/patterns');
                    if (!response.ok) {
                        throw new Error(response.statusText);
                    }
                    const data = await response.json();
                    state.pattern.loaded = true;
                    processPatternStore(data, { statusText: 'Patronen geladen', statusTone: 'success' });
                } catch (error) {
                    state.pattern.loaded = false;
                    state.pattern.store = { patterns: [], active_pattern: '' };
                    processPatternStore(state.pattern.store, { statusText: 'Patronen laden mislukt', statusTone: 'error' });
                    throw error;
                } finally {
                    state.pattern.loadingPromise = null;
                }
            })();
        }
        try {
            await state.pattern.loadingPromise;
        } catch (error) {
            // status al gezet
        }
    };

    const ensureColors = async () => {
        if (state.color.loaded && state.color.store) {
            processColorStore(state.color.store, { selectedId: state.color.selectedId || state.color.activeId || COLOR_DEFAULT, skipStatus: true });
            return;
        }
        if (!state.color.loadingPromise) {
            state.color.loadingPromise = (async () => {
                try {
                    const response = await fetch('/api/light/colors');
                    if (!response.ok) {
                        throw new Error(response.statusText);
                    }
                    const data = await response.json();
                    state.color.loaded = true;
                    processColorStore(data, { skipStatus: true });
                } catch (error) {
                    state.color.loaded = false;
                    state.color.store = { colors: [], active_color: '' };
                    processColorStore(state.color.store, { skipStatus: true });
                    setLightSettingsStatus('Kleurensets laden mislukt', 'error');
                    throw error;
                } finally {
                    state.color.loadingPromise = null;
                }
            })();
        }
        try {
            await state.color.loadingPromise;
        } catch (error) {
            // status reeds gezet
        }
    };

    const markPatternDirty = () => {
        if (!state.pattern.draft) {
            return;
        }
        state.pattern.dirty = true;
        state.previewActive = false;
        setLightSettingsStatus('Wijzigingen niet opgeslagen', 'pending');
        updatePatternControls();
        updatePreviewState();
    };

    const markColorDirty = () => {
        if (!state.color.draft) {
            return;
        }
        state.color.dirty = true;
        state.previewActive = false;
        setLightSettingsStatus('Wijzigingen niet opgeslagen', 'pending');
        updateColorControls();
        updatePreviewState();
    };

    lightRangeInputs.forEach((input) => {
        input.addEventListener('input', () => {
            const key = input.getAttribute('data-setting-range');
            const cfg = patternSettingMap.get(key);
            if (!state.pattern.draft || !cfg) {
                return;
            }
            const value = clamp(parseFloat(input.value), cfg.min, cfg.max);
            state.pattern.draft.params[key] = value;
            updateLightSettingDisplay(key, value);
            markPatternDirty();
        });
    });

    Object.values(dom.colorInputs).forEach((input) => {
        if (!input) {
            return;
        }
        input.addEventListener('input', () => {
            if (!state.color.draft || !validHex(input.value)) {
                return;
            }
            state.color.draft[input.dataset.color] = input.value;
            markColorDirty();
        });
    });

    if (dom.patternSelect) {
        dom.patternSelect.addEventListener('change', () => {
            const value = dom.patternSelect.value;
            state.pattern.selectedId = value;
            state.previewActive = false;

            if (value === PATTERN_CONTEXT) {
                state.pattern.labelDirty = false;
                state.pattern.originalLabel = '';
                state.pattern.draft = normalizePattern(null);
                applyPatternDraftToUI(state.pattern.draft);
                setLightSettingsStatus('Contextmodus geselecteerd', 'info');
            } else {
                const pattern = state.pattern.map.get(value);
                if (!pattern) {
                    setLightSettingsStatus('Patroon niet gevonden', 'error');
                    return;
                }
                state.pattern.draft = normalizePattern(pattern);
                state.pattern.originalLabel = pattern.label && typeof pattern.label === 'string' ? pattern.label.trim() : '';
                state.pattern.labelDirty = false;
                applyPatternDraftToUI(state.pattern.draft);
                setLightSettingsStatus('Patroon geladen', 'info');
            }
            updatePatternControls();
        });
    }

    if (dom.patternName) {
        dom.patternName.addEventListener('input', () => {
            if (state.pattern.selectedId === PATTERN_CONTEXT) {
                dom.patternName.value = '';
                return;
            }
            const value = dom.patternName.value.trim();
            const original = (state.pattern.originalLabel || '').trim();
            state.pattern.labelDirty = value !== original;
            state.previewActive = false;
            updatePatternControls();
            updatePreviewState();
        });
    }

    if (dom.colorSelect) {
        dom.colorSelect.addEventListener('change', () => {
            const value = dom.colorSelect.value;
            state.color.selectedId = value;
            state.previewActive = false;

            if (value === COLOR_DEFAULT) {
                state.color.draft = normalizeColor(null);
                state.color.labelDirty = false;
                state.color.originalLabel = '';
                applyColorDraftToUI(state.color.draft);
                setLightSettingsStatus('Standaard kleuren gekozen', 'info');
            } else {
                const entry = state.color.map.get(value);
                if (!entry) {
                    setLightSettingsStatus('Kleurset niet gevonden', 'error');
                    return;
                }
                state.color.draft = normalizeColor(entry);
                state.color.originalLabel = entry.label && typeof entry.label === 'string' ? entry.label.trim() : '';
                state.color.labelDirty = false;
                applyColorDraftToUI(state.color.draft);
                setLightSettingsStatus('Kleurset geladen', 'info');
            }
            updateColorControls();
        });
    }

    if (dom.colorName) {
        dom.colorName.addEventListener('input', () => {
            if (state.color.selectedId === COLOR_DEFAULT) {
                dom.colorName.value = '';
                return;
            }
            const value = dom.colorName.value.trim();
            const original = (state.color.originalLabel || '').trim();
            state.color.labelDirty = value !== original;
            state.previewActive = false;
            updateColorControls();
            updatePreviewState();
        });
    }

    const persistPattern = async ({ createNew = false } = {}) => {
        if (!state.pattern.draft) {
            return false;
        }
        if (createNew && state.pattern.items.length >= MAX_LIGHT_PATTERNS) {
            setLightSettingsStatus('Maximaal aantal patronen bereikt', 'error');
            return false;
        }
        const body = {
            label: dom.patternName ? dom.patternName.value.trim() : '',
            select: createNew || state.pattern.selectedId === state.pattern.activeId,
            params: state.pattern.draft.params
        };
        if (!createNew) {
            if (!state.pattern.selectedId || state.pattern.selectedId === PATTERN_CONTEXT) {
                setLightSettingsStatus('Kies een patroon om te bewaren', 'error');
                return false;
            }
            body.id = state.pattern.selectedId;
        }
        setLightSettingsStatus('Patroon opslaan…', 'pending');
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
            state.pattern.loaded = true;
            state.pattern.store = store;
            state.previewActive = false;
            processPatternStore(store, { selectedId: headerId || PATTERN_CONTEXT, statusText: 'Patroon opgeslagen', statusTone: 'success' });
            await ensureColors().catch(() => {});
            return true;
        } catch (error) {
            setLightSettingsStatus(error.message || 'Opslaan mislukt', 'error');
            return false;
        }
    };

    const persistColor = async ({ createNew = false } = {}) => {
        if (!state.color.draft) {
            return false;
        }
        if (createNew && state.color.items.length >= MAX_LIGHT_COLORS) {
            setLightSettingsStatus('Maximaal aantal kleurensets bereikt', 'error');
            return false;
        }
        const body = {
            label: dom.colorName ? dom.colorName.value.trim() : '',
            select: createNew || state.color.selectedId === state.color.activeId,
            rgb1_hex: state.color.draft.rgb1_hex,
            rgb2_hex: state.color.draft.rgb2_hex
        };
        if (!createNew) {
            if (!state.color.selectedId || state.color.selectedId === COLOR_DEFAULT) {
                setLightSettingsStatus('Kies een kleurset om te bewaren', 'error');
                return false;
            }
            body.id = state.color.selectedId;
        }
        setLightSettingsStatus('Kleurset opslaan…', 'pending');
        try {
            const response = await fetch('/api/light/colors', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(body)
            });
            if (!response.ok) {
                const message = await response.text();
                throw new Error(message || response.statusText);
            }
            const store = await response.json();
            const headerId = response.headers.get('X-Light-Color') || body.id || '';
            state.color.loaded = true;
            state.color.store = store;
            state.previewActive = false;
            processColorStore(store, { selectedId: headerId || COLOR_DEFAULT, skipStatus: true });
            setLightSettingsStatus('Kleurset opgeslagen', 'success');
            return true;
        } catch (error) {
            setLightSettingsStatus(error.message || 'Opslaan mislukt', 'error');
            return false;
        }
    };

    const deletePattern = async () => {
        if (!state.pattern.selectedId || state.pattern.selectedId === PATTERN_CONTEXT) {
            setLightSettingsStatus('Selecteer een patroon om te verwijderen', 'error');
            return false;
        }
        const entry = state.pattern.map.get(state.pattern.selectedId);
        const label = entry ? (entry.label || entry.id) : state.pattern.selectedId;
        if (!window.confirm(`Patroon "${label}" verwijderen?`)) {
            return false;
        }
        setLightSettingsStatus('Patroon verwijderen…', 'pending');
        try {
            const response = await fetch('/api/light/patterns/delete', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ id: state.pattern.selectedId })
            });
            if (!response.ok) {
                const message = await response.text();
                throw new Error(message || response.statusText);
            }
            const store = await response.json();
            const headerId = response.headers.get('X-Light-Pattern') || store.active_pattern || '';
            state.pattern.loaded = true;
            state.pattern.store = store;
            state.previewActive = false;
            processPatternStore(store, { selectedId: headerId || PATTERN_CONTEXT, statusText: 'Patroon verwijderd', statusTone: 'info' });
            await ensureColors().catch(() => {});
            return true;
        } catch (error) {
            setLightSettingsStatus(error.message || 'Verwijderen mislukt', 'error');
            return false;
        }
    };

    const deleteColor = async () => {
        if (!state.color.selectedId || state.color.selectedId === COLOR_DEFAULT) {
            setLightSettingsStatus('Selecteer een kleurset om te verwijderen', 'error');
            return false;
        }
        const entry = state.color.map.get(state.color.selectedId);
        const label = entry ? (entry.label || entry.id) : state.color.selectedId;
        if (!window.confirm(`Kleurset "${label}" verwijderen?`)) {
            return false;
        }
        setLightSettingsStatus('Kleurset verwijderen…', 'pending');
        try {
            const response = await fetch('/api/light/colors/delete', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ id: state.color.selectedId })
            });
            if (!response.ok) {
                const message = await response.text();
                throw new Error(message || response.statusText);
            }
            const store = await response.json();
            const headerId = response.headers.get('X-Light-Color') || store.active_color || '';
            state.color.loaded = true;
            state.color.store = store;
            state.previewActive = false;
            processColorStore(store, { selectedId: headerId || COLOR_DEFAULT, skipStatus: true });
            setLightSettingsStatus('Kleurset verwijderd', 'info');
            return true;
        } catch (error) {
            setLightSettingsStatus(error.message || 'Verwijderen mislukt', 'error');
            return false;
        }
    };

    const previewSelection = async () => {
        if (!state.pattern.draft || !state.color.draft) {
            return false;
        }

        const selectedPattern = state.pattern.selectedId || '';
        const activePattern = state.pattern.activeId || '';
        const lastPattern = state.pattern.lastAppliedId || '';
        const selectedColor = state.color.selectedId || '';
        const activeColor = state.color.activeId || '';
        const lastColor = state.color.lastAppliedId || '';

        const patternIdForPreview = (() => {
            if (selectedPattern && selectedPattern !== PATTERN_CONTEXT) {
                return selectedPattern;
            }
            if (activePattern && activePattern !== PATTERN_CONTEXT) {
                return activePattern;
            }
            if (lastPattern) {
                return lastPattern;
            }
            return '';
        })();

        const colorIdForPreview = (() => {
            if (selectedColor && selectedColor !== COLOR_DEFAULT) {
                return selectedColor;
            }
            if (activeColor && activeColor !== COLOR_DEFAULT) {
                return activeColor;
            }
            if (lastColor) {
                return lastColor;
            }
            return '';
        })();

        const body = {
            pattern: {
                params: deepClone(state.pattern.draft.params)
            },
            color: {
                rgb1_hex: state.color.draft.rgb1_hex,
                rgb2_hex: state.color.draft.rgb2_hex
            }
        };

        const debugSnapshot = {
            selectedPattern,
            activePattern,
            lastPattern,
            selectedColor,
            activeColor,
            lastColor,
            patternIdForPreview,
            colorIdForPreview
        };
        window.__LIGHT_PREVIEW_DEBUG__ = {
            at: new Date().toISOString(),
            state: debugSnapshot,
            body: deepClone(body)
        };
        console.debug('[light-preview] prepared request', window.__LIGHT_PREVIEW_DEBUG__);

        body._debug = {
            version: window.APP_BUILD_INFO && window.APP_BUILD_INFO.version ? window.APP_BUILD_INFO.version : 'unknown',
            snapshot: debugSnapshot,
            timestamp: window.__LIGHT_PREVIEW_DEBUG__.at
        };

        if (patternIdForPreview) {
            body.pattern.id = patternIdForPreview;
            body.pattern_id = patternIdForPreview;
        }
        if (colorIdForPreview) {
            body.color.id = colorIdForPreview;
            body.color_id = colorIdForPreview;
        }

        if (!patternIdForPreview) {
            setLightSettingsStatus('Geen patroon beschikbaar voor voorbeeld', 'error');
            state.previewActive = false;
            updatePreviewState();
            return false;
        }

        const previewPatternLabel = patternIdForPreview || '(onbekend)';
        const previewColorLabel = colorIdForPreview || 'default';
        setLightSettingsStatus(`Voorbeeld: ${previewPatternLabel} • kleur ${previewColorLabel}`, 'pending');
        try {
            const response = await fetch('/api/light/preview', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(body)
            });
            if (!response.ok) {
                const message = await response.text();
                throw new Error(message || response.statusText);
            }
            state.previewActive = true;
            setLightSettingsStatus('Voorbeeld actief (niet opgeslagen)', 'info');
            updatePreviewState();
            return true;
        } catch (error) {
            state.previewActive = false;
            setLightSettingsStatus(error.message || 'Voorbeeld mislukt', 'error');
            updatePreviewState();
            return false;
        }
    };

    const activateSelection = async (patternId, colorId) => {
        const selectedPattern = typeof patternId === 'string' ? patternId : state.pattern.selectedId || PATTERN_CONTEXT;
        const selectedColor = typeof colorId === 'string' ? colorId : state.color.selectedId || COLOR_DEFAULT;

        const patternPayload = { id: selectedPattern === PATTERN_CONTEXT ? '' : selectedPattern };
        const colorPayload = { id: selectedColor === COLOR_DEFAULT ? '' : selectedColor };

        setLightSettingsStatus('Activeren…', 'pending');
        try {
            const patternResponse = await fetch('/api/light/patterns/select', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(patternPayload)
            });
            if (!patternResponse.ok) {
                const message = await patternResponse.text();
                throw new Error(message || patternResponse.statusText);
            }
            const patternStore = await patternResponse.json();
            state.pattern.loaded = true;
            state.pattern.store = patternStore;
            processPatternStore(patternStore, {
                selectedId: patternPayload.id || PATTERN_CONTEXT,
                statusText: patternPayload.id ? 'Patroon actief' : 'Context actief',
                statusTone: patternPayload.id ? 'success' : 'info'
            });

            const colorResponse = await fetch('/api/light/colors/select', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(colorPayload)
            });
            if (!colorResponse.ok) {
                const message = await colorResponse.text();
                throw new Error(message || colorResponse.statusText);
            }
            const colorStore = await colorResponse.json();
            state.color.loaded = true;
            state.color.store = colorStore;
            processColorStore(colorStore, { selectedId: colorPayload.id || COLOR_DEFAULT, skipStatus: true });

            state.previewActive = false;
            updatePreviewState();
            return true;
        } catch (error) {
            setLightSettingsStatus(error.message || 'Activeren mislukt', 'error');
            return false;
        }
    };

    const restoreDrafts = () => {
        if (state.pattern.store) {
            processPatternStore(state.pattern.store, {
                selectedId: state.pattern.activeId || PATTERN_CONTEXT,
                statusText: state.pattern.lastLoadStatus.text,
                statusTone: state.pattern.lastLoadStatus.tone
            });
        }
        if (state.color.store) {
            processColorStore(state.color.store, {
                selectedId: state.color.activeId || COLOR_DEFAULT,
                skipStatus: true
            });
        }
        state.previewActive = false;
        updatePreviewState();
    };

    if (dom.patternSave) {
        dom.patternSave.addEventListener('click', () => {
            persistPattern({ createNew: false }).catch(() => {});
        });
    }
    if (dom.patternSaveAs) {
        dom.patternSaveAs.addEventListener('click', () => {
            persistPattern({ createNew: true }).catch(() => {});
        });
    }
    if (dom.patternDelete) {
        dom.patternDelete.addEventListener('click', () => {
            deletePattern().catch(() => {});
        });
    }

    if (dom.colorSave) {
        dom.colorSave.addEventListener('click', () => {
            persistColor({ createNew: false }).catch(() => {});
        });
    }
    if (dom.colorSaveAs) {
        dom.colorSaveAs.addEventListener('click', () => {
            persistColor({ createNew: true }).catch(() => {});
        });
    }
    if (dom.colorDelete) {
        dom.colorDelete.addEventListener('click', () => {
            deleteColor().catch(() => {});
        });
    }

    if (dom.presetPreview) {
        dom.presetPreview.addEventListener('click', () => {
            previewSelection().catch(() => {});
        });
    }

    if (dom.presetActivate) {
        dom.presetActivate.addEventListener('click', () => {
            activateSelection().catch(() => {});
        });
    }

    const showBrightness = () => {
        const sliderPercent = percentFrom255(state.brightnessDraft);
        dom.lightSlider.value = String(sliderPercent);
        dom.lightValue.textContent = String(sliderPercent);
        if (dom.lightModalPercent) {
            dom.lightModalPercent.textContent = `${percentFrom255(state.brightnessLive)}%`;
        }
        setMarker(state.brightnessLive);
    };

    const getBrightness = async () => {
        setStatus('lightStatus', 'Bezig…', 'pending');
        try {
            const value = parseInt(await fetchText('/getBrightness'), 10);
            if (Number.isNaN(value)) {
                throw new Error('bad value');
            }
            state.brightnessLive = value;
            state.brightnessDraft = value;
            showBrightness();
            setStatus('lightStatus', `Niveau ${percentFrom255(value)}%`, 'success');
        } catch (error) {
            setStatus('lightStatus', 'Mislukt', 'error');
        }
    };

    const pushBrightness = async (draftPercent, source) => {
        const value255 = valueFromPercent(draftPercent);
        setStatus('lightStatus', 'Versturen…', 'pending');
        try {
            await fetchText(`/setBrightness?value=${value255}`);
            state.brightnessLive = value255;
            if (source !== 'modal') {
                state.brightnessDraft = value255;
            }
            showBrightness();
            setStatus('lightStatus', `Niveau ${draftPercent}%`, 'success');
        } catch (error) {
            showBrightness();
            setStatus('lightStatus', 'Mislukt', 'error');
        }
    };

    let lightTimer = null;
    dom.lightSlider.addEventListener('input', () => {
        const percent = parseInt(dom.lightSlider.value, 10);
        dom.lightValue.textContent = String(percent);
        clearTimeout(lightTimer);
        lightTimer = window.setTimeout(() => pushBrightness(percent, 'main'), 250);
    });
    dom.lightSlider.addEventListener('change', () => {
        const percent = parseInt(dom.lightSlider.value, 10);
        state.brightnessDraft = valueFromPercent(percent);
        pushBrightness(percent, 'main');
    });

    document.getElementById('lightNext').addEventListener('click', async () => {
        setStatus('lightStatus', 'Volgende show…', 'pending');
        try {
            const response = await fetch('/light/next');
            if (!response.ok) {
                throw new Error(response.statusText);
            }
            const contentType = response.headers.get('content-type') || '';
            if (contentType.includes('application/json')) {
                const payload = await response.json();
                const message = applyActiveLightStatus(payload) || payload.message || 'Aangevraagd';
                setStatus('lightStatus', message, 'success');
            } else {
                const text = (await response.text()).trim();
                setStatus('lightStatus', text || 'Aangevraagd', 'success');
            }
        } catch (error) {
            setStatus('lightStatus', 'Mislukt', 'error');
        }
    });

    const getAudioLevel = async () => {
        setStatus('audioStatus', 'Bezig…', 'pending');
        try {
            const raw = await fetchText('/getWebAudioLevel');
            const level = parseFloat(raw);
            if (!Number.isFinite(level)) {
                throw new Error('bad value');
            }
            state.audioVolume = Math.round(level * 100);
            dom.audioVolume.value = String(state.audioVolume);
            dom.audioVolumeLabel.textContent = `${state.audioVolume}%`;
            setStatus('audioStatus', 'Klaar', 'success');
        } catch (error) {
            setStatus('audioStatus', 'Mislukt', 'error');
        }
    };

    dom.audioVolume.addEventListener('input', () => {
        const value = parseInt(dom.audioVolume.value, 10);
        dom.audioVolumeLabel.textContent = `${value}%`;
    });
    dom.audioVolume.addEventListener('change', async () => {
        const value = parseInt(dom.audioVolume.value, 10);
        setStatus('audioStatus', 'Versturen…', 'pending');
        try {
            await fetchText(`/setWebAudioLevel?value=${(value / 100).toFixed(2)}`);
            state.audioVolume = value;
            setStatus('audioStatus', `Volume ${value}%`, 'success');
        } catch (error) {
            dom.audioVolume.value = String(state.audioVolume);
            dom.audioVolumeLabel.textContent = `${state.audioVolume}%`;
            setStatus('audioStatus', 'Mislukt', 'error');
        }
    });

    const refreshPlaybackInfo = async (options = {}) => {
        const retries = Number.isFinite(options.retries) ? Math.max(1, Math.trunc(options.retries)) : 1;
        const expectChange = options.expectChange === true;
        const waitMs = options.delay && options.delay > 0 ? options.delay : 200;
        const previousKey = `${state.audio.dir}-${state.audio.file}`;

        for (let attempt = 0; attempt < retries; attempt += 1) {
            try {
                const text = await fetchText('/vote?delta=0');
                const match = /dir=(\d+)\s+file=(\d+)/i.exec(text);
                if (match) {
                    const dir = parseInt(match[1], 10);
                    const file = parseInt(match[2], 10);
                    const key = `${dir}-${file}`;
                    if (expectChange && key === previousKey && attempt + 1 < retries) {
                        await sleep(waitMs);
                        continue;
                    }
                    state.audio.dir = dir;
                    state.audio.file = file;
                    dom.audioCurrent.textContent = key;
                    setStatus('audioStatus', 'Klaar', 'success');
                    return key;
                }
            } catch (error) {
                // retry
            }
            if (attempt + 1 < retries) {
                await sleep(waitMs);
            }
        }

        state.audio.dir = 0;
        state.audio.file = 0;
        dom.audioCurrent.textContent = '--';
        setStatus('audioStatus', 'Geen data', 'error');
        return null;
    };

    document.getElementById('audioNextButton').addEventListener('click', async () => {
        setStatus('audioStatus', 'Volgende…', 'pending');
        try {
            const text = await fetchText('/next', { method: 'POST' });
            setStatus('audioStatus', text || 'Aangevraagd', 'success');
            await refreshPlaybackInfo({ retries: 5, delay: 250, expectChange: true });
        } catch (error) {
            setStatus('audioStatus', 'Mislukt', 'error');
        }
    });

    document.querySelectorAll('[data-vote]').forEach((button) => {
        button.addEventListener('click', async () => {
            const vote = button.getAttribute('data-vote');
            let url = '/vote';
            if (vote === 'up') {
                url = `${url}?delta=5`;
            } else if (vote === 'down') {
                url = `${url}?delta=-5`;
            } else {
                url = `${url}?ban=1`;
            }
            setStatus('audioStatus', 'Stemmen…', 'pending');
            try {
                const text = await fetchText(url, { method: 'POST' });
                setStatus('audioStatus', text, 'success');
                await refreshPlaybackInfo({ retries: 3, delay: 200 });
            } catch (error) {
                setStatus('audioStatus', 'Mislukt', 'error');
            }
        });
    });

    const otaAction = async (mode) => {
        const cfg = mode === 'arm'
            ? { url: '/ota/arm', method: 'GET', note: 'Armeren…' }
            : { url: '/ota/confirm', method: 'POST', note: 'Bevestigen…' };
        setStatus('otaStatus', cfg.note, 'pending');
        try {
            const text = await fetchText(cfg.url, { method: cfg.method });
            setStatus('otaStatus', text || 'OK', 'success');
        } catch (error) {
            setStatus('otaStatus', 'Mislukt', 'error');
        }
    };

    document.querySelectorAll('[data-ota]').forEach((button) => {
        button.addEventListener('click', () => {
            const action = button.getAttribute('data-ota');
            if (action) {
                otaAction(action);
            }
        });
    });

    const modals = {
        light: dom.lightModal,
        sd: dom.sdModal,
        ota: dom.otaModal
    };
    let openKey = null;

    const openModal = async (key) => {
        const modal = modals[key];
        if (!modal) {
            return;
        }
        if (key === 'light') {
            state.brightnessDraft = state.brightnessLive;
            state.previewActive = false;
            showBrightness();
            try {
                await Promise.all([ensurePatterns(), ensureColors()]);
            } catch (error) {
                // status reeds gezet
            }
        }
        modal.classList.remove('hidden');
        document.body.classList.add('modal-open');
        openKey = key;
        if (key === 'sd') {
            if (!state.sd.status) {
                setStatus('sdStatus', 'Status ophalen...', 'pending');
            }
            fetchSdStatus({ showPending: !state.sd.status })
                .then(() => {
                    if (state.sd.status && state.sd.status.ready && !state.sd.status.busy) {
                        return fetchSdList(state.sd.path || '/', { showPending: true });
                    }
                    return null;
                })
                .catch(() => {});
        }
    };

    const closeModal = async () => {
        if (!openKey) {
            return;
        }
        if (openKey === 'light') {
            if (state.previewActive) {
                await activateSelection(state.pattern.activeId || PATTERN_CONTEXT, state.color.activeId || COLOR_DEFAULT);
            }
            restoreDrafts();
        }
        modals[openKey].classList.add('hidden');
        document.body.classList.remove('modal-open');
        openKey = null;
    };

    document.querySelectorAll('[data-open-light]').forEach((node) => {
        node.addEventListener('click', () => {
            openModal('light').catch(() => {});
        });
    });
    if (dom.openSdManager) {
        dom.openSdManager.addEventListener('click', () => {
            openModal('sd').catch(() => {});
        });
    }
    document.getElementById('openOtaSettings').addEventListener('click', () => openModal('ota'));
    document.querySelectorAll('[data-dismiss]').forEach((node) => node.addEventListener('click', () => {
        closeModal().catch(() => {});
    }));
    window.addEventListener('keydown', (event) => {
        if (event.key === 'Escape') {
            closeModal().catch(() => {});
        }
    });

    if (dom.sdRefresh) {
        dom.sdRefresh.addEventListener('click', () => {
            const currentPath = state.sd.path || '/';
            fetchSdStatus({ showPending: true })
                .then(() => {
                    if (state.sd.status && state.sd.status.ready) {
                        return fetchSdList(currentPath, { showPending: true });
                    }
                    return null;
                })
                .catch(() => {});
        });
    }
    if (dom.sdUp) {
        dom.sdUp.addEventListener('click', () => {
            if (!state.sd.status || !state.sd.status.ready) {
                return;
            }
            const targetPath = state.sd.parent || '/';
            fetchSdList(targetPath, { showPending: true }).catch(() => {});
        });
    }
    if (dom.sdFile) {
        dom.sdFile.addEventListener('change', () => {
            if (!dom.sdUploadStatus) {
                return;
            }
            const file = dom.sdFile.files && dom.sdFile.files[0];
            dom.sdUploadStatus.textContent = file ? file.name : 'Geen bestand gekozen';
        });
    }
    if (dom.sdEntries) {
        dom.sdEntries.addEventListener('click', (event) => {
            if (!state.sd.status || !state.sd.status.ready) {
                return;
            }
            const navTarget = event.target.closest('.sd-entry-nav');
            if (navTarget) {
                const name = navTarget.dataset.name || '';
                const nextPath = joinSdPath(state.sd.path || '/', name);
                fetchSdList(nextPath, { showPending: true }).catch(() => {});
                return;
            }
            const deleteTarget = event.target.closest('.sd-entry-delete');
            if (deleteTarget) {
                const name = deleteTarget.dataset.name || '';
                const type = deleteTarget.dataset.type || 'file';
                deleteSdEntry(name, type).catch(() => {});
            }
        });
    }
    if (dom.sdUploadForm) {
        dom.sdUploadForm.addEventListener('submit', (event) => {
            event.preventDefault();
            uploadSdFile().catch(() => {});
        });
    }

    const refreshAll = async () => {
        await Promise.all([
            getBrightness(),
            getAudioLevel(),
            refreshPlaybackInfo({ retries: 3, delay: 150 })
        ]);
    };

    document.getElementById('refreshAll').addEventListener('click', refreshAll);

    refreshAll();
})();
