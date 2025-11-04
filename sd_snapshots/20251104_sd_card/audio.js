(function () {
    'use strict';

    const app = window.kwalApp;
    if (!app) {
        throw new Error('[audio] kwalApp is not initialised');
    }

    const { utils, state } = app;
    const audioModule = app.audio;

    const getAudioLevel = async () => {
        utils.setStatus('audioStatus', 'Bezig…', 'pending');
        try {
            const raw = await utils.fetchText('/getWebAudioLevel');
            const level = parseFloat(raw);
            if (!Number.isFinite(level)) {
                throw new Error('bad value');
            }
            state.audioVolume = Math.round(level * 100);
            if (app.dom.audioVolume) {
                app.dom.audioVolume.value = String(state.audioVolume);
            }
            if (app.dom.audioVolumeLabel) {
                app.dom.audioVolumeLabel.textContent = `${state.audioVolume}%`;
            }
            utils.setStatus('audioStatus', 'Klaar', 'success');
        } catch (error) {
            utils.setStatus('audioStatus', 'Mislukt', 'error');
        }
    };

    const refreshPlaybackInfo = async (options = {}) => {
        const retries = Number.isFinite(options.retries) ? Math.max(1, Math.trunc(options.retries)) : 1;
        const expectChange = options.expectChange === true;
        const waitMs = options.delay && options.delay > 0 ? options.delay : 200;
        const previousKey = `${state.audio.dir}-${state.audio.file}`;

        for (let attempt = 0; attempt < retries; attempt += 1) {
            try {
                const text = await utils.fetchText('/vote?delta=0');
                const match = /dir=(\d+)\s+file=(\d+)/i.exec(text);
                if (match) {
                    const dir = parseInt(match[1], 10);
                    const file = parseInt(match[2], 10);
                    const key = `${dir}-${file}`;
                    if (expectChange && key === previousKey && attempt + 1 < retries) {
                        await utils.sleep(waitMs);
                        continue;
                    }
                    state.audio.dir = dir;
                    state.audio.file = file;
                    if (app.dom.audioCurrent) {
                        app.dom.audioCurrent.textContent = key;
                    }
                    utils.setStatus('audioStatus', 'Klaar', 'success');
                    return key;
                }
            } catch (error) {
                // retry
            }
            if (attempt + 1 < retries) {
                await utils.sleep(waitMs);
            }
        }

        state.audio.dir = 0;
        state.audio.file = 0;
        if (app.dom.audioCurrent) {
            app.dom.audioCurrent.textContent = '--';
        }
        utils.setStatus('audioStatus', 'Geen data', 'error');
        return null;
    };

    const otaAction = async (mode) => {
        const cfg = mode === 'arm'
            ? { url: '/ota/arm', method: 'GET', note: 'Armeren…' }
            : { url: '/ota/confirm', method: 'POST', note: 'Bevestigen…' };
        utils.setStatus('otaStatus', cfg.note, 'pending');
        try {
            const text = await utils.fetchText(cfg.url, { method: cfg.method });
            utils.setStatus('otaStatus', text || 'OK', 'success');
        } catch (error) {
            utils.setStatus('otaStatus', 'Mislukt', 'error');
        }
    };

    audioModule.getLevel = () => getAudioLevel();
    audioModule.refreshPlaybackInfo = (options) => refreshPlaybackInfo(options);
    audioModule.otaAction = otaAction;

    app.ready(() => {
        if (app.dom.audioVolume && app.dom.audioVolumeLabel) {
            app.dom.audioVolume.addEventListener('input', () => {
                const value = parseInt(app.dom.audioVolume.value, 10);
                app.dom.audioVolumeLabel.textContent = `${value}%`;
            });
            app.dom.audioVolume.addEventListener('change', async () => {
                const value = parseInt(app.dom.audioVolume.value, 10);
                utils.setStatus('audioStatus', 'Versturen…', 'pending');
                try {
                    await utils.fetchText(`/setWebAudioLevel?value=${(value / 100).toFixed(2)}`);
                    state.audioVolume = value;
                    utils.setStatus('audioStatus', `Volume ${value}%`, 'success');
                } catch (error) {
                    app.dom.audioVolume.value = String(state.audioVolume);
                    app.dom.audioVolumeLabel.textContent = `${state.audioVolume}%`;
                    utils.setStatus('audioStatus', 'Mislukt', 'error');
                }
            });
        }

        if (app.dom.audioNextButton) {
            app.dom.audioNextButton.addEventListener('click', async () => {
                utils.setStatus('audioStatus', 'Volgende…', 'pending');
                try {
                    const text = await utils.fetchText('/next', { method: 'POST' });
                    utils.setStatus('audioStatus', text || 'Aangevraagd', 'success');
                    await refreshPlaybackInfo({ retries: 5, delay: 250, expectChange: true });
                } catch (error) {
                    utils.setStatus('audioStatus', 'Mislukt', 'error');
                }
            });
        }

        if (Array.isArray(app.dom.audioVoteButtons)) {
            app.dom.audioVoteButtons.forEach((button) => {
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
                    utils.setStatus('audioStatus', 'Stemmen…', 'pending');
                    try {
                        const text = await utils.fetchText(url, { method: 'POST' });
                        utils.setStatus('audioStatus', text, 'success');
                        await refreshPlaybackInfo({ retries: 3, delay: 200 });
                    } catch (error) {
                        utils.setStatus('audioStatus', 'Mislukt', 'error');
                    }
                });
            });
        }

        if (Array.isArray(app.dom.otaButtons)) {
            app.dom.otaButtons.forEach((button) => {
                button.addEventListener('click', () => {
                    const action = button.getAttribute('data-ota');
                    if (action) {
                        otaAction(action);
                    }
                });
            });
        }
    });
})();
