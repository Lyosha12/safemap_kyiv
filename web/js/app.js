const map = L.map('map', { zoomControl: false }).setView([50.4470, 30.5210], 14);
L.control.zoom({ position: 'bottomright' }).addTo(map);

L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
    maxZoom: 19,
    attribution: '&copy; OpenStreetMap'
}).addTo(map);

let markersLayer = L.layerGroup().addTo(map);
let userMarker = null;
let routingControl = null;
let establishmentsData = [];
let isNavMode = false;

async function loadData() {
    try {
        const res = await fetch('/api/establishments');
        establishmentsData = await res.json();

        markersLayer.clearLayers();
        const listEl = document.getElementById('locationsList');
        const selectEl = document.getElementById('reportEstSelect');
        document.getElementById('countBadge').innerText = establishmentsData.length;
        listEl.innerHTML = '';
        selectEl.innerHTML = '';

        establishmentsData.forEach(item => {
            const markerColor = item.accessible ? '#10b981' : '#f43f5e';
            const customIcon = L.divIcon({
                className: 'custom-pin',
                html: `<div style="background-color: ${markerColor}; width: 14px; height: 14px; border: 3px solid white; border-radius: 9999px; box-shadow: 0 4px 10px rgba(0,0,0,0.3);"></div>`,
                iconSize: [14, 14],
                iconAnchor: [7, 7]
            });

            const marker = L.marker([item.lat, item.lng], { icon: customIcon }).addTo(markersLayer);

            const badge = item.accessible
                ? `<span class="px-2.5 py-0.5 rounded-full text-[9px] font-extrabold bg-emerald-50 text-emerald-700 border border-emerald-200/80">ВІДЧИНЕНО</span>`
                : `<span class="px-2.5 py-0.5 rounded-full text-[9px] font-extrabold bg-rose-50 text-rose-700 border border-rose-200/80">ЗАЧИНЕНО</span>`;

            marker.bindPopup(`
                <div class="p-1.5 text-center font-sans">
                    <h5 class="font-extrabold text-zinc-900 text-xs mb-0.5">${item.name}</h5>
                    <p class="text-[10px] text-zinc-400 font-medium mb-2">${item.type}</p>
                    <div class="mb-2.5">${badge}</div>
                    <div class="text-[11px] text-zinc-600 mb-3">Місць в укритті: <b class="text-zinc-900">${item.shelter_free}</b> / ${item.shelter_capacity}</div>
                    <button onclick="showDetails(${item.id})" class="w-full py-1.5 px-3 bg-zinc-900 hover:bg-zinc-800 text-white rounded-lg text-[10px] font-extrabold transition">Детальніше</button>
                </div>
            `);

            const card = document.createElement('div');
            card.className = "p-3.5 bg-white hover:bg-zinc-50/80 border border-zinc-200/70 hover:border-zinc-300 rounded-2xl cursor-pointer transition shadow-sm flex items-center justify-between";
            card.innerHTML = `
                <div class="pr-2">
                    <div class="font-bold text-xs text-zinc-800 line-clamp-1">${item.name}</div>
                    <div class="text-[10px] text-zinc-400 font-medium mt-0.5">Вільних місць: <span class="font-bold text-zinc-700">${item.shelter_free}</span></div>
                </div>
                ${badge}
            `;
            card.onclick = () => {
                map.flyTo([item.lat, item.lng], 16, { duration: 1.2 });
                marker.openPopup();
            };
            listEl.appendChild(card);

            const opt = document.createElement('option');
            opt.value = item.id;
            opt.textContent = item.name;
            selectEl.appendChild(opt);
        });

    } catch (err) {
        console.error(err);
    }
}

const alertDescriptions = {
    'none': '🟢 Усі заклади функціонують у стандартному режимі.',
    'yellow': '🟡 Жовтий рівень: перевірено резервне живлення, чергування персоналу.',
    'red': '🔴 Червоний рівень: відкриті виключно сертифіковані укриття!'
};

async function syncAlertLevel() {
    try {
        const res = await fetch('/api/alert');
        const { level } = await res.json();
        document.getElementById('alertSelect').value = level;
        document.getElementById('alertDesc').innerText = alertDescriptions[level];
    } catch (err) {
        console.error(err);
    }
}

document.getElementById('alertSelect').addEventListener('change', async (e) => {
    const level = e.target.value;
    document.getElementById('alertDesc').innerText = alertDescriptions[level];

    await fetch('/api/alert', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ level })
    });
    loadData();
});

const navBtn = document.getElementById('btnToggleNav');
const navBanner = document.getElementById('navBanner');

navBtn.addEventListener('click', () => {
    isNavMode = !isNavMode;
    if (isNavMode) {
        navBtn.classList.replace('bg-primary-600', 'bg-amber-500');
        navBtn.classList.replace('hover:bg-primary-700', 'hover:bg-amber-600');
        document.getElementById('navBtnText').innerText = "Вимкнути";
        navBanner.classList.remove('hidden');
    } else {
        navBtn.classList.replace('bg-amber-500', 'bg-primary-600');
        navBtn.classList.replace('hover:bg-amber-600', 'hover:bg-primary-700');
        document.getElementById('navBtnText').innerText = "Навігатор";
        navBanner.classList.add('hidden');
        clearNavigation();
    }
});

const ROUTERS = {
    walk: { url: 'https://routing.openstreetmap.de/routed-foot/route/v1', label: 'Пішки' },
    drive: { url: 'https://routing.openstreetmap.de/routed-car/route/v1', label: 'Авто' }
};
const ROUTE_COLOR = '#4f46e5';

let travelMode = 'walk';
let routeOrigin = null;
let fallbackLine = null;
let routeRequestId = 0;

function formatDistance(km) {
    return km < 1 ? `${Math.round(km * 1000)} м` : `${km.toFixed(1)} км`;
}

function formatDuration(minutes) {
    const total = Math.max(1, Math.round(minutes));
    if (total < 60) return `${total} хв`;
    return `${Math.floor(total / 60)} год ${total % 60} хв`;
}

function clearRoute() {
    routeRequestId++;
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
    document.getElementById('routeCard').classList.add('hidden');
}

function clearNavigation() {
    clearRoute();
    routeOrigin = null;
}

function showRouteCard(shelter, distanceKm, minutes, sourceText, isEstimate = false) {
    document.getElementById('routeBadgeText').innerText = isEstimate ? 'Приблизна оцінка' : 'Маршрут розраховано';
    document.getElementById('routeTargetName').innerText = shelter.name;
    document.getElementById('routeDistance').innerText = formatDistance(distanceKm);
    document.getElementById('routeTimeLabel').innerText = ROUTERS[travelMode].label;
    document.getElementById('routeTime').innerText = formatDuration(minutes);
    document.getElementById('routePlaces').innerText = shelter.free_places;
    document.getElementById('routeSource').innerText = sourceText;
    document.getElementById('routeCard').classList.remove('hidden');
}

function showEstimatedRoute(origin, shelter) {
    const estimate = shelter.estimates[travelMode];
    fallbackLine = L.polyline([[origin.lat, origin.lng], [shelter.lat, shelter.lng]], {
        color: ROUTE_COLOR,
        weight: 4,
        opacity: 0.8,
        dashArray: '6 10'
    }).addTo(map);
    map.fitBounds(fallbackLine.getBounds(), { padding: [60, 60] });
    showRouteCard(shelter, estimate.distance_km, estimate.minutes,
        'Сервіс маршрутів недоступний — приблизна оцінка (пряма відстань з поправкою на вулиці)', true);
}

async function buildRoute(origin) {
    clearRoute();
    routeOrigin = origin;
    const requestId = routeRequestId;

    userMarker = L.circleMarker([origin.lat, origin.lng], {
        radius: 7,
        fillColor: ROUTE_COLOR,
        color: '#ffffff',
        weight: 3,
        fillOpacity: 1
    }).addTo(map).bindPopup("Ви тут").openPopup();

    let shelter;
    try {
        const res = await fetch(`/api/nearest-shelter?lat=${origin.lat}&lng=${origin.lng}`);
        shelter = await res.json();
        if (!res.ok) throw new Error(shelter.error);
    } catch (err) {
        alert(err.message || 'Не вдалося зв\'язатися з сервером');
        return;
    }
    if (requestId !== routeRequestId) return;
    if (!shelter.found) {
        alert(shelter.message);
        return;
    }

    routingControl = L.Routing.control({
        waypoints: [L.latLng(origin.lat, origin.lng), L.latLng(shelter.lat, shelter.lng)],
        router: L.Routing.osrmv1({
            serviceUrl: ROUTERS[travelMode].url,
            profile: 'driving',
            timeout: 10000
        }),
        lineOptions: { styles: [{ color: ROUTE_COLOR, weight: 5, opacity: 0.9 }] },
        addWaypoints: false,
        draggableWaypoints: false,
        fitSelectedRoutes: true,
        show: false
    }).addTo(map);

    routingControl.on('routesfound', (e) => {
        if (requestId !== routeRequestId) return;
        const summary = e.routes[0].summary;
        showRouteCard(shelter, summary.totalDistance / 1000, summary.totalTime / 60,
            'Маршрут вулицями: OSRM, дані © OpenStreetMap');
    });

    routingControl.on('routingerror', () => {
        if (requestId !== routeRequestId) return;
        map.removeControl(routingControl);
        routingControl = null;
        showEstimatedRoute(origin, shelter);
    });
}

function setTravelMode(mode) {
    travelMode = mode;
    document.querySelectorAll('#travelModeToggle .mode-btn').forEach(btn => {
        const active = btn.dataset.mode === mode;
        btn.classList.toggle('bg-white', active);
        btn.classList.toggle('shadow-sm', active);
        btn.classList.toggle('text-zinc-900', active);
        btn.classList.toggle('text-zinc-500', !active);
    });
    if (routeOrigin) buildRoute(routeOrigin);
}

document.querySelectorAll('#travelModeToggle .mode-btn').forEach(btn => {
    btn.addEventListener('click', () => setTravelMode(btn.dataset.mode));
});

document.getElementById('btnLocateMe').addEventListener('click', () => {
    if (!navigator.geolocation) {
        alert('Браузер не підтримує визначення місцезнаходження');
        return;
    }
    navigator.geolocation.getCurrentPosition(
        (pos) => buildRoute({ lat: pos.coords.latitude, lng: pos.coords.longitude }),
        () => alert('Не вдалося визначити місцезнаходження. Дозвольте доступ до геолокації або клікніть на мапі в режимі навігатора.'),
        { enableHighAccuracy: true, timeout: 10000 }
    );
});

map.on('click', (e) => {
    if (!isNavMode) return;
    buildRoute({ lat: e.latlng.lat, lng: e.latlng.lng });
});

document.getElementById('btnCloseRoute').onclick = clearNavigation;

function openModal(id) {
    const el = document.getElementById(id);
    el.classList.remove('hidden');
    el.classList.add('flex');
}

function closeModal(id) {
    const el = document.getElementById(id);
    el.classList.remove('flex');
    el.classList.add('hidden');
}

window.showDetails = function(id) {
    const item = establishmentsData.find(x => x.id === id);
    if (!item) return;

    document.getElementById('detailsName').innerText = item.name;
    document.getElementById('detailsBody').innerHTML = `
        <div class="flex justify-between py-2 border-b border-zinc-100">
            <span class="text-zinc-400 font-medium">Тип закладу:</span>
            <span class="font-bold text-zinc-800">${item.type}</span>
        </div>
        <div class="flex justify-between py-2 border-b border-zinc-100">
            <span class="text-zinc-400 font-medium">Резервне живлення:</span>
            <span class="font-bold ${item.generator ? 'text-emerald-600' : 'text-zinc-400'}">${item.generator ? '✅ Підключено' : '❌ Відсутнє'}</span>
        </div>
        <div class="flex justify-between py-2 border-b border-zinc-100">
            <span class="text-zinc-400 font-medium">Місткість укриття:</span>
            <span class="font-bold text-zinc-800">${item.shelter_capacity} осіб</span>
        </div>
        <div class="flex justify-between py-2 border-b border-zinc-100">
            <span class="text-zinc-400 font-medium">Вільних місць:</span>
            <span class="font-extrabold text-primary-600">${item.shelter_free}</span>
        </div>
        <div class="flex justify-between py-2">
            <span class="text-zinc-400 font-medium">Поточний статус:</span>
            <span class="font-bold ${item.accessible ? 'text-emerald-600' : 'text-rose-600'}">${item.accessible ? 'Доступно для цивільних' : 'Зачинено через тривогу'}</span>
        </div>
    `;
    openModal('detailsModal');
};

document.getElementById('btnOpenReport').onclick = () => {
    document.getElementById('reportFeedback').classList.add('hidden');
    openModal('reportModal');
};

document.querySelectorAll('.modal-close').forEach(btn => {
    btn.onclick = () => {
        closeModal('detailsModal');
        closeModal('reportModal');
    };
});

document.getElementById('btnSubmitReport').addEventListener('click', async () => {
    const estId = parseInt(document.getElementById('reportEstSelect').value);
    const hasGen = document.getElementById('reportGenerator').checked;
    const occDelta = parseInt(document.getElementById('reportOccupancy').value) || 0;
    const hazard = document.getElementById('reportHazard').checked;
    const feedback = document.getElementById('reportFeedback');

    try {
        const res = await fetch('/api/reports', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ id: estId, generator: hasGen, occupied: occDelta, hazard })
        });
        const data = await res.json();
        await loadData();

        if (!res.ok || data.warning) {
            feedback.className = 'px-6 pb-4 text-[11px] font-semibold text-rose-600';
            feedback.innerText = data.error || data.warning;
            return;
        }
        document.getElementById('reportHazard').checked = false;
        closeModal('reportModal');
    } catch (err) {
        feedback.className = 'px-6 pb-4 text-[11px] font-semibold text-rose-600';
        feedback.innerText = 'Не вдалося зв\'язатися з сервером';
    }
});

document.getElementById('refreshBtn').addEventListener('click', loadData);
syncAlertLevel();
loadData();
