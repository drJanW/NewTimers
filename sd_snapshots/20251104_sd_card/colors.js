(function () {
    'use strict';

    const app = window.kwalApp;
    if (!app) {
        throw new Error('[colors] kwalApp is not initialised');
    }

    const { utils, state } = app;
    const light = app.light;

    const COLOR_DEFAULT = '__default__';
    const MAX_LIGHT_COLORS = 100;

    const colorState = utils.createCollectionState('Kleurset geladen');
    state.color = colorState;

    const moduleApi = {
        COLOR_DEFAULT,
        MAX_LIGHT_COLORS
    };

    const validHex = (value) => typeof value === 'string' && /^#([0-9a-f]{6})$/i.test(value.trim());

    const normalizeColor = (source) => {
        const base = {
            rgb1_hex: '#ff7f00',
            rgb2_hex: '#552200'
        };
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

    const applyColorDraftToUI = (draft) => {
        if (!draft) {
            return;
        }
        const inputs = app.dom.colorInputs || {};
        if (inputs.rgb1_hex) {
            inputs.rgb1_hex.value = draft.rgb1_hex;
        }
        if (inputs.rgb2_hex) {
            inputs.rgb2_hex.value = draft.rgb2_hex;
        }
        colorState.dirty = false;
    };

    const refreshColorControls = () => {
        const select = app.dom.colorSelect;
        if (!select) {
            return;
        }
        const currentId = colorState.selectedId || COLOR_DEFAULT;
        const activeId = colorState.activeId || '';
        select.innerHTML = '';

        const defaultOption = document.createElement('option');
        defaultOption.value = COLOR_DEFAULT;
        defaultOption.textContent = activeId ? 'Standaard kleuren' : 'Standaard kleuren •';
        defaultOption.selected = currentId === COLOR_DEFAULT;
        select.appendChild(defaultOption);

        colorState.items.forEach((entry) => {
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
        const hasColor = !isDefault && colorState.map.has(currentId);

        if (app.dom.colorName) {
            app.dom.colorName.disabled = isDefault;
            if (!colorState.labelDirty) {
                const entry = colorState.map.get(currentId);
                const label = entry && entry.label && entry.label.trim() ? entry.label : entry ? entry.id : '';
                app.dom.colorName.value = isDefault ? '' : (label || '');
            }
        }
        if (app.dom.colorSave) {
            app.dom.colorSave.disabled = isDefault || (!colorState.dirty && !colorState.labelDirty);
        }
        if (app.dom.colorSaveAs) {
            app.dom.colorSaveAs.disabled = colorState.items.length >= MAX_LIGHT_COLORS;
        }
        if (app.dom.colorDelete) {
            app.dom.colorDelete.disabled = isDefault || !hasColor;
        }

        light.updatePreviewState();
        light.updateActivateButton();
    };

    const processColorStore = (store, options = {}) => {
        const colors = Array.isArray(store.colors) ? store.colors.filter((item) => item && item.id) : [];
        colorState.store = store;
        colorState.items = colors;
        colorState.map = new Map(colors.map((item) => [item.id, item]));
        colorState.activeId = typeof store.active_color === 'string' ? store.active_color : '';

        let nextSelected = options.selectedId || colorState.selectedId;
        if (nextSelected && nextSelected !== COLOR_DEFAULT && !colorState.map.has(nextSelected)) {
            nextSelected = '';
        }
        if (!nextSelected) {
            if (colorState.activeId && colorState.map.has(colorState.activeId)) {
                nextSelected = colorState.activeId;
            } else if (colors.length > 0) {
                nextSelected = colors[0].id;
            } else {
                nextSelected = COLOR_DEFAULT;
            }
        }

        colorState.selectedId = nextSelected || COLOR_DEFAULT;
        const source = colorState.selectedId === COLOR_DEFAULT ? null : colorState.map.get(colorState.selectedId);
        colorState.draft = normalizeColor(source);
        colorState.dirty = false;
        colorState.labelDirty = false;
        colorState.originalLabel = source && typeof source.label === 'string' ? source.label.trim() : '';

        applyColorDraftToUI(colorState.draft);

        if (options.statusText && options.skipStatus !== true) {
            colorState.lastLoadStatus = { text: options.statusText, tone: options.statusTone || 'info' };
            light.setSettingsStatus(options.statusText, options.statusTone || 'info');
        }

        refreshColorControls();
    };

    const ensureColors = async () => {
        if (colorState.loaded && colorState.store) {
            processColorStore(colorState.store, {
                selectedId: colorState.selectedId || colorState.activeId || COLOR_DEFAULT,
                skipStatus: true
            });
            return;
        }
        if (!colorState.loadingPromise) {
            colorState.loadingPromise = (async () => {
                try {
                    const response = await fetch('/api/light/colors');
                    if (!response.ok) {
                        throw new Error(response.statusText);
                    }
                    const data = await response.json();
                    colorState.loaded = true;
                    processColorStore(data, { skipStatus: true });
                } catch (error) {
                    colorState.loaded = false;
                    colorState.store = { colors: [], active_color: '' };
                    processColorStore(colorState.store, { skipStatus: true });
                    light.setSettingsStatus('Kleurensets laden mislukt', 'error');
                    throw error;
                } finally {
                    colorState.loadingPromise = null;
                }
            })();
        }
        try {
            await colorState.loadingPromise;
        } catch (error) {
            // status reeds gezet
        }
    };

    const markColorDirty = () => {
        if (!colorState.draft) {
            return;
        }
        colorState.dirty = true;
        light.clearPreview();
        light.setSettingsStatus('Wijzigingen niet opgeslagen', 'pending');
        refreshColorControls();
    };

    const persistColor = async ({ createNew = false } = {}) => {
        if (!colorState.draft) {
            return false;
        }
        if (createNew && colorState.items.length >= MAX_LIGHT_COLORS) {
            light.setSettingsStatus('Maximaal aantal kleurensets bereikt', 'error');
            return false;
        }
        const body = {
            label: app.dom.colorName ? app.dom.colorName.value.trim() : '',
            select: createNew || colorState.selectedId === colorState.activeId,
            rgb1_hex: colorState.draft.rgb1_hex,
            rgb2_hex: colorState.draft.rgb2_hex
        };
        if (!createNew) {
            if (!colorState.selectedId || colorState.selectedId === COLOR_DEFAULT) {
                light.setSettingsStatus('Kies een kleurset om te bewaren', 'error');
                return false;
            }
            body.id = colorState.selectedId;
        }
        light.setSettingsStatus('Kleurset opslaan…', 'pending');
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
            colorState.loaded = true;
            colorState.store = store;
            processColorStore(store, {
                selectedId: headerId || COLOR_DEFAULT,
                skipStatus: true
            });
            light.setSettingsStatus('Kleurset opgeslagen', 'success');
            return true;
        } catch (error) {
            light.setSettingsStatus(error.message || 'Opslaan mislukt', 'error');
            return false;
        }
    };

    const deleteColor = async () => {
        if (!colorState.selectedId || colorState.selectedId === COLOR_DEFAULT) {
            light.setSettingsStatus('Selecteer een kleurset om te verwijderen', 'error');
            return false;
        }
        const entry = colorState.map.get(colorState.selectedId);
        const label = entry ? (entry.label || entry.id) : colorState.selectedId;
        if (!window.confirm(`Kleurset "${label}" verwijderen?`)) {
            return false;
        }
        light.setSettingsStatus('Kleurset verwijderen…', 'pending');
        try {
            const response = await fetch('/api/light/colors/delete', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ id: colorState.selectedId })
            });
            if (!response.ok) {
                const message = await response.text();
                throw new Error(message || response.statusText);
            }
            const store = await response.json();
            const headerId = response.headers.get('X-Light-Color') || store.active_color || '';
            colorState.loaded = true;
            colorState.store = store;
            processColorStore(store, {
                selectedId: headerId || COLOR_DEFAULT,
                skipStatus: true
            });
            light.setSettingsStatus('Kleurset verwijderd', 'info');
            return true;
        } catch (error) {
            light.setSettingsStatus(error.message || 'Verwijderen mislukt', 'error');
            return false;
        }
    };

    const restoreDrafts = () => {
        if (!colorState.store) {
            return;
        }
        processColorStore(colorState.store, {
            selectedId: colorState.activeId || COLOR_DEFAULT,
            skipStatus: true
        });
    };

    const applyActivePayload = (payload) => {
        if (!payload || typeof payload !== 'object') {
            return null;
        }
        const rawColorId = typeof payload.color_id === 'string' ? payload.color_id.trim() : '';
        const colorOverride = payload.color_overridden === true;
        const colorIsDefault = !colorOverride || !rawColorId || rawColorId.toLowerCase() === 'default';

        colorState.lastAppliedId = rawColorId;
        colorState.activeId = colorIsDefault ? '' : rawColorId;

        refreshColorControls();

        const label = (() => {
            if (colorOverride && rawColorId && colorState.map.has(rawColorId)) {
                const entry = colorState.map.get(rawColorId);
                if (entry && typeof entry.label === 'string' && entry.label.trim()) {
                    return entry.label.trim();
                }
                return entry ? entry.id : utils.formatIdLabel(rawColorId);
            }
            if (colorOverride && rawColorId) {
                return utils.formatIdLabel(rawColorId);
            }
            return 'Standaard';
        })();

        return {
            label,
            override: colorOverride
        };
    };

    moduleApi.ensure = () => ensureColors();
    moduleApi.processStore = (store, options) => processColorStore(store, options || {});
    moduleApi.restoreDrafts = restoreDrafts;
    moduleApi.applyActivePayload = applyActivePayload;
    moduleApi.markDirty = markColorDirty;

    app.light.colors = moduleApi;

    app.ready(() => {
        if (app.dom.colorSelect) {
            app.dom.colorSelect.addEventListener('change', () => {
                const value = app.dom.colorSelect.value;
                colorState.selectedId = value;
                light.clearPreview();

                if (value === COLOR_DEFAULT) {
                    colorState.draft = normalizeColor(null);
                    colorState.labelDirty = false;
                    colorState.originalLabel = '';
                    applyColorDraftToUI(colorState.draft);
                    light.setSettingsStatus('Standaard kleuren gekozen', 'info');
                } else {
                    const entry = colorState.map.get(value);
                    if (!entry) {
                        light.setSettingsStatus('Kleurset niet gevonden', 'error');
                        refreshColorControls();
                        return;
                    }
                    colorState.draft = normalizeColor(entry);
                    colorState.originalLabel = entry.label && typeof entry.label === 'string' ? entry.label.trim() : '';
                    colorState.labelDirty = false;
                    applyColorDraftToUI(colorState.draft);
                    light.setSettingsStatus('Kleurset geladen', 'info');
                }
                refreshColorControls();
            });
        }

        if (app.dom.colorName) {
            app.dom.colorName.addEventListener('input', () => {
                if (colorState.selectedId === COLOR_DEFAULT) {
                    app.dom.colorName.value = '';
                    return;
                }
                const value = app.dom.colorName.value.trim();
                const original = (colorState.originalLabel || '').trim();
                colorState.labelDirty = value !== original;
                light.clearPreview();
                refreshColorControls();
            });
        }

        const inputs = app.dom.colorInputs || {};
        Object.values(inputs).forEach((input) => {
            if (!input) {
                return;
            }
            input.addEventListener('input', () => {
                if (!colorState.draft || !validHex(input.value)) {
                    return;
                }
                colorState.draft[input.dataset.color] = input.value;
                markColorDirty();
            });
        });

        if (app.dom.colorSave) {
            app.dom.colorSave.addEventListener('click', () => {
                persistColor({ createNew: false }).catch(() => {});
            });
        }
        if (app.dom.colorSaveAs) {
            app.dom.colorSaveAs.addEventListener('click', () => {
                persistColor({ createNew: true }).catch(() => {});
            });
        }
        if (app.dom.colorDelete) {
            app.dom.colorDelete.addEventListener('click', () => {
                deleteColor().catch(() => {});
            });
        }
    });
})();
