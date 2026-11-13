# SafeMap

Веб-застосунок для пошуку укриттів і безпечних маршрутів у смт Коцюбинське (Київська область).
Сервер написаний на C++17, інтерфейс — веб-сторінка з інтерактивною мапою.

Що вміє:

- показує на мапі укриття, магазини, аптеки, пошту, кафе й АЗС селища;
- змінює доступність закладів залежно від рівня тривоги (відбій / готовність / тривога);
- знаходить найближче укриття з вільними місцями і будує маршрут пішки або авто з часом у дорозі;
- приймає краудсорсингові звіти про стан закладу (генератор, кількість людей, небезпека);
- завантажує всі локації з CSV-файлів, тож дані можна оновлювати без перекомпіляції.

## Зовнішні бібліотеки (Лабораторна робота №3)

### C++ (сервер)

| Бібліотека | Версія | Ліцензія | Де в коді | Які можливості використано |
|---|---|---|---|---|
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | 0.63.0 | MIT | `main.cpp` | `httplib::Server`: маршрути `Get`/`Post`/`Options` для REST API; `set_mount_point` для роздачі папки `web/`; `set_file_extension_and_mimetype_mapping` для SVG; `set_default_headers` для CORS; `set_socket_options` для ексклюзивного порту на Windows; `Request::has_param`/`get_param_value` для query-параметрів |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | MIT | `main.cpp` | `json::parse` з `allow_exceptions = false` (`is_discarded`) для безпечного розбору тіла запитів; `json::value` зі значеннями за замовчуванням; `json::update` для злиття `config.json` з налаштуваннями за замовчуванням; побудова відповідей API через ініціалізаційні списки та `dump` |
| [rapidcsv](https://github.com/d99kris/rapidcsv) | 8.84 | BSD-3-Clause | `DataLoader.cpp` | `rapidcsv::Document` з `LabelParams` (рядок заголовків), `SeparatorParams` (обрізання пробілів, лапки з комами всередині), `LineReaderParams` (пропуск порожніх рядків і коментарів `#`); `GetColumnIdx` для необов'язкових колонок; `GetColumnNames` для додаткових колонок типів; `GetCell<std::string>`; автоматичне відкидання UTF-8 BOM |

Усі три бібліотеки header-only і лежать у `include/`.

### JavaScript (сторінка)

| Бібліотека / сервіс | Версія | Що використано |
|---|---|---|
| [Leaflet](https://leafletjs.com/) | 1.9.4 | мапа, `L.tileLayer` з власним CSS-класом (темна тема), `L.divIcon` для SVG-маркерів, попапи, підказки, `fitBounds` |
| [Leaflet Routing Machine](https://www.liedman.net/leaflet-routing-machine/) | 3.2.12 | `L.Routing.control` + `L.Routing.osrmv1`, події `routesfound` / `routingerror` |
| [OSRM](https://project-osrm.org/) на `routing.openstreetmap.de` | — | пішохідний (`routed-foot`) і автомобільний (`routed-car`) маршрути |
| Тайли [OpenStreetMap](https://www.openstreetmap.org/copyright) | — | підкладка мапи |
| Google Fonts: Unbounded, Manrope | — | шрифти з кирилицею |

### Інструменти даних

| Інструмент | Що використано |
|---|---|
| [Overpass API](https://wiki.openstreetmap.org/wiki/Overpass_API) | запит закладів у межах адміністративної межі селища (`tools/osm_to_csv.py`, лише стандартна бібліотека Python) |

Ретроспектива використання бібліотек: [docs/LAB3_RETROSPECTIVE.md](docs/LAB3_RETROSPECTIVE.md).

## Збірка та запуск

Потрібні CMake 3.20+ і компілятор з підтримкою C++17 (перевірено на MinGW-w64 GCC з MSYS2, Windows).

```bash
cmake -S . -B build
cmake --build build
cd build
./safemap_kyiv
```

Потім відкрийте <http://localhost:8080>.

Сервер шукає `config.json`, `web/` і `data/` у поточній теці та на рівень вище, тому запуск з `build/` чи `cmake-build-debug/` (CLion) працює без копіювання файлів.

Для маршрутів, тайлів мапи і шрифтів потрібен інтернет. Якщо сервіс маршрутів недоступний, сторінка показує приблизну оцінку відстані й часу, яку рахує сервер.

### config.json

```json
{
    "host": "0.0.0.0",
    "port": 8080,
    "web_root": "web",
    "data_files": ["data/osm_places.csv", "data/shelters.csv"]
}
```

## Дані

| Файл | Звідки | Як оновлювати |
|---|---|---|
| `data/osm_places.csv` | OpenStreetMap, межа смт Коцюбинське ([relation 1754752](https://www.openstreetmap.org/relation/1754752)) | `python tools/osm_to_csv.py` (перезаписує файл) |
| `data/shelters.csv` | карта Google My Maps «Укриття Коцюбинської СТГ» | вручну |

Місткість укриттів у джерелі не вказана, тому в `shelters.csv` стоять оцінки (позначено в колонці `source`).

### Формат CSV

Обов'язкові колонки: `id, type, name, lat, lng`. Необов'язкові: `shelter_capacity, underground, generator, address, source`.
Логічні значення можна писати як `1/0`, `true/false`, `yes/no` або `так/ні`. Назви з комами беруться в лапки.

| `type` | Клас | Додаткові колонки |
|---|---|---|
| `shelter` | `PublicShelter` | — |
| `supermarket` | `Supermarket` | `food_reserves` |
| `grocery` | `GroceryStore` | — |
| `pharmacy` | `Pharmacy` | `open_24h` |
| `post_office` | `PostOffice` | `max_parcel_kg` |
| `cafe` | `Cafe` | `wifi` |
| `gas_station` | `GasStation` | `open_24h` |

Рядки з помилками (невідомий тип, некоректні координати, повторний `id` тощо) пропускаються, а сервер пише в консоль номер рядка і причину.

## REST API

| Метод | Шлях | Опис |
|---|---|---|
| `GET` | `/api/establishments` | усі заклади з поточною доступністю |
| `GET` | `/api/alert` | поточний рівень тривоги |
| `POST` | `/api/alert` | `{"level": "none" \| "yellow" \| "red"}` |
| `POST` | `/api/reports` | `{"id": 5, "generator": true, "occupied": 10, "hazard": false}` |
| `GET` | `/api/nearest-shelter?lat=..&lng=..` | найближче доступне укриття з оцінкою часу пішки й авто |

## Структура

```
main.cpp                    HTTP-сервер, API, конфігурація
Infrastructure.*            Coordinates (haversine), Shelter
Establishments.*            ієрархія закладів і правила доступності
Crowdsourcing.*             User, Report, StatusReport, HazardReport
RoutePlanner.*              пошук укриттів, оцінка часу в дорозі
EstablishmentFactory.*      створення закладів за типом із CSV
DataLoader.*                IDataSource, CsvDataSource (rapidcsv), DataLoader
web/                        index.html, css/style.css, js/app.js, img/logo.svg
data/                       CSV з локаціями
tools/osm_to_csv.py         експорт закладів з OpenStreetMap
```

## Ліцензії даних

- Дані мапи й закладів © [OpenStreetMap contributors](https://www.openstreetmap.org/copyright), ODbL.
- Маршрути: OSRM на сервері FOSSGIS (`routing.openstreetmap.de`).
- Перелік укриттів: карта «Укриття Коцюбинської СТГ» (Google My Maps).
