/* Hardware Test tab — fabrication check of the receiver link, the GNSS fix
   and the geofence LED / relay output. */
(function () {
    'use strict';

    const GEOFENCE_RADIUS_M = 300;
    // No confidence required: the relay is under test, not the accuracy.
    const GEOFENCE_CONFIDENCE_LEVEL = 0;
    const PIN_POLARITY_LOW_MEANS_INSIDE = 0;
    const INSIDE_TIMEOUT_MS = 60000;

    const RELAY_HINT_ID = 'hwtest-relay-msg';

    // idle → applying → waiting → passed | failed
    let relayState = 'idle';
    let fixPvt = null;      // latest PVT, only while the receiver has a fix
    let insideTimer = null;
    let relayHint = '';

    function setText(id, value) {
        const el = document.getElementById(id);
        if (el) el.textContent = value;
    }

    function setBadge(id, state, text) {
        const el = document.getElementById(id);
        if (!el) return;
        el.className = 'ntrip-status-badge ' + state;
        el.textContent = text;
    }

    function formatPosition(lat, lon) {
        return `${lat.toFixed(7)}°, ${lon.toFixed(7)}°`;
    }

    function showResult(message, isError) {
        const el = document.getElementById('hwtest-result');
        if (!el) return;
        el.textContent = message;
        el.className = 'config-status ' + (isError ? 'error' : 'success');
        el.style.display = message ? 'block' : 'none';
    }

    function isRunning() {
        return relayState === 'applying' || relayState === 'waiting';
    }

    function updateButton() {
        const btn = document.getElementById('hwtest-relay-btn');
        if (btn) btn.disabled = !fixPvt || isRunning();
    }

    function finish(state, badgeState, badgeText, message) {
        clearTimeout(insideTimer);
        insideTimer = null;
        relayState = state;
        setBadge('hwtest-relay-status', badgeState, badgeText);
        setText(RELAY_HINT_ID, relayHint);
        showResult(message, state === 'failed');
        updateButton();
    }

    function pass() {
        finish('passed', 'connected', 'Inside',
            'Receiver reports Inside. Check that the GEOFENCE LED is lit '
            + 'and the relay is closed.');
    }

    function fail(message) {
        finish('failed', 'error', 'Failed', message);
    }

    async function runRelayTest() {
        if (!fixPvt || isRunning()) return;

        const centre = { lat: fixPvt.latitude, lon: fixPvt.longitude };
        relayState = 'applying';
        showResult('', false);
        setText('hwtest-relay-centre', formatPosition(centre.lat, centre.lon));
        setBadge('hwtest-relay-status', 'connecting', 'Applying');
        updateButton();

        let config;
        try {
            const resp = await fetch('/api/config');
            const body = await resp.json().catch(() => ({}));
            if (!resp.ok) throw new Error(body.error || resp.statusText);
            config = body;
        } catch (e) {
            fail('Could not read the current configuration: ' + e.message);
            return;
        }

        config.geofencing = {
            geofences: [{
                lat: centre.lat,
                lon: centre.lon,
                radius: GEOFENCE_RADIUS_M,
            }],
            confidence_level: GEOFENCE_CONFIDENCE_LEVEL,
            pin_polarity: PIN_POLARITY_LOW_MEANS_INSIDE,
        };
        // A test geofence must never end up in the flash of a board that ships.
        config.save_to_flash = false;

        const error = await submitConfig(config);
        if (error) {
            fail('Configuration was not applied: ' + error);
            return;
        }
        populateFormFromConfig(config);

        relayState = 'waiting';
        setBadge('hwtest-relay-status', 'connecting', 'Waiting for Inside');
        setText(RELAY_HINT_ID,
            'Configuration applied, waiting for the receiver to report Inside.');
        insideTimer = setTimeout(() => {
            fail(`Receiver did not report Inside within ${INSIDE_TIMEOUT_MS / 1000} s. `
                + 'Check the antenna and run the test again.');
        }, INSIDE_TIMEOUT_MS);
    }

    function onGpsUpdate(data) {
        if (!data || !data.pvt) return;
        const pvt = data.pvt;
        const hasFix = pvt.fix_status === 'Active';
        fixPvt = hasFix ? pvt : null;

        setBadge('hwtest-link-status', 'connected', 'OK');
        setBadge('hwtest-fix-status',
            hasFix ? 'connected' : 'connecting',
            hasFix ? 'OK' : 'Waiting for fix');
        setText('hwtest-fix-type', pvt.fix_type);
        setText('hwtest-fix-sats', pvt.visible_satellites);
        setText('hwtest-fix-pos',
            hasFix ? formatPosition(pvt.latitude, pvt.longitude) : '—');

        const geo = data.geofencing;
        if (geo) {
            const active = geo.status === 'Active';
            setText('hwtest-relay-state',
                active ? geo.combined_state : geo.status);
            if (relayState === 'waiting' && active
                    && geo.combined_state === 'Inside') {
                pass();
            }
        }
        updateButton();
    }

    // Nothing shown may outlive the connection: after a power cycle the page
    // reconnects to a different board.
    function onDisconnect() {
        fixPvt = null;
        setBadge('hwtest-link-status', 'error', 'Server offline');
        setBadge('hwtest-fix-status', 'disconnected', 'Pending');
        setText('hwtest-fix-type', '—');
        setText('hwtest-fix-sats', '—');
        setText('hwtest-fix-pos', '—');
        setText('hwtest-relay-state', '—');

        if (isRunning()) {
            fail('Connection to the server was lost during the test.');
            return;
        }
        relayState = 'idle';
        setBadge('hwtest-relay-status', 'disconnected', 'Pending');
        setText('hwtest-relay-centre', '—');
        showResult('', false);
        updateButton();
    }

    function onConfigProgress(data) {
        if (relayState === 'applying' && data && data.message) {
            setText(RELAY_HINT_ID, data.message);
        }
    }

    function init() {
        const btn = document.getElementById('hwtest-relay-btn');
        if (!btn) return;
        if (typeof socket === 'undefined' || !socket) {
            // Socket not yet available — defer.
            setTimeout(init, 100);
            return;
        }
        relayHint = document.getElementById(RELAY_HINT_ID).textContent;
        setText('hwtest-relay-radius', `${GEOFENCE_RADIUS_M} m`);
        btn.addEventListener('click', runRelayTest);
        socket.on('gps_update', onGpsUpdate);
        socket.on('disconnect', onDisconnect);
        socket.on('config_progress', onConfigProgress);
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        init();
    }
})();
