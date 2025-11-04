(function () {
    'use strict';

    const app = window.kwalApp;
    if (!app) {
        throw new Error('[sd] kwalApp is not initialised');
    }

    const { utils } = app;
    const sd = app.sd = app.sd || {};

    const state = {
        currentPath: '/',
        entries: [],
        loading: false,
        loaded: false,
        uploading: false
    };

    let loadToken = 0;

    const getDom = () => app.dom || {};

    const setStatus = (text, tone = 'info') => {
        utils.setStatus('sdStatus', text, tone);
    };

    const setUploadStatus = (text, tone = 'info') => {
        const { sdUploadStatus } = getDom();
        if (!sdUploadStatus) {
            return;
        }
        sdUploadStatus.textContent = text;
        sdUploadStatus.className = `status status-${tone}`;
    };

    const formatSize = (value) => {
        if (typeof value !== 'number' || !Number.isFinite(value)) {
            return '-';
        }
        if (value < 1024) {
            return `${value} B`;
        }
        if (value < 1024 * 1024) {
            return `${(value / 1024).toFixed(1)} KB`;
        }
        return `${(value / (1024 * 1024)).toFixed(1)} MB`;
    };

    const parentPath = (value) => {
        if (!value || value === '/') {
            return '/';
        }
        let path = value;
        while (path.length > 1 && path.endsWith('/')) {
            path = path.slice(0, -1);
        }
        const index = path.lastIndexOf('/');
        if (index <= 0) {
            return '/';
        }
        return path.substring(0, index);
    };

    const updatePathField = () => {
        const { sdPath } = getDom();
        if (sdPath) {
            sdPath.value = state.currentPath;
        }
    };

    const updateControls = () => {
        const { sdRefresh, sdUp, sdUploadButton, sdFile } = getDom();
        if (sdRefresh) {
            sdRefresh.disabled = state.loading;
        }
        if (sdUp) {
            sdUp.disabled = state.loading || state.currentPath === '/';
        }
        if (sdUploadButton) {
            sdUploadButton.disabled = state.loading || state.uploading;
        }
        if (sdFile) {
            sdFile.disabled = state.uploading;
        }
    };

    const renderEntries = () => {
        const { sdEntries } = getDom();
        if (!sdEntries) {
            return;
        }
        sdEntries.innerHTML = '';

        if (state.loading) {
            const row = document.createElement('tr');
            const cell = document.createElement('td');
            cell.colSpan = 4;
            cell.className = 'sd-empty';
            cell.textContent = 'Laden…';
            row.appendChild(cell);
            sdEntries.appendChild(row);
            return;
        }

        if (!Array.isArray(state.entries) || state.entries.length === 0) {
            const row = document.createElement('tr');
            const cell = document.createElement('td');
            cell.colSpan = 4;
            cell.className = 'sd-empty';
            cell.textContent = 'Geen bestanden in deze map';
            row.appendChild(cell);
            sdEntries.appendChild(row);
            return;
        }

        const createActionButton = (label, action, path) => {
            const button = document.createElement('button');
            button.type = 'button';
            button.className = 'btn btn-small';
            button.dataset.sdAction = action;
            button.dataset.sdPath = path;
            button.textContent = label;
            return button;
        };

        state.entries.forEach((entry) => {
            const tr = document.createElement('tr');

            const nameCell = document.createElement('td');
            const nameButton = document.createElement('button');
            nameButton.type = 'button';
            nameButton.className = 'sd-name-btn';
            nameButton.dataset.sdPath = entry.path;
            nameButton.dataset.sdAction = entry.type === 'dir' ? 'open' : 'download';

            const icon = document.createElement('span');
            icon.className = 'sd-icon';
            icon.textContent = entry.type === 'dir' ? '📁' : '📄';

            const text = document.createElement('span');
            text.textContent = entry.name || '(naamloos)';

            nameButton.appendChild(icon);
            nameButton.appendChild(text);
            nameCell.appendChild(nameButton);
            tr.appendChild(nameCell);

            const typeCell = document.createElement('td');
            typeCell.className = 'sd-type';
            typeCell.textContent = entry.type === 'dir' ? 'Map' : 'Bestand';
            tr.appendChild(typeCell);

            const sizeCell = document.createElement('td');
            sizeCell.className = 'sd-size';
            sizeCell.textContent = entry.type === 'dir' ? '—' : formatSize(entry.size);
            tr.appendChild(sizeCell);

            const actionsCell = document.createElement('td');
            actionsCell.className = 'sd-actions';
            if (entry.type === 'dir') {
                actionsCell.appendChild(createActionButton('Open', 'open', entry.path));
            } else {
                actionsCell.appendChild(createActionButton('Download', 'download', entry.path));
                actionsCell.appendChild(createActionButton('Verwijder', 'delete', entry.path));
            }
            tr.appendChild(actionsCell);

            sdEntries.appendChild(tr);
        });
    };

    const setLoading = (flag) => {
        state.loading = flag;
        updateControls();
        renderEntries();
    };

    const setUploading = (flag) => {
        state.uploading = flag;
        updateControls();
    };

    const readErrorMessage = async (response) => {
        try {
            const contentType = response.headers.get('content-type') || '';
            if (contentType.includes('application/json')) {
                const json = await response.clone().json();
                if (json && typeof json.error === 'string' && json.error.length > 0) {
                    return json.error;
                }
            }
        } catch (error) {
            // ignore parse errors
        }
        try {
            const text = await response.text();
            if (text && text.trim().length > 0) {
                return text.trim();
            }
        } catch (error) {
            // ignore read errors
        }
        return response.statusText || 'Onbekende fout';
    };

    const loadDirectory = async (target) => {
        const path = typeof target === 'string' && target.length > 0 ? target : state.currentPath;
        const token = ++loadToken;
        setLoading(true);
        setStatus('Laden…', 'pending');
        try {
            const response = await fetch(`/api/sd/list?path=${encodeURIComponent(path)}`);
            if (!response.ok) {
                const message = await readErrorMessage(response);
                throw new Error(message);
            }
            const payload = await response.json();
            if (token !== loadToken) {
                return;
            }
            state.currentPath = typeof payload.path === 'string' && payload.path.length > 0 ? payload.path : path;
            state.entries = Array.isArray(payload.entries) ? payload.entries : [];
            state.loaded = true;
            updatePathField();
            setStatus(`Map ${state.currentPath}`, 'success');
        } catch (error) {
            if (token === loadToken) {
                setStatus(error.message || 'Laden mislukt', 'error');
            }
        } finally {
            if (token === loadToken) {
                setLoading(false);
                updatePathField();
            }
        }
    };

    const handleDownload = (path) => {
        if (!path) {
            return;
        }
        const url = `/api/sd/download?path=${encodeURIComponent(path)}`;
        window.open(url, '_blank');
        setStatus('Download gestart', 'info');
    };

    const handleDelete = async (path) => {
        if (!path) {
            return;
        }
        const confirmMessage = `Verwijder bestand\n${path}?`;
        if (!window.confirm(confirmMessage)) {
            return;
        }
        setStatus('Verwijderen…', 'pending');
        try {
            const response = await fetch(`/api/sd/delete?path=${encodeURIComponent(path)}`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ path })
            });
            if (!response.ok) {
                const message = await readErrorMessage(response);
                throw new Error(message);
            }
            setStatus('Bestand verwijderd', 'success');
            await loadDirectory(state.currentPath);
        } catch (error) {
            setStatus(error.message || 'Verwijderen mislukt', 'error');
        }
    };

    const handleEntryAction = (event) => {
        const button = event.target.closest('button[data-sd-action]');
        if (!button) {
            return;
        }
        const action = button.dataset.sdAction;
        const path = button.dataset.sdPath;
        if (!action) {
            return;
        }
        if (action === 'open') {
            loadDirectory(path).catch(() => { });
        } else if (action === 'download') {
            handleDownload(path);
        } else if (action === 'delete') {
            handleDelete(path).catch(() => { });
        }
    };

    const handleUpload = async (event) => {
        event.preventDefault();
        const { sdFile } = getDom();
        if (!sdFile || !sdFile.files || sdFile.files.length === 0) {
            setUploadStatus('Geen bestand geselecteerd', 'error');
            return;
        }
        const file = sdFile.files[0];
        const formData = new FormData();
        formData.append('file', file, file.name);
        formData.append('path', state.currentPath);

        setUploadStatus('Uploaden…', 'pending');
        setStatus('Uploaden…', 'pending');
        setUploading(true);
        try {
            const response = await fetch('/api/sd/upload', {
                method: 'POST',
                body: formData
            });
            if (!response.ok) {
                const message = await readErrorMessage(response);
                throw new Error(message);
            }
            await response.json().catch(() => ({}));
            setUploadStatus('Upload voltooid', 'success');
            setStatus('Upload voltooid', 'success');
            sdFile.value = '';
            await loadDirectory(state.currentPath);
        } catch (error) {
            setUploadStatus(error.message || 'Upload mislukt', 'error');
            setStatus(error.message || 'Upload mislukt', 'error');
        } finally {
            setUploading(false);
        }
    };

    const handleFileChange = () => {
        const { sdFile } = getDom();
        if (!sdFile) {
            return;
        }
        if (!sdFile.files || sdFile.files.length === 0) {
            setUploadStatus('Geen bestand gekozen', 'info');
            return;
        }
        const file = sdFile.files[0];
        setUploadStatus(`Gekozen: ${file.name}`, 'info');
    };

    sd.handleOpen = () => {
        updatePathField();
        updateControls();
        renderEntries();
        if (!state.loaded) {
            loadDirectory(state.currentPath).catch(() => { });
        }
    };

    sd.handleClose = () => {
        const { sdFile } = getDom();
        if (sdFile) {
            sdFile.value = '';
        }
        setUploadStatus('Geen bestand gekozen', 'info');
    };

    sd.refresh = () => loadDirectory(state.currentPath);

    app.ready(() => {
        const {
            sdRefresh,
            sdUp,
            sdEntries,
            sdUploadForm,
            sdFile
        } = getDom();

        if (sdRefresh) {
            sdRefresh.addEventListener('click', () => {
                loadDirectory(state.currentPath).catch(() => { });
            });
        }

        if (sdUp) {
            sdUp.addEventListener('click', () => {
                const parent = parentPath(state.currentPath);
                loadDirectory(parent).catch(() => { });
            });
        }

        if (sdEntries) {
            sdEntries.addEventListener('click', handleEntryAction);
        }

        if (sdUploadForm) {
            sdUploadForm.addEventListener('submit', handleUpload);
        }

        if (sdFile) {
            sdFile.addEventListener('change', handleFileChange);
        }

        setUploadStatus('Geen bestand gekozen', 'info');
        updatePathField();
        updateControls();
        renderEntries();
    });
})();
