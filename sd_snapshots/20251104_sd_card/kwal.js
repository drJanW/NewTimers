(function () {
    'use strict';

    const readyQueue = [];
    const startQueue = [];

    const fetchText = async (url, options = {}) => {
        const response = await fetch(url, options);
        if (!response.ok) {
            throw new Error(response.statusText);
        }
        return (await response.text()).trim();
    };

    const sleep = (ms) => new Promise((resolve) => window.setTimeout(resolve, ms));

    const clamp = (value, min, max) => Math.min(max, Math.max(min, value));

    const deepClone = (value) => JSON.parse(JSON.stringify(value));

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

    const percentFrom255 = (value) => Math.round((value / 255) * 100);

    const valueFromPercent = (value) => Math.round((value / 100) * 255);

    const formatIdLabel = (id) => {
        if (typeof id !== 'string' || id.length === 0) {
            return '';
        }
        return id.replace(/_/g, ' ').replace(/\b\w/g, (ch) => ch.toUpperCase());
    };

    const setStatus = (id, text, tone = 'info') => {
        const node = document.getElementById(id);
        if (!node) {
            return;
        }
        node.className = `status status-${tone}`;
        node.textContent = text;
    };

    const app = {
        utils: {
            fetchText,
            sleep,
            clamp,
            deepClone,
            createCollectionState,
            percentFrom255,
            valueFromPercent,
            formatIdLabel,
            setStatus
        },
        state: {
            brightnessLive: 0,
            brightnessDraft: 0,
            audioVolume: 0,
            audio: { dir: 0, file: 0 },
            previewActive: false
        },
        dom: {},
        ready(fn) {
            if (typeof fn === 'function') {
                readyQueue.push(fn);
            }
        },
        onStart(fn) {
            if (typeof fn === 'function') {
                startQueue.push(fn);
            }
        },
        collectDom() {
            this.dom = {
                audioStatus: document.getElementById('audioStatus'),
                audioCurrent: document.getElementById('audioCurrent'),
                audioVolume: document.getElementById('audioVolume'),
                audioVolumeLabel: document.getElementById('audioVolumeLabel'),
                audioNextButton: document.getElementById('audioNextButton'),
                audioVoteButtons: Array.from(document.querySelectorAll('[data-vote]')),
                lightStatus: document.getElementById('lightStatus'),
                lightSlider: document.getElementById('lightSlider'),
                lightValue: document.getElementById('lightValue'),
                lightModalPercent: document.getElementById('lightModalPercent'),
                lightNext: document.getElementById('lightNext'),
                lightOpenButtons: Array.from(document.querySelectorAll('[data-open-light]')),
                lightSettingsStatus: document.getElementById('lightSettingsStatus'),
                lightSettingsList: document.getElementById('lightSettingsList'),
                lightMarker: document.querySelector('[data-marker="light"]'),
                presetPreview: document.getElementById('presetPreview'),
                presetActivate: document.getElementById('presetActivate'),
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
                colorInputs: {
                    rgb1_hex: document.querySelector('input[data-color="rgb1_hex"]'),
                    rgb2_hex: document.querySelector('input[data-color="rgb2_hex"]')
                },
                modalDismissButtons: Array.from(document.querySelectorAll('[data-dismiss]')),
                refreshAllButton: document.getElementById('refreshAll'),
                openOtaSettings: document.getElementById('openOtaSettings'),
                otaButtons: Array.from(document.querySelectorAll('[data-ota]')),
                otaStatus: document.getElementById('otaStatus'),
                openSdManager: document.getElementById('openSdManager'),
                sdStatus: document.getElementById('sdStatus'),
                sdPath: document.getElementById('sdPath'),
                sdEntries: document.getElementById('sdEntries'),
                sdRefresh: document.getElementById('sdRefresh'),
                sdUp: document.getElementById('sdUp'),
                sdUploadForm: document.getElementById('sdUploadForm'),
                sdFile: document.getElementById('sdFile'),
                sdUploadStatus: document.getElementById('sdUploadStatus'),
                sdUploadButton: document.getElementById('sdUploadButton'),
                lightModal: document.getElementById('lightModal'),
                otaModal: document.getElementById('otaModal'),
                sdModal: document.getElementById('sdModal')
            };
        },
        boot() {
            this.collectDom();
            readyQueue.forEach((fn) => {
                try {
                    fn(this);
                } catch (error) {
                    console.error('[kwal.boot] ready hook failed', error);
                }
            });
            startQueue.forEach((fn) => {
                try {
                    fn(this);
                } catch (error) {
                    console.error('[kwal.boot] start hook failed', error);
                }
            });
        },
        light: {},
        audio: {}
    };

    window.APP_BUILD_INFO = Object.freeze({
    version: 'web-split-110308D',
        features: ['modularScripts', 'previewFallback', 'lastAppliedTracking', 'sdManager']
    });

    window.kwalApp = app;
})();
