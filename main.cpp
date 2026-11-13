#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#endif

#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include "httplib.h"
#include "json.hpp"
#include "DataLoader.h"
#include "RoutePlanner.h"
#include "Establishments.h"
#include "Infrastructure.h"
#include "Crowdsourcing.h"
#include "AlertMonitor.h"
#include <windows.h>

using json = nlohmann::json;

namespace {

const std::vector<std::string> kSearchPrefixes = {"", "../"};

json load_config() {
    json defaults = {{"host", "0.0.0.0"}, {"port", 8080}, {"web_root", "web"},
                     {"data_files", {"data/osm_places.csv", "data/shelters.csv"}},
                     {"overrides_file", "data/overrides.csv"},
                     {"alerts", {{"enabled", true}, {"region", "Київська область"},
                                 {"neighbor", "м. Київ"}, {"poll_seconds", 60}}}};
    for (const auto& prefix : kSearchPrefixes) {
        std::ifstream file(prefix + "config.json");
        if (!file.is_open()) continue;
        try {
            json loaded = json::parse(file);
            defaults.update(loaded);
            std::cout << "[Конфіг] Завантажено " << prefix << "config.json" << std::endl;
        } catch (const json::parse_error& e) {
            std::cerr << "[Конфіг] Помилка у config.json: " << e.what() << std::endl;
        }
        break;
    }
    return defaults;
}

std::optional<std::string> resolve_existing(const std::string& relative) {
    for (const auto& prefix : kSearchPrefixes) {
        if (std::ifstream(prefix + relative).good()) return prefix + relative;
    }
    return std::nullopt;
}

std::string resolve_web_root(const std::string& configured) {
    for (const auto& prefix : kSearchPrefixes) {
        std::string candidate = prefix + configured;
        if (std::ifstream(candidate + "/index.html").good()) return candidate;
    }
    return {};
}

std::string alert_level_name(AlertLevel level) {
    switch (level) {
        case AlertLevel::Red: return "red";
        case AlertLevel::Yellow: return "yellow";
        default: return "none";
    }
}

std::optional<AlertLevel> parse_alert_level(const std::string& name) {
    if (name == "none") return AlertLevel::None;
    if (name == "yellow") return AlertLevel::Yellow;
    if (name == "red") return AlertLevel::Red;
    return std::nullopt;
}

std::optional<Coordinates> parse_coordinates(const httplib::Request& req) {
    if (!req.has_param("lat") || !req.has_param("lng")) return std::nullopt;
    try {
        double lat = std::stod(req.get_param_value("lat"));
        double lng = std::stod(req.get_param_value("lng"));
        if (lat < -90.0 || lat > 90.0 || lng < -180.0 || lng > 180.0) return std::nullopt;
        return Coordinates(lat, lng);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

enum class LevelParam { Absent, Valid, Invalid };

LevelParam read_level_param(const httplib::Request& req, AlertLevel& level) {
    if (!req.has_param("level")) return LevelParam::Absent;
    auto parsed = parse_alert_level(req.get_param_value("level"));
    if (!parsed) return LevelParam::Invalid;
    level = *parsed;
    return LevelParam::Valid;
}

void send_error(httplib::Response& res, int status, const std::string& message) {
    res.status = status;
    res.set_content(json{{"error", message}}.dump(), "application/json");
}

json establishment_to_json(const Establishment& est, AlertLevel alert) {
    const Shelter shelter = est.get_shelter();
    const Coordinates coords = est.get_coordinates();
    return {
        {"id", est.get_id()},
        {"name", est.get_name()},
        {"type", est.get_type_name()},
        {"type_key", est.get_type_key()},
        {"lat", coords.get_latitude()},
        {"lng", coords.get_longitude()},
        {"address", est.get_address()},
        {"source", est.get_data_source()},
        {"opening_hours", est.get_opening_hours()},
        {"generator", est.has_backup_power()},
        {"shelter_capacity", shelter.get_capacity()},
        {"shelter_free", std::max(0, shelter.get_free_places())},
        {"shelter_underground", shelter.is_underground_shelter()},
        {"accessible", est.is_accessible(alert)},
        {"availability", {
            {"none", est.is_accessible(AlertLevel::None)},
            {"yellow", est.is_accessible(AlertLevel::Yellow)},
            {"red", est.is_accessible(AlertLevel::Red)}
        }}
    };
}

}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    json config = load_config();
    RoutePlanner planner;

    auto overrides = std::make_shared<RecordOverrides>();
    std::string overrides_setting = config.value("overrides_file", std::string());
    if (!overrides_setting.empty()) {
        auto overrides_file = resolve_existing(overrides_setting);
        if (overrides_file) {
            try {
                std::vector<LoadIssue> issues;
                *overrides = RecordOverrides::from_csv(*overrides_file, issues);
                std::cout << "[Уточнення] " << overrides->size() << " записів з " << *overrides_file << std::endl;
                for (const auto& issue : issues) {
                    std::cerr << "[Уточнення] " << *overrides_file << ", рядок " << issue.line
                              << " пропущено: " << issue.message << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[Помилка] Не вдалося прочитати " << *overrides_file << ": " << e.what() << std::endl;
                return 1;
            }
        } else {
            std::cout << "[Уточнення] Файл '" << overrides_setting << "' не знайдено, працюю без уточнень" << std::endl;
        }
    }

    const EstablishmentFactory factory = EstablishmentFactory::with_default_types();
    for (const auto& entry : config["data_files"]) {
        std::string configured = entry.get<std::string>();
        auto data_file = resolve_existing(configured);
        if (!data_file) {
            std::cerr << "[Помилка] Не знайдено файл даних '" << configured << "'" << std::endl;
            return 1;
        }
        try {
            DataLoader loader(std::make_unique<OverriddenDataSource>(
                                  std::make_unique<CsvDataSource>(*data_file), overrides),
                              factory);
            LoadSummary summary = loader.load_into(planner);
            std::cout << "[Дані] Завантажено " << summary.loaded << " закладів з " << *data_file << std::endl;
            for (const auto& issue : summary.issues) {
                std::cerr << "[Дані] " << *data_file << ", рядок " << issue.line << " пропущено: " << issue.message << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[Помилка] Не вдалося прочитати " << *data_file << ": " << e.what() << std::endl;
            return 1;
        }
    }
    for (const auto& item : overrides->unused()) {
        std::cerr << "[Уточнення] Рядок " << item.line << " ні з чим не збігся ("
                  << (item.match_by_id ? "id " + std::to_string(item.id) : item.osm_ref)
                  << "). Можливо, заклад зник з даних" << std::endl;
    }
    if (planner.get_establishments().empty()) {
        std::cerr << "[Попередження] Жодного закладу не завантажено" << std::endl;
    }

    httplib::Server svr;
#ifdef _WIN32
    svr.set_socket_options([](socket_t sock) {
        int enabled = 1;
        setsockopt(sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&enabled), sizeof(enabled));
    });
#endif

    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });

    svr.Options(R"(/api/.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    std::string web_root = resolve_web_root(config["web_root"].get<std::string>());
    if (web_root.empty() || !svr.set_mount_point("/", web_root)) {
        std::cerr << "[Помилка] Не знайдено папку фронтенду '" << config["web_root"].get<std::string>()
                  << "' з index.html" << std::endl;
        return 1;
    }
    svr.set_file_extension_and_mimetype_mapping("svg", "image/svg+xml");
    std::cout << "[Фронтенд] Роздаю статичні файли з " << web_root << std::endl;

    std::mutex planner_mutex;
    std::unique_ptr<AlertMonitor> alert_monitor;
    json alert_config = config["alerts"];
    std::string alert_region = alert_config.value("region", std::string("Київська область"));
    std::string alert_neighbor = alert_config.value("neighbor", std::string("м. Київ"));
    if (alert_config.value("enabled", true)) {
        int poll_seconds = std::max(60, alert_config.value("poll_seconds", 60));
        alert_monitor = std::make_unique<AlertMonitor>(
            std::make_unique<UbillingAlertSource>(alert_region, alert_neighbor), std::chrono::seconds(poll_seconds));
        alert_monitor->start([&planner, &planner_mutex](const AlertSnapshot& snapshot) {
            if (!snapshot.ok) {
                std::cerr << "[Тривога] " << snapshot.error << std::endl;
                return;
            }
            AlertLevel level = level_for(snapshot);
            std::lock_guard<std::mutex> lock(planner_mutex);
            if (level != planner.get_alert_level()) {
                planner.set_alert_level(level);
                std::cout << "[Тривога] Реальний рівень: " << alert_level_name(level) << std::endl;
            }
        });
        std::cout << "[Тривога] Стежу за регіоном '" << alert_region << "' кожні " << poll_seconds << " с" << std::endl;
    }

    svr.Get("/api/establishments", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(planner_mutex);
        AlertLevel level = planner.get_alert_level();
        if (read_level_param(req, level) == LevelParam::Invalid) {
            send_error(res, 400, "Невідомий рівень (очікується none, yellow або red)");
            return;
        }
        json response = json::array();
        for (const auto& est : planner.get_establishments()) {
            response.push_back(establishment_to_json(*est, level));
        }
        res.set_content(response.dump(), "application/json");
    });

    svr.Get("/api/alert", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(planner_mutex);
        json reply = {{"level", alert_level_name(planner.get_alert_level())}, {"live", false}};
        if (alert_monitor) {
            AlertSnapshot snapshot = alert_monitor->snapshot();
            reply["live"] = true;
            reply["ok"] = snapshot.ok;
            reply["stale"] = snapshot.stale;
            reply["source"] = alert_monitor->source_name();
            reply["region"] = alert_region;
            reply["region_alert"] = snapshot.region_alert;
            reply["region_changed"] = snapshot.region_changed;
            reply["neighbor"] = alert_neighbor;
            reply["neighbor_alert"] = snapshot.neighbor_alert;
            reply["neighbor_changed"] = snapshot.neighbor_changed;
            reply["checked_at"] = snapshot.fetched_at;
            if (!snapshot.error.empty()) reply["error"] = snapshot.error;
        }
        res.set_content(reply.dump(), "application/json");
    });

    svr.Post("/api/reports", [&](const httplib::Request& req, httplib::Response& res) {
        json body = json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.is_object() || !body.contains("id") || !body["id"].is_number_integer()) {
            send_error(res, 400, "Очікується JSON з цілим полем id");
            return;
        }
        int est_id = body["id"].get<int>();
        bool has_power = body.value("generator", true);
        int occupancy_delta = body.value("occupied", 0);
        bool hazard = body.value("hazard", false);
        User author(body.value("author", std::string("Диспетчер (веб)")));

        std::lock_guard<std::mutex> lock(planner_mutex);
        auto target = planner.find_by_id(est_id);
        if (!target) {
            send_error(res, 404, "Заклад з id " + std::to_string(est_id) + " не знайдено");
            return;
        }

        int occupancy_before = target->get_shelter().get_current_occupancy();
        std::shared_ptr<Report> report;
        if (hazard) {
            report = std::make_shared<HazardReport>(author, true);
        } else {
            report = std::make_shared<StatusReport>(author, has_power, occupancy_delta);
        }
        planner.process_user_report(report, target);

        bool occupancy_applied = hazard || occupancy_delta == 0 ||
                                 target->get_shelter().get_current_occupancy() != occupancy_before;
        std::cout << "[Звіт] " << target->get_name() << " (id " << est_id << "): "
                  << (hazard ? "небезпека, заклад закрито" : "оновлено стан") << std::endl;

        json reply = {
            {"status", "applied"},
            {"occupancy_applied", occupancy_applied},
            {"establishment", establishment_to_json(*target, planner.get_alert_level())}
        };
        if (!occupancy_applied) {
            reply["warning"] = "Кількість людей не змінено: результат виходить за межі місткості укриття";
        }
        res.set_content(reply.dump(), "application/json");
    });

    svr.Get("/api/nearest-shelter", [&](const httplib::Request& req, httplib::Response& res) {
        auto user_coords = parse_coordinates(req);
        if (!user_coords) {
            send_error(res, 400, "Потрібні коректні параметри lat і lng");
            return;
        }

        std::lock_guard<std::mutex> lock(planner_mutex);
        AlertLevel level = planner.get_alert_level();
        if (read_level_param(req, level) == LevelParam::Invalid) {
            send_error(res, 400, "Невідомий рівень (очікується none, yellow або red)");
            return;
        }
        auto shelter = planner.find_nearest_shelter(*user_coords, level);
        if (!shelter) {
            res.set_content(json{{"found", false},
                                 {"message", "Доступних укриттів із вільними місцями не знайдено"}}.dump(),
                            "application/json");
            return;
        }

        double distance_km = user_coords->distance_to(shelter->get_coordinates());
        std::cout << "[Укриття] Найближче: " << shelter->get_name() << " (" << distance_km << " км)" << std::endl;

        auto estimate_json = [&](TravelMode mode) {
            TravelEstimate estimate = planner.estimate_travel(*user_coords, shelter->get_coordinates(), mode);
            return json{{"distance_km", std::round(estimate.distance_km * 100.0) / 100.0},
                        {"minutes", std::max(1.0, std::round(estimate.minutes))}};
        };

        json result = {
            {"estimates", {{"walk", estimate_json(TravelMode::Walk)}, {"drive", estimate_json(TravelMode::Drive)}}},
            {"found", true},
            {"id", shelter->get_id()},
            {"name", shelter->get_name()},
            {"lat", shelter->get_coordinates().get_latitude()},
            {"lng", shelter->get_coordinates().get_longitude()},
            {"distance_km", std::round(distance_km * 100.0) / 100.0},
            {"free_places", shelter->get_shelter().get_free_places()}
        };
        res.set_content(result.dump(), "application/json");
    });

    std::string host = config["host"].get<std::string>();
    int port = config["port"].get<int>();
    std::cout << "Сервер SafeMap Kyiv працює на http://localhost:" << port << std::endl;
    if (!svr.listen(host, port)) {
        std::cerr << "[Помилка] Не вдалося запустити сервер на порту " << port << " (можливо, його вже зайняв інший екземпляр SafeMap)" << std::endl;
        return 1;
    }

    return 0;
}