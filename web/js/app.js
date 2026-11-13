const DEFAULT_VIEW = { center: [50.4905, 30.3338], zoom: 15 };

const ROUTERS = {
    walk: { url: 'https://routing.openstreetmap.de/routed-foot/route/v1', label: 'Пішки' },
    drive: { url: 'https://routing.openstreetmap.de/routed-car/route/v1', label: 'Авто' }
};
const ROUTE_COLOR = '#4C9BFF';

const LEVEL_NAMES = {
    none: 'Відбій',
    yellow: 'Жовтий рівень',
    red: 'Червоний рівень'
};

const ALERT_DESCRIPTIONS = {
    none: 'Відбій. Заклади працюють у звичайному режимі.',
    yellow: 'Жовтий рівень. Працюють заклади з резервним живленням, персонал чергує.',
    red: 'Червоний рівень. Відчинені лише укриття та цілодобові аптеки з генератором.'
};

const ALERT_POLL_MS = 30000;
const SCHEDULE_REFRESH_MS = 60000;
const NIGHT_START_HOUR = 1;
const NIGHT_END_HOUR = 6;

const TYPE_ICONS = {
    shelter: '<path d="M12 3l7 3v5c0 5-3 8.5-7 10-4-1.5-7-5-7-10V6z"/>',
    supermarket: '<path d="M3 4h2.5l2.2 10.5h10.6L20.5 7H6.3"/><circle cx="9" cy="19" r="1.4"/><circle cx="17" cy="19" r="1.4"/>',
    grocery: '<path d="M5 8h14l-1.2 11.2a1 1 0 0 1-1 .8H7.2a1 1 0 0 1-1-.8z"/><path d="M9 8V6a3 3 0 0 1 6 0v2"/>',
    pharmacy: '<rect x="4" y="4" width="16" height="16" rx="4"/><path d="M12 8v8M8 12h8"/>',
    post_office: '<rect x="3" y="6" width="18" height="12" rx="2"/><path d="M3 8l9 6 9-6"/>',
    cafe: '<path d="M4 9h12v4a5 5 0 0 1-5 5H9a5 5 0 0 1-5-5z"/><path d="M16 10h1.5a2.5 2.5 0 0 1 0 5H16"/><path d="M8 3.5v2M12 3.5v2"/>',
    gas_station: '<rect x="5" y="4" width="9" height="16" rx="1.5"/><path d="M7.5 8h4"/><path d="M14 10h2l2.5 2.5V18a1.5 1.5 0 0 1-3 0v-3H14"/>',
    default: '<circle cx="12" cy="10" r="3"/><path d="M12 21s-7-6-7-11a7 7 0 0 1 14 0c0 5-7 11-7 11z"/>'
};

const TYPE_LABELS_PLURAL = {
    shelter: 'Укриття',
    supermarket: 'Супермаркети',
    grocery: 'Продукти',
    pharmacy: 'Аптеки',
    post_office: 'Пошта',
    cafe: 'Кафе',
    gas_station: 'АЗС'
};

const state = {
    establishments: [],
    markers: new Map(),
    alertLevel: 'none',
    mode: 'live',
    simLevel: 'red',
    live: null,
    hiddenBySchedule: 0,
    typeFilter: 'all',
    tierFilter: 'all',
    search: '',
    onlyOpen: false,
    isNavMode: false,
    travelMode: 'walk',
    routeOrigin: null,
    routeRequestId: 0,
    hasFittedBounds: false
};

const $ = (id) => document.getElementById(id);

const map = L.map('map', { zoomControl: false }).setView(DEFAULT_VIEW.center, DEFAULT_VIEW.zoom);
L.control.zoom({ position: 'bottomright' }).addTo(map);
L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
    maxZoom: 19,
    className: 'dark-tiles',
    attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a>'
}).addTo(map);

const markersLayer = L.layerGroup().addTo(map);
let userMarker = null;
let routingControl = null;
let fallbackLine = null;

function escapeHtml(value) {
    return String(value ?? '').replace(/[&<>"']/g, (ch) => ({
        '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
    }[ch]));
}

function iconSvg(typeKey) {
    return `<svg viewBox="0 0 24 24">${TYPE_ICONS[typeKey] || TYPE_ICONS.default}</svg>`;
}

const TIERS = {
    red: { label: 'Працює при червоному рівні', short: 'Червоний рівень' },
    yellow: { label: 'Працює при жовтому рівні', short: 'Жовтий рівень' },
    none: { label: 'Не працює під час тривоги', short: 'Лише у відбій' },
    closed: { label: 'Зачинено через звіт про небезпеку', short: 'Зачинено' }
};

const scheduleCache = new Map();

function parsedSchedule(item) {
    if (!item.opening_hours || typeof opening_hours === 'undefined') return null;
    const key = item.opening_hours;
    if (!scheduleCache.has(key)) {
        try {
            scheduleCache.set(key, new opening_hours(key, {
                lat: item.lat,
                lon: item.lng,
                address: { country_code: 'ua', state: 'Київська область' }
            }));
        } catch (err) {
            console.warn(`Не вдалося розібрати графік "${key}":`, err);
            scheduleCache.set(key, null);
        }
    }
    return scheduleCache.get(key);
}

function formatClock(date) {
    return date.toLocaleTimeString('uk-UA', { hour: '2-digit', minute: '2-digit' });
}

function scheduleStatus(item, now) {
    if (item.type_key === 'shelter') {
        return { open: true, kind: 'always', text: 'Укриття відчинене цілодобово' };
    }
    const schedule = parsedSchedule(item);
    if (schedule) {
        const open = schedule.getState(now);
        const next = schedule.getNextChange(now);
        const nextText = next ? `${open ? 'зачиниться' : 'відчиниться'} о ${formatClock(next)}` : '';
        return { open, kind: 'hours', text: `${open ? 'Відчинено' : 'Зачинено'} за графіком${nextText ? ', ' + nextText : ''}` };
    }
    const hour = now.getHours();
    const open = hour < NIGHT_START_HOUR || hour >= NIGHT_END_HOUR;
    return {
        open,
        kind: 'night',
        text: `Графік невідомий: вважаємо зачиненим з 0${NIGHT_START_HOUR}:00 до 0${NIGHT_END_HOUR}:00`
    };
}

function applySchedules() {
    const now = new Date();
    state.hiddenBySchedule = 0;
    state.establishments.forEach(item => {
        item.schedule = scheduleStatus(item, now);
        item.accessible = item.alertAccessible && (state.mode === 'sim' || item.schedule.open);
        item.hiddenBySchedule = state.mode === 'live' && !item.schedule.open;
        if (item.hiddenBySchedule) state.hiddenBySchedule++;
    });
}
const TIER_ORDER = ['red', 'yellow', 'none', 'closed'];

function tierOf(item) {
    const a = item.availability;
    if (!a.none) return 'closed';
    if (a.red) return 'red';
    if (a.yellow) return 'yellow';
    return 'none';
}

function tierBadge(item) {
    const tier = tierOf(item);
    return `<span class="tier tier-${tier}"><i></i>${TIERS[tier].label}</span>`;
}

function markerClasses(item) {
    const classes = [`tier-${tierOf(item)}`];
    if (item.type_key === 'shelter') classes.push('shelter');
    if (!item.accessible) classes.push('closed');
    return classes.join(' ');
}

function createMarkerIcon(item) {
    return L.divIcon({
        className: '',
        html: `<div class="map-pin ${markerClasses(item)}">${iconSvg(item.type_key)}</div>`,
        iconSize: [32, 32],
        iconAnchor: [16, 34],
        popupAnchor: [0, -32]
    });
}

function statusBadge(item) {
    return item.accessible
        ? '<span class="status open">Відчинено</span>'
        : '<span class="status closed">Зачинено</span>';
}

function shelterText(item) {
    return item.shelter_capacity > 0
        ? `укриття: <b>${item.shelter_free}</b> / ${item.shelter_capacity}`
        : 'без укриття';
}

let toastTimer = null;
function showToast(message, kind = 'info') {
    const toast = $('toast');
    toast.textContent = message;
    toast.className = `toast ${kind === 'error' ? 'error' : ''}`;
    toast.hidden = false;
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => { toast.hidden = true; }, 4500);
}

function matchesFilters(item) {
    if (item.hiddenBySchedule) return false;
    if (state.typeFilter !== 'all' && item.type_key !== state.typeFilter) return false;
    if (state.tierFilter !== 'all' && tierOf(item) !== state.tierFilter) return false;
    if (state.onlyOpen && !item.accessible) return false;
    if (state.search) {
        const haystack = `${item.name} ${item.address || ''} ${item.type}`.toLowerCase();
        if (!haystack.includes(state.search)) return false;
    }
    return true;
}

function sortForList(a, b) {
    if (a.accessible !== b.accessible) return a.accessible ? -1 : 1;
    const tierDiff = TIER_ORDER.indexOf(tierOf(a)) - TIER_ORDER.indexOf(tierOf(b));
    if (tierDiff !== 0) return tierDiff;
    if ((a.type_key === 'shelter') !== (b.type_key === 'shelter')) return a.type_key === 'shelter' ? -1 : 1;
    return a.name.localeCompare(b.name, 'uk');
}

function shownEstablishments() {
    return state.establishments.filter(item => !item.hiddenBySchedule);
}

function renderTierFilters() {
    const shown = shownEstablishments();
    const counts = new Map();
    shown.forEach(item => counts.set(tierOf(item), (counts.get(tierOf(item)) || 0) + 1));
    if (state.tierFilter !== 'all' && !counts.has(state.tierFilter)) state.tierFilter = 'all';

    const chips = [['all', 'Будь-коли', shown.length],
        ...TIER_ORDER.filter(t => counts.has(t)).map(t => [t, TIERS[t].short, counts.get(t)])];

    $('tierFilters').innerHTML = chips.map(([key, label, count]) => `
        <button class="chip tier-chip ${key !== 'all' ? `tier-${key}` : ''} ${state.tierFilter === key ? 'active' : ''}" data-tier="${key}">
            ${key !== 'all' ? '<i></i>' : ''}${escapeHtml(label)} <span class="chip-count">${count}</span>
        </button>`).join('');
}

function renderTypeFilters() {
    const shown = shownEstablishments();
    const counts = new Map();
    shown.forEach(item => counts.set(item.type_key, (counts.get(item.type_key) || 0) + 1));
    if (state.typeFilter !== 'all' && !counts.has(state.typeFilter)) state.typeFilter = 'all';

    const chips = [['all', 'Усі', shown.length], ...[...counts.entries()]
        .sort((a, b) => b[1] - a[1])
        .map(([key, count]) => [key, TYPE_LABELS_PLURAL[key] || key, count])];

    $('typeFilters').innerHTML = chips.map(([key, label, count]) => `
        <button class="chip ${state.typeFilter === key ? 'active' : ''}" data-type="${escapeHtml(key)}">
            ${escapeHtml(label)} <span class="chip-count">${count}</span>
        </button>`).join('');
}

function popupHtml(item) {
    return `
        <div class="popup-type">${escapeHtml(item.type)}</div>
        <div class="popup-name">${escapeHtml(item.name)}</div>
        <div class="popup-row">${statusBadge(item)}<span>${shelterText(item)}</span></div>
        <div class="popup-tier">${tierBadge(item)}</div>
        <button class="btn btn-ghost btn-sm btn-block" onclick="showDetails(${item.id})">Детальніше</button>`;
}

function renderMap() {
    markersLayer.clearLayers();
    state.markers.clear();
    state.establishments.filter(matchesFilters).forEach(item => {
        const marker = L.marker([item.lat, item.lng], {
            icon: createMarkerIcon(item),
            title: item.name,
            zIndexOffset: item.type_key === 'shelter' ? 500 : 0
        }).bindPopup(popupHtml(item));
        marker.addTo(markersLayer);
        state.markers.set(item.id, marker);
    });
}

function renderList() {
    const visible = state.establishments.filter(matchesFilters).sort(sortForList);
    $('countBadge').textContent = visible.length;

    const hiddenNote = state.hiddenBySchedule
        ? `<div class="hidden-note">Ще ${state.hiddenBySchedule} закладів зараз зачинені за графіком і приховані. Перемкніться на «Симуляцію», щоб побачити всі.</div>`
        : '';

    if (!visible.length) {
        $('locationsList').innerHTML = '<div class="empty">Нічого не знайдено за цими фільтрами</div>' + hiddenNote;
        return;
    }

    $('locationsList').innerHTML = hiddenNote + visible.map(item => `
        <button class="location-card ${item.accessible ? '' : 'closed'}" data-id="${item.id}">
            <span class="type-icon ${item.type_key === 'shelter' ? 'shelter' : ''}">${iconSvg(item.type_key)}</span>
            <span class="location-main">
                <span class="location-name">${escapeHtml(item.name)}</span>
                <span class="location-meta">${item.type_key === 'shelter'
                    ? `${escapeHtml(item.address || item.type)} · місць: <b>${item.shelter_free}</b> / ${item.shelter_capacity}`
                    : `${escapeHtml(item.type)} · ${shelterText(item)}`}</span>
                ${tierBadge(item)}
            </span>
            ${statusBadge(item)}
        </button>`).join('');
}

function renderModeDescription() {
    const level = state.alertLevel;
    const desc = $('alertDesc');
    const modeText = state.mode === 'sim'
        ? 'Симуляція: рівень обрано вручну, графіки роботи не враховуються.'
        : 'Зараз: рівень за реальною тривогою, враховано графіки роботи.';
    desc.innerHTML = `<strong>${ALERT_DESCRIPTIONS[level]}</strong><span>${modeText}</span>`;
    desc.dataset.level = level;
}

function renderReportOptions() {
    const select = $('reportEstSelect');
    const selected = select.value;
    select.innerHTML = [...state.establishments]
        .sort((a, b) => a.name.localeCompare(b.name, 'uk'))
        .map(item => `<option value="${item.id}">${escapeHtml(item.name)}</option>`)
        .join('');
    if (selected) select.value = selected;
}

function renderAll() {
    applySchedules();
    renderModeDescription();
    renderTierFilters();
    renderTypeFilters();
    renderMap();
    renderList();
    renderReportOptions();
}

function fitToData() {
    if (state.hasFittedBounds || !state.establishments.length) return;
    const bounds = L.latLngBounds(state.establishments.map(item => [item.lat, item.lng]));
    const isPhone = window.matchMedia('(max-width: 720px)').matches;
    map.fitBounds(bounds, {
        paddingTopLeft: [40, 40],
        paddingBottomRight: [40, isPhone ? $('sidebar').offsetHeight + 20 : 40],
        maxZoom: 15
    });
    state.hasFittedBounds = true;
}

function levelQuery(prefix) {
    return state.mode === 'sim' ? `${prefix}level=${state.simLevel}` : '';
}

async function loadData() {
    try {
        const res = await fetch(`/api/establishments${levelQuery('?')}`);
        if (!res.ok) throw new Error();
        state.establishments = (await res.json()).map(item => ({ ...item, alertAccessible: item.accessible }));
        renderAll();
        fitToData();
    } catch (err) {
        $('locationsList').innerHTML = '<div class="empty">Не вдалося завантажити дані з сервера</div>';
        showToast('Сервер SafeMap недоступний', 'error');
    }
}

function setActive(selector, attr, value) {
    document.querySelectorAll(selector).forEach(btn => {
        const active = btn.dataset[attr] === value;
        btn.classList.toggle('active', active);
        btn.setAttribute('aria-checked', active);
    });
}

function applyAlertLevel(level) {
    state.alertLevel = level;
    setActive('.alert-option', 'level', state.simLevel);
    $('alertBanner').hidden = level !== 'red';
    $('alertBannerText').textContent = state.mode === 'sim'
        ? 'Симуляція червоного рівня тривоги.'
        : `Повітряна тривога: ${state.live?.region || 'ваш регіон'}. Прямуйте до найближчого укриття.`;
    renderModeDescription();
    setTimeout(() => map.invalidateSize(), 0);
}

function renderLiveStatus() {
    const live = state.live;
    let title = 'Перевіряю тривогу…';
    let meta = '';
    let level = 'unknown';

    if (live && !live.live) {
        title = 'Реальна тривога вимкнена';
        meta = 'увімкніть alerts у config.json';
    } else if (live && live.ok === false) {
        title = 'Немає даних про тривогу';
        meta = live.error || '';
    } else if (live && live.ok) {
        level = live.level;
        const since = (changed) => changed ? ` з ${changed.slice(11, 16)}` : '';
        if (live.region_alert) title = `Тривога: ${live.region}${since(live.region_changed)}`;
        else if (live.neighbor_alert) title = `Тривога: ${live.neighbor}${since(live.neighbor_changed)}`;
        else title = 'Тривоги немає';
        meta = live.stale
            ? `${LEVEL_NAMES[live.level]} · дані з ${live.checked_at.slice(11, 16)}, сервіс тимчасово недоступний`
            : `${LEVEL_NAMES[live.level]} · перевірено ${live.checked_at.slice(11, 16)}`;
    }
    $('liveStatus').dataset.level = level;
    $('liveTitle').textContent = title;
    $('liveMeta').textContent = meta;
}

async function pollLiveAlert() {
    try {
        const res = await fetch('/api/alert');
        state.live = await res.json();
    } catch (err) {
        state.live = { live: true, ok: false, error: 'сервер SafeMap недоступний' };
    }
    renderLiveStatus();
    if (state.mode === 'live' && state.live.ok) {
        if (state.live.level !== state.alertLevel) {
            applyAlertLevel(state.live.level);
            await loadData();
        }
    }
}

function savePreference(key, value) {
    try { localStorage.setItem(key, value); } catch (err) { }
}

function loadPreference(key, fallback) {
    try { return localStorage.getItem(key) || fallback; } catch (err) { return fallback; }
}

async function setMode(mode) {
    state.mode = mode;
    savePreference('safemap-mode', mode);
    setActive('.mode-option', 'mode', mode);
    $('alertSwitch').hidden = mode !== 'sim';
    $('liveStatus').hidden = mode !== 'live';
    const level = mode === 'sim' ? state.simLevel : (state.live?.ok ? state.live.level : 'none');
    applyAlertLevel(level);
    clearNavigation();
    await loadData();
}

async function setSimLevel(level) {
    state.simLevel = level;
    savePreference('safemap-sim-level', level);
    applyAlertLevel(level);
    clearNavigation();
    await loadData();
}

function formatDistance(km) {
    return km < 1 ? `${Math.round(km * 1000)} м` : `${km.toFixed(1)} км`;
}

function formatDuration(minutes) {
    const total = Math.max(1, Math.round(minutes));
    if (total < 60) return `${total} хв`;
    return `${Math.floor(total / 60)} год ${total % 60} хв`;
}

function clearRoute() {
    state.routeRequestId++;
    if (routingControl) {
        map.removeControl(routingControl);
        routingControl = null;
    }
    if (fallbackLine) {
        map.removeLayer(fallbackLine);
        fallbackLine = null;
    }
    if (userMarker) {
        map.removeLayer(userMarker);
        userMarker = null;
    }
    $('routeCard').hidden = true;
}

function clearNavigation() {
    clearRoute();
    state.routeOrigin = null;
}

function showRouteCard(shelter, distanceKm, minutes, sourceText, isEstimate = false) {
    const badge = $('routeBadgeText');
    badge.textContent = isEstimate ? 'Приблизна оцінка' : 'Маршрут розраховано';
    badge.classList.toggle('estimate', isEstimate);
    $('routeTargetName').textContent = shelter.name;
    $('routeDistance').textContent = formatDistance(distanceKm);
    $('routeTimeLabel').textContent = ROUTERS[state.travelMode].label;
    $('routeTime').textContent = formatDuration(minutes);
    $('routePlaces').textContent = shelter.free_places;
    $('routeSource').textContent = sourceText;
    $('routeCard').hidden = false;
}

function showEstimatedRoute(origin, shelter) {
    const estimate = shelter.estimates[state.travelMode];
    fallbackLine = L.polyline([[origin.lat, origin.lng], [shelter.lat, shelter.lng]], {
        color: ROUTE_COLOR,
        weight: 4,
        opacity: 0.85,
        dashArray: '6 10'
    }).addTo(map);
    map.fitBounds(fallbackLine.getBounds(), { padding: [80, 80], maxZoom: 17 });
    showRouteCard(shelter, estimate.distance_km, estimate.minutes,
        'Сервіс маршрутів недоступний — приблизна оцінка (пряма відстань з поправкою на вулиці)', true);
}

async function buildRoute(origin) {
    clearRoute();
    state.routeOrigin = origin;
    const requestId = state.routeRequestId;

    userMarker = L.marker([origin.lat, origin.lng], {
        icon: L.divIcon({ className: '', html: '<div class="user-pin"></div>', iconSize: [18, 18], iconAnchor: [9, 9] }),
        zIndexOffset: 1000
    }).addTo(map).bindTooltip('Ви тут', { direction: 'top', offset: [0, -10] });

    let shelter;
    try {
        const res = await fetch(`/api/nearest-shelter?lat=${origin.lat}&lng=${origin.lng}${levelQuery('&')}`);
        shelter = await res.json();
        if (!res.ok) throw new Error(shelter.error);
    } catch (err) {
        showToast(err.message || 'Не вдалося зв\'язатися з сервером', 'error');
        return;
    }
    if (requestId !== state.routeRequestId) return;
    if (!shelter.found) {
        showToast(shelter.message, 'error');
        return;
    }

    routingControl = L.Routing.control({
        waypoints: [L.latLng(origin.lat, origin.lng), L.latLng(shelter.lat, shelter.lng)],
        router: L.Routing.osrmv1({
            serviceUrl: ROUTERS[state.travelMode].url,
            profile: 'driving',
            timeout: 10000
        }),
        lineOptions: {
            styles: [
                { color: '#0B0D12', weight: 9, opacity: 0.6 },
                { color: ROUTE_COLOR, weight: 5, opacity: 0.95 }
            ]
        },
        createMarker: () => null,
        addWaypoints: false,
        draggableWaypoints: false,
        fitSelectedRoutes: false,
        show: false
    }).addTo(map);

    routingControl.on('routesfound', (e) => {
        if (requestId !== state.routeRequestId) return;
        map.fitBounds(L.latLngBounds(e.routes[0].coordinates), { padding: [80, 80], maxZoom: 17 });
        const summary = e.routes[0].summary;
        showRouteCard(shelter, summary.totalDistance / 1000, summary.totalTime / 60,
            'Маршрут вулицями: OSRM, дані © OpenStreetMap');
    });

    routingControl.on('routingerror', () => {
        if (requestId !== state.routeRequestId) return;
        map.removeControl(routingControl);
        routingControl = null;
        showEstimatedRoute(origin, shelter);
    });
}

function setTravelMode(mode) {
    state.travelMode = mode;
    document.querySelectorAll('#travelModeToggle .segment').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.mode === mode);
    });
    if (state.routeOrigin) buildRoute(state.routeOrigin);
}

function setNavMode(enabled) {
    state.isNavMode = enabled;
    $('btnToggleNav').classList.toggle('active', enabled);
    $('navBtnText').textContent = enabled ? 'Вимкнути навігатор' : 'Навігатор';
    $('navBanner').hidden = !enabled;
    map.getContainer().style.cursor = enabled ? 'crosshair' : '';
    if (!enabled) clearNavigation();
}

function routeFromMyLocation() {
    if (!navigator.geolocation) {
        showToast('Браузер не підтримує визначення місцезнаходження', 'error');
        return;
    }
    navigator.geolocation.getCurrentPosition(
        (pos) => buildRoute({ lat: pos.coords.latitude, lng: pos.coords.longitude }),
        () => showToast('Не вдалося визначити місцезнаходження. Увімкніть навігатор і клікніть на мапі.', 'error'),
        { enableHighAccuracy: true, timeout: 10000 }
    );
}

function openModal(id) {
    $(id).hidden = false;
}

function closeModals() {
    $('detailsModal').hidden = true;
    $('reportModal').hidden = true;
}

function detailRow(label, value, cls = '') {
    return `<div class="detail-row"><span>${label}</span><span class="${cls}">${value}</span></div>`;
}

window.showDetails = function (id) {
    const item = state.establishments.find(x => x.id === id);
    if (!item) return;

    $('detailsType').textContent = item.type;
    $('detailsName').textContent = item.name;

    const levels = ['none', 'yellow', 'red'].map(level => [level, LEVEL_NAMES[level]]);
    const schedule = levels.map(([level, label]) => {
        const works = item.availability[level];
        return `<div class="level-cell level-${level} ${works ? 'works' : 'off'} ${state.alertLevel === level ? 'current' : ''}">
            <span class="level-name">${label}</span>
            <span class="level-state">${works ? 'Працює' : 'Не працює'}</span>
        </div>`;
    }).join('');

    const rows = [
        detailRow(state.mode === 'sim' ? `При рівні «${LEVEL_NAMES[state.alertLevel]}»` : 'Зараз',
            item.accessible ? 'Відчинено' : 'Зачинено', item.accessible ? 'good' : 'bad'),
        `<div class="level-grid">${schedule}</div>`,
        item.opening_hours ? detailRow('Графік роботи', escapeHtml(item.opening_hours)) : '',
        item.schedule ? `<p class="schedule-note">${escapeHtml(item.schedule.text)}</p>` : '',
        item.address ? detailRow('Адреса', escapeHtml(item.address)) : '',
        detailRow('Резервне живлення', item.generator ? 'Є генератор' : 'Немає', item.generator ? 'good' : ''),
    ];

    if (item.shelter_capacity > 0) {
        const used = item.shelter_capacity - item.shelter_free;
        const percent = Math.round(used / item.shelter_capacity * 100);
        rows.push(
            detailRow('Розташування укриття', item.shelter_underground ? 'Підвал / підземне' : 'Не вказано'),
            detailRow('Вільних місць', `${item.shelter_free} з ${item.shelter_capacity}`, 'accent'),
            `<div class="occupancy" title="Заповненість ${percent}%"><span style="width:${percent}%"></span></div>`
        );
    } else {
        rows.push(detailRow('Укриття', 'Немає'));
    }

    if (item.source) rows.push(detailRow('Джерело даних', escapeHtml(item.source)));

    $('detailsBody').innerHTML = rows.join('');
    openModal('detailsModal');
};

async function submitReport() {
    const feedback = $('reportFeedback');
    const payload = {
        id: parseInt($('reportEstSelect').value, 10),
        generator: $('reportGenerator').checked,
        occupied: parseInt($('reportOccupancy').value, 10) || 0,
        hazard: $('reportHazard').checked
    };

    try {
        const res = await fetch('/api/reports', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        const data = await res.json();
        await loadData();

        if (!res.ok || data.warning) {
            feedback.textContent = data.error || data.warning;
            feedback.hidden = false;
            return;
        }
        $('reportHazard').checked = false;
        closeModals();
        showToast(payload.hazard ? 'Локацію позначено як небезпечну' : 'Звіт прийнято, дані оновлено');
    } catch (err) {
        feedback.textContent = 'Не вдалося зв\'язатися з сервером';
        feedback.hidden = false;
    }
}

function setSheetState(mode) {
    const sidebar = $('sidebar');
    sidebar.classList.toggle('expanded', mode === 'expanded');
    sidebar.classList.toggle('collapsed', mode === 'collapsed');
}

$('alertSwitch').addEventListener('click', (e) => {
    const btn = e.target.closest('.alert-option');
    if (btn && btn.dataset.level !== state.simLevel) setSimLevel(btn.dataset.level);
});

$('modeSwitch').addEventListener('click', (e) => {
    const btn = e.target.closest('.mode-option');
    if (btn && btn.dataset.mode !== state.mode) setMode(btn.dataset.mode);
});

$('tierFilters').addEventListener('click', (e) => {
    const chip = e.target.closest('.chip');
    if (!chip) return;
    state.tierFilter = chip.dataset.tier;
    renderTierFilters();
    renderMap();
    renderList();
});

$('typeFilters').addEventListener('click', (e) => {
    const chip = e.target.closest('.chip');
    if (!chip) return;
    state.typeFilter = chip.dataset.type;
    renderTypeFilters();
    renderMap();
    renderList();
});

$('searchInput').addEventListener('input', (e) => {
    state.search = e.target.value.trim().toLowerCase();
    renderMap();
    renderList();
});

$('onlyOpen').addEventListener('change', (e) => {
    state.onlyOpen = e.target.checked;
    renderMap();
    renderList();
});

$('locationsList').addEventListener('click', (e) => {
    const card = e.target.closest('.location-card');
    if (!card) return;
    const id = Number(card.dataset.id);
    const item = state.establishments.find(x => x.id === id);
    const marker = state.markers.get(id);
    if (!item || !marker) return;
    if (window.matchMedia('(max-width: 720px)').matches) setSheetState('collapsed');
    map.flyTo([item.lat, item.lng], 17, { duration: 1 });
    map.once('moveend', () => marker.openPopup());
});

$('travelModeToggle').addEventListener('click', (e) => {
    const btn = e.target.closest('.segment');
    if (btn) setTravelMode(btn.dataset.mode);
});

$('btnToggleNav').addEventListener('click', () => setNavMode(!state.isNavMode));
$('btnLocateMe').addEventListener('click', routeFromMyLocation);
$('btnBannerShelter').addEventListener('click', routeFromMyLocation);
$('btnCloseRoute').addEventListener('click', clearNavigation);
$('refreshBtn').addEventListener('click', () => { pollLiveAlert(); loadData(); });

$('btnOpenReport').addEventListener('click', () => {
    $('reportFeedback').hidden = true;
    openModal('reportModal');
});
$('btnSubmitReport').addEventListener('click', submitReport);

document.querySelectorAll('.modal-close').forEach(btn => btn.addEventListener('click', closeModals));
document.querySelectorAll('.modal').forEach(modal => modal.addEventListener('click', (e) => {
    if (e.target === modal) closeModals();
}));
document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') closeModals();
});

$('sheetHandle').addEventListener('click', () => {
    const sidebar = $('sidebar');
    setSheetState(sidebar.classList.contains('expanded') ? 'default'
        : sidebar.classList.contains('collapsed') ? 'default' : 'expanded');
});

map.on('click', (e) => {
    if (!state.isNavMode) return;
    buildRoute({ lat: e.latlng.lat, lng: e.latlng.lng });
});

async function init() {
    state.simLevel = loadPreference('safemap-sim-level', 'red');
    await pollLiveAlert();
    await setMode(loadPreference('safemap-mode', 'live'));
    setInterval(pollLiveAlert, ALERT_POLL_MS);
    setInterval(() => {
        if (state.mode !== 'live' || !state.establishments.length) return;
        renderAll();
    }, SCHEDULE_REFRESH_MS);
}

init();
