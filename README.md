# SafeMap

Веб-застосунок для пошуку укриттів і безпечних маршрутів у смт Коцюбинське (Київська область).
Сервер написаний на C++17, інтерфейс — веб-сторінка з інтерактивною мапою.

Що вміє:

- показує на мапі укриття, магазини, аптеки, пошту, кафе й АЗС селища;
- показує, що працює при відбої, жовтому та червоному рівні тривоги;
- режим **«Зараз»**: рівень тривоги береться з реальних даних (Київська область), а заклади, зачинені за графіком роботи, приховуються;
- режим **«Симуляція»**: рівень обирається вручну, щоб заздалегідь знати, що працюватиме під час тривоги;
- знаходить найближче укриття з вільними місцями і будує маршрут пішки або авто з часом у дорозі;
- приймає краудсорсингові звіти про стан закладу (генератор, кількість людей, небезпека);
- завантажує всі локації з CSV-файлів, тож дані можна оновлювати без перекомпіляції.

## Зовнішні бібліотеки (Лабораторна робота №3)

### C++ (сервер)

| Бібліотека | Версія | Ліцензія | Де в коді | Які можливості використано |
|---|---|---|---|---|
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | 0.63.0 | MIT | `main.cpp` | `httplib::Server`: маршрути `Get`/`Post`/`Options` для REST API; `set_mount_point` для роздачі папки `web/`; `set_file_extension_and_mimetype_mapping` для SVG; `set_default_headers` для CORS; `set_socket_options` для ексклюзивного порту на Windows; `Request::has_param`/`get_param_value` для query-параметрів; `httplib::SSLClient` (HTTPS-клієнт з таймаутами) для опитування сервісу тривог в `AlertMonitor.cpp` |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | MIT | `main.cpp`, `AlertMonitor.cpp` | `json::parse` з `allow_exceptions = false` (`is_discarded`) для безпечного розбору тіла запитів; `json::value` зі значеннями за замовчуванням; `json::update` для злиття `config.json` з налаштуваннями за замовчуванням; побудова відповідей API через ініціалізаційні списки та `dump` |
| [rapidcsv](https://github.com/d99kris/rapidcsv) | 8.84 | BSD-3-Clause | `DataLoader.cpp` | `rapidcsv::Document` з `LabelParams` (рядок заголовків), `SeparatorParams` (обрізання пробілів, лапки з комами всередині), `LineReaderParams` (пропуск порожніх рядків і коментарів `#`); `GetColumnIdx` для необов'язкових колонок; `GetColumnNames` для додаткових колонок типів; `GetCell<std::string>`; автоматичне відкидання UTF-8 BOM |

Усі три бібліотеки header-only і лежать у `include/`. Для HTTPS-клієнта httplib потрібен **OpenSSL 3** (у MSYS2 вже є в `mingw64`; CMake знаходить його автоматично). Якщо OpenSSL не знайдено, проєкт збирається без режиму реальної тривоги.

### JavaScript (сторінка)

| Бібліотека / сервіс | Версія | Що використано |
|---|---|---|
| [Leaflet](https://leafletjs.com/) | 1.9.4 | мапа, `L.tileLayer` з власним CSS-класом (темна тема), `L.divIcon` для SVG-маркерів, попапи, підказки, `fitBounds` |
| [Leaflet Routing Machine](https://www.liedman.net/leaflet-routing-machine/) | 3.2.12 | `L.Routing.control` + `L.Routing.osrmv1`, події `routesfound` / `routingerror` |
| [OSRM](https://project-osrm.org/) на `routing.openstreetmap.de` | — | пішохідний (`routed-foot`) і автомобільний (`routed-car`) маршрути |
| Тайли [OpenStreetMap](https://www.openstreetmap.org/copyright) | — | підкладка мапи |
| [opening_hours.js](https://github.com/opening-hours/opening_hours.js) | 3.15.0 | розбір графіків роботи OSM (`Mo-Fr 08:00-20:00; Sa 09:00-15:00`): `getState(date)` чи відчинено зараз, `getNextChange(date)` коли зміниться |
| [SunCalc](https://github.com/mourner/suncalc) | 1.9.0 | залежність opening_hours.js |
| Google Fonts: Unbounded, Manrope | — | шрифти з кирилицею |

### Інструменти даних

| Інструмент | Що використано |
|---|---|
| [Overpass API](https://wiki.openstreetmap.org/wiki/Overpass_API) | запит закладів і графіків роботи (`opening_hours`) у межах адміністративної межі селища (`tools/osm_to_csv.py`, лише стандартна бібліотека Python) |

### Реальна тривога

Сервер раз на 60 секунд запитує [ubilling.net.ua/aerialalerts](https://ubilling.net.ua/aerialalerts/) (публічний, без ключа; відповідь кешується сервісом на 60 с). Браузер не може звернутися до нього напряму (немає CORS), тому це робить C++-сервер (`AlertMonitor`).

| Ситуація | Рівень |
|---|---|
| тривога в Київській області | червоний |
| тривога лише в м. Київ (сусідній регіон) | жовтий |
| тривоги немає | відбій |

Сервіс дає статус лише на рівні області. Якщо запит не вдався, сервер тримає останній успішний статус і позначає його як застарілий.

### Графіки роботи в режимі «Зараз»

- укриття відчинені завжди;
- якщо в OSM є `opening_hours`, стан визначає opening_hours.js;
- якщо графіка немає, заклад вважається зачиненим з 01:00 до 06:00.

Заклади, зачинені за графіком, у режимі «Зараз» приховуються. У «Симуляції» графіки не враховуються.

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

Для маршрутів, тайлів мапи, шрифтів і статусу тривоги потрібен інтернет. Якщо сервіс маршрутів недоступний, сторінка показує приблизну оцінку відстані й часу, яку рахує сервер.

### config.json

```json
{
    "host": "0.0.0.0",
    "port": 8080,
    "web_root": "web",
    "data_files": ["data/osm_places.csv", "data/shelters.csv"],
    "overrides_file": "data/overrides.csv",
    "alerts": {
        "enabled": true,
        "region": "Київська область",
        "neighbor": "м. Київ",
        "poll_seconds": 60
    }
}
```

## Дані

| Файл | Звідки | Як оновлювати |
|---|---|---|
| `data/osm_places.csv` | OpenStreetMap, межа смт Коцюбинське ([relation 1754752](https://www.openstreetmap.org/relation/1754752)) | `python tools/osm_to_csv.py` (перезаписує файл) |
| `data/shelters.csv` | карта Google My Maps «Укриття Коцюбинської СТГ» | вручну |

Місткість укриттів у джерелі не вказана, тому в `shelters.csv` стоять оцінки (позначено в колонці `source`).

### Ручні уточнення: data/overrides.csv

OpenStreetMap не знає, де є генератор, тому всі заклади з OSM за замовчуванням вважаються такими, що не мають генератора. Через це при жовтому рівні чи тривозі вони закриваються. Щоб це виправити, не редагуйте `osm_places.csv` (його перезаписує скрипт). Заповніть `data/overrides.csv`:

```csv
osm,id,note,generator,open_24h,wifi,shelter_capacity,underground,name,type,address
way/502804717,,АТБ-Маркет,1,,,,,,,
node/12199698726,,Подорожник,так,1,,,,,,
,13,Укриття на Шевченка,,,,30,,,,
```

- Заклад знаходиться за колонкою `osm` (`node/123`, `way/123`, або повне посилання `https://www.openstreetmap.org/node/123`). Посилання OSM не змінюються після оновлення даних, на відміну від `id`. Для укриттів із `shelters.csv` використовуйте `id`.
- Порожня клітинка нічого не змінює. `note` — лише для вас, сервер її ігнорує.
- Можна змінити `generator`, `open_24h`, `wifi`, `food_reserves`, `max_parcel_kg`, `shelter_capacity`, `underground`, `name`, `type`, `address`, `lat`, `lng`.
- Уточнений заклад отримує в джерелі даних позначку «уточнено вручну».
- Якщо запис ні з чим не збігся (наприклад, заклад зник з OSM), сервер попереджає в консолі.

Файл задається в `config.json` як `overrides_file`.

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
| `GET` | `/api/establishments[?level=none\|yellow\|red]` | усі заклади: доступність для заданого рівня (без параметра — для реального), `availability` для всіх трьох рівнів, `opening_hours` |
| `GET` | `/api/alert` | реальний рівень тривоги, статус регіону й сусіда, час перевірки |
| `POST` | `/api/reports` | `{"id": 5, "generator": true, "occupied": 10, "hazard": false}` |
| `GET` | `/api/nearest-shelter?lat=..&lng=..[&level=..]` | найближче доступне укриття з оцінкою часу пішки й авто |

## Структура

```
main.cpp                    HTTP-сервер, API, конфігурація
Infrastructure.*            Coordinates (haversine), Shelter
Establishments.*            ієрархія закладів і правила доступності
Crowdsourcing.*             User, Report, StatusReport, HazardReport
RoutePlanner.*              пошук укриттів, оцінка часу в дорозі
AlertMonitor.*              IAlertSource, UbillingAlertSource (httplib SSLClient), фоновий AlertMonitor
EstablishmentFactory.*      створення закладів за типом із CSV
DataLoader.*                IDataSource, CsvDataSource (rapidcsv), OverriddenDataSource + RecordOverrides, DataLoader
web/                        index.html, css/style.css, js/app.js, img/logo.svg
data/                       CSV з локаціями
tools/osm_to_csv.py         експорт закладів з OpenStreetMap
```

## Ліцензії даних

- Дані мапи й закладів © [OpenStreetMap contributors](https://www.openstreetmap.org/copyright), ODbL.
- Маршрути: OSRM на сервері FOSSGIS (`routing.openstreetmap.de`).
- Перелік укриттів: карта «Укриття Коцюбинської СТГ» (Google My Maps).
