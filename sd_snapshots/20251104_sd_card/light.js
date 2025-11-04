(function () {
    'use strict';

    const app = window.kwalApp;
    if (!app) {
        throw new Error('[light] kwalApp is not initialised');
    }

    const { utils, state } = app;
    const light = app.light;

    const setMarker = (value255) => {
        const marker = app.dom.lightMarker;
        if (!marker) {
            return;
        }
        const clamped = Math.max(0, Math.min(255, value255));
        const ratio = clamped / 255;
        marker.style.left = `${(ratio * 100).toFixed(2)}%`;
    };

    light.setSettingsStatus = (text, tone = 'info') => {
        const node = app.dom.lightSettingsStatus;
        if (!node) {
            return;
        }
        node.className = `status status-${tone}`;
        node.textContent = text;
    };

    const updatePreviewButtonState = () => {
        const previewButton = app.dom.presetPreview;
        if (!previewButton) {
            return;
        }
        const hasDrafts = Boolean(state.pattern && state.pattern.draft && state.color && state.color.draft);
        previewButton.disabled = !hasDrafts;
        previewButton.textContent = state.previewActive ? 'Voorbeeld actief' : 'Voorbeeld';
    };

    light.updatePreviewState = updatePreviewButtonState;

    const updateActivateButtonState = () => {
        const activateButton = app.dom.presetActivate;
        if (!activateButton || !state.pattern || !state.color) {
            return;
        }
        const patterns = app.light.patterns || {};
        const colors = app.light.colors || {};
        const patternContext = patterns.PATTERN_CONTEXT || '__context__';
        const colorDefault = colors.COLOR_DEFAULT || '__default__';

        const selectedPattern = state.pattern.selectedId || patternContext;
        const activePattern = state.pattern.activeId || patternContext;
        const selectedColor = state.color.selectedId || colorDefault;
        const activeColor = state.color.activeId || colorDefault;

        const patternMatches = selectedPattern === activePattern;
        const colorMatches = selectedColor === activeColor;
        const anyDirty = Boolean(
            (state.pattern.dirty || state.pattern.labelDirty || 0) ||
            (state.color.dirty || state.color.labelDirty || 0)
        );

        if (selectedPattern === patternContext) {
            activateButton.textContent = activePattern === patternContext ? 'Context actief' : 'Activeer context';
            activateButton.disabled = activePattern === patternContext && !anyDirty && selectedColor === activeColor;
        } else {
            activateButton.textContent = 'Activeer';
            activateButton.disabled = patternMatches && colorMatches && !anyDirty;
        }
    };

    light.updateActivateButton = updateActivateButtonState;

    light.clearPreview = () => {
        state.previewActive = false;
        updatePreviewButtonState();
    };

    const showBrightness = () => {
        const slider = app.dom.lightSlider;
        const sliderLabel = app.dom.lightValue;
        const modalLabel = app.dom.lightModalPercent;
        if (slider && sliderLabel) {
            const draftPercent = utils.percentFrom255(state.brightnessDraft);
            slider.value = String(draftPercent);
            sliderLabel.textContent = String(draftPercent);
        }
        if (modalLabel) {
            modalLabel.textContent = `${utils.percentFrom255(state.brightnessLive)}%`;
        }
        setMarker(state.brightnessLive);
    };

    light.showBrightness = showBrightness;

    const getBrightness = async () => {
        utils.setStatus('lightStatus', 'Bezig…', 'pending');
        try {
            const raw = await utils.fetchText('/getBrightness');
            const value = parseInt(raw, 10);
            if (Number.isNaN(value)) {
                throw new Error('bad value');
            }
            state.brightnessLive = value;
            state.brightnessDraft = value;
            showBrightness();
            utils.setStatus('lightStatus', `Niveau ${utils.percentFrom255(value)}%`, 'success');
        } catch (error) {
            utils.setStatus('lightStatus', 'Mislukt', 'error');
        }
    };

    const pushBrightness = async (percent, source) => {
        const value255 = utils.valueFromPercent(percent);
        utils.setStatus('lightStatus', 'Versturen…', 'pending');
        try {
            await utils.fetchText(`/setBrightness?value=${value255}`);
            state.brightnessLive = value255;
            if (source !== 'modal') {
                state.brightnessDraft = value255;
            }
            showBrightness();
            utils.setStatus('lightStatus', `Niveau ${percent}%`, 'success');
        } catch (error) {
            showBrightness();
            utils.setStatus('lightStatus', 'Mislukt', 'error');
        }
    };

    const restoreDrafts = () => {
        if (app.light.patterns && typeof app.light.patterns.restoreDrafts === 'function') {
            app.light.patterns.restoreDrafts();
        }
        if (app.light.colors && typeof app.light.colors.restoreDrafts === 'function') {
            app.light.colors.restoreDrafts();
        }
        state.previewActive = false;
        updatePreviewButtonState();
        updateActivateButtonState();
    };

    light.restoreDrafts = restoreDrafts;

    const applyActiveLightStatus = (payload) => {
        const patternInfo = app.light.patterns && typeof app.light.patterns.applyActivePayload === 'function'
            ? app.light.patterns.applyActivePayload(payload)
            : null;
        const colorInfo = app.light.colors && typeof app.light.colors.applyActivePayload === 'function'
            ? app.light.colors.applyActivePayload(payload)
            : null;

        state.previewActive = false;
        updatePreviewButtonState();
        updateActivateButtonState();

        if (!patternInfo) {
            return '';
        }

        const { label, manual, source } = patternInfo;
        let message;
        if (manual || source === 'manual') {
            message = `Patroon ${label} actief`;
        } else if (source === 'calendar') {
            message = `Kalenderpatroon ${label}`;
        } else if (source === 'default') {
            message = `Standaardpatroon ${label}`;
        } else if (label === 'Context') {
            message = 'Context actief';
        } else {
            message = `Context ${label}`;
        }

        if (colorInfo && colorInfo.override) {
            message += ` • kleuren ${colorInfo.label}`;
        } else {
            message += ' • standaardkleuren';
        }

        return message;
    };

    light.applyActiveLightStatus = applyActiveLightStatus;

    const previewSelection = async () => {
        if (!state.pattern || !state.color || !state.pattern.draft || !state.color.draft) {
            return false;
        }
        const patterns = app.light.patterns || {};
        const colors = app.light.colors || {};
        const patternContext = patterns.PATTERN_CONTEXT || '__context__';
        const colorDefault = colors.COLOR_DEFAULT || '__default__';

        const selectedPattern = state.pattern.selectedId || '';
        const activePattern = state.pattern.activeId || '';
        const lastPattern = state.pattern.lastAppliedId || '';
        const selectedColor = state.color.selectedId || '';
        const activeColor = state.color.activeId || '';
        const lastColor = state.color.lastAppliedId || '';

        const patternIdForPreview = (() => {
            if (selectedPattern && selectedPattern !== patternContext) {
                return selectedPattern;
            }
            if (activePattern && activePattern !== patternContext) {
                return activePattern;
            }
            if (lastPattern) {
                return lastPattern;
            }
            return '';
        })();

        const colorIdForPreview = (() => {
            if (selectedColor && selectedColor !== colorDefault) {
                return selectedColor;
            }
            if (activeColor && activeColor !== colorDefault) {
                return activeColor;
            }
            if (lastColor) {
                return lastColor;
            }
            return '';
        })();

        const body = {
            pattern: {
                params: utils.deepClone(state.pattern.draft.params)
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
            body: utils.deepClone(body)
        };

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
            light.setSettingsStatus('Geen patroon beschikbaar voor voorbeeld', 'error');
            light.clearPreview();
            return false;
        }

        const previewPatternLabel = patternIdForPreview || '(onbekend)';
        const previewColorLabel = colorIdForPreview || 'default';
        light.setSettingsStatus(`Voorbeeld: ${previewPatternLabel} • kleur ${previewColorLabel}`, 'pending');

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
            updatePreviewButtonState();
            light.setSettingsStatus('Voorbeeld actief (niet opgeslagen)', 'info');
            return true;
        } catch (error) {
            light.clearPreview();
            light.setSettingsStatus(error.message || 'Voorbeeld mislukt', 'error');
            return false;
        }
    };

    light.requestPreview = () => previewSelection();

    const activateSelection = async (patternIdInput, colorIdInput) => {
        if (!app.light.patterns || !app.light.colors || !state.pattern || !state.color) {
            return false;
        }
        const patternContext = app.light.patterns.PATTERN_CONTEXT;
        const colorDefault = app.light.colors.COLOR_DEFAULT;

        const selectedPattern = typeof patternIdInput === 'string' ? patternIdInput : (state.pattern.selectedId || patternContext);
        const selectedColor = typeof colorIdInput === 'string' ? colorIdInput : (state.color.selectedId || colorDefault);

        const patternPayload = { id: selectedPattern === patternContext ? '' : selectedPattern };
        const colorPayload = { id: selectedColor === colorDefault ? '' : selectedColor };

        light.setSettingsStatus('Activeren…', 'pending');
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
            if (typeof app.light.patterns.processStore === 'function') {
                app.light.patterns.processStore(patternStore, {
                    selectedId: patternPayload.id || patternContext,
                    statusText: patternPayload.id ? 'Patroon actief' : 'Context actief',
                    statusTone: patternPayload.id ? 'success' : 'info'
                });
            }

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
            if (typeof app.light.colors.processStore === 'function') {
                app.light.colors.processStore(colorStore, {
                    selectedId: colorPayload.id || colorDefault,
                    skipStatus: true
                });
            }

            state.previewActive = false;
            updatePreviewButtonState();
            updateActivateButtonState();
            return true;
        } catch (error) {
            light.setSettingsStatus(error.message || 'Activeren mislukt', 'error');
            return false;
        }
    };

    light.requestActivation = (patternId, colorId) => activateSelection(patternId, colorId);

    light.refreshAll = async () => {
        const tasks = [getBrightness()];
        if (app.audio && typeof app.audio.getLevel === 'function') {
            tasks.push(app.audio.getLevel());
        }
        if (app.audio && typeof app.audio.refreshPlaybackInfo === 'function') {
            tasks.push(app.audio.refreshPlaybackInfo({ retries: 3, delay: 150 }));
        }
        await Promise.all(tasks);
    };

    app.ready(() => {
        const {
            lightSlider,
            lightValue,
            lightModalPercent,
            lightNext,
            refreshAllButton,
            presetPreview,
            presetActivate,
            lightOpenButtons,
            modalDismissButtons,
            lightModal,
            otaModal,
            openOtaSettings,
            openSdManager,
            sdModal
        } = app.dom;

        if (lightModalPercent) {
            lightModalPercent.textContent = `${utils.percentFrom255(state.brightnessLive)}%`;
        }

        let sliderTimer = null;
        if (lightSlider && lightValue) {
            lightSlider.addEventListener('input', () => {
                const percent = parseInt(lightSlider.value, 10);
                lightValue.textContent = String(percent);
                clearTimeout(sliderTimer);
                sliderTimer = window.setTimeout(() => {
                    pushBrightness(percent, 'main').catch(() => {});
                }, 250);
            });
            lightSlider.addEventListener('change', () => {
                const percent = parseInt(lightSlider.value, 10);
                state.brightnessDraft = utils.valueFromPercent(percent);
                pushBrightness(percent, 'main').catch(() => {});
            });
        }

        if (lightNext) {
            lightNext.addEventListener('click', async () => {
                utils.setStatus('lightStatus', 'Volgende show…', 'pending');
                try {
                    const response = await fetch('/light/next');
                    if (!response.ok) {
                        throw new Error(response.statusText);
                    }
                    const contentType = response.headers.get('content-type') || '';
                    if (contentType.includes('application/json')) {
                        const payload = await response.json();
                        const message = applyActiveLightStatus(payload) || payload.message || 'Aangevraagd';
                        utils.setStatus('lightStatus', message, 'success');
                    } else {
                        const text = (await response.text()).trim();
                        utils.setStatus('lightStatus', text || 'Aangevraagd', 'success');
                    }
                } catch (error) {
                    utils.setStatus('lightStatus', 'Mislukt', 'error');
                }
            });
        }

        if (presetPreview) {
            presetPreview.addEventListener('click', () => {
                previewSelection().catch(() => {});
            });
        }
        if (presetActivate) {
            presetActivate.addEventListener('click', () => {
                activateSelection().catch(() => {});
            });
        }

        if (refreshAllButton) {
            refreshAllButton.addEventListener('click', () => {
                light.refreshAll().catch(() => {});
            });
        }

        const modals = {
            light: lightModal,
            ota: otaModal,
            sd: sdModal
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
                updatePreviewButtonState();
                try {
                    const patternEnsure = app.light.patterns && typeof app.light.patterns.ensure === 'function'
                        ? app.light.patterns.ensure()
                        : Promise.resolve();
                    const colorEnsure = app.light.colors && typeof app.light.colors.ensure === 'function'
                        ? app.light.colors.ensure()
                        : Promise.resolve();
                    await Promise.all([patternEnsure, colorEnsure]);
                } catch (error) {
                    // status al bijgewerkt door onderliggende modules
                }
            }
            if (key === 'sd' && app.sd && typeof app.sd.handleOpen === 'function') {
                app.sd.handleOpen();
            }
            modal.classList.remove('hidden');
            document.body.classList.add('modal-open');
            openKey = key;
        };

        const closeModal = async () => {
            if (!openKey) {
                return;
            }
            if (openKey === 'light') {
                const patterns = app.light.patterns || {};
                const colors = app.light.colors || {};
                const patternContext = patterns.PATTERN_CONTEXT || '__context__';
                const colorDefault = colors.COLOR_DEFAULT || '__default__';
                if (state.previewActive) {
                    await activateSelection(
                        state.pattern ? state.pattern.activeId || patternContext : patternContext,
                        state.color ? state.color.activeId || colorDefault : colorDefault
                    ).catch(() => {});
                }
                restoreDrafts();
            }
            if (openKey === 'sd' && app.sd && typeof app.sd.handleClose === 'function') {
                app.sd.handleClose();
            }
            const modal = modals[openKey];
            if (modal) {
                modal.classList.add('hidden');
            }
            document.body.classList.remove('modal-open');
            openKey = null;
        };

        light.openModal = openModal;
        light.closeModal = closeModal;

        lightOpenButtons.forEach((node) => {
            node.addEventListener('click', () => {
                openModal('light').catch(() => {});
            });
        });

        if (openOtaSettings) {
            openOtaSettings.addEventListener('click', () => {
                openModal('ota').catch(() => {});
            });
        }

        if (openSdManager) {
            openSdManager.addEventListener('click', () => {
                openModal('sd').catch(() => {});
            });
        }

        modalDismissButtons.forEach((node) => {
            node.addEventListener('click', () => {
                closeModal().catch(() => {});
            });
        });

        window.addEventListener('keydown', (event) => {
            if (event.key === 'Escape') {
                closeModal().catch(() => {});
            }
        });
    });

    app.onStart(() => {
        light.refreshAll().catch(() => {});
    });
})();
