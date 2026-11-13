#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#endif

#include <iostream>
#include <fstream>
#include <memory>
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
#include <windows.h>

using json = nlohmann::json;

namespace {

const std::vector<std::string> kSearchPrefixes = {"", "../"};

json load_config() {
    json defaults = {{"host", "0.0.0.0"}, {"port", 8080}, {"web_root", "web"},
                     {"data_file", "data/establishments.csv"}};
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

}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    json config = load_config();
    RoutePlanner planner;

    auto data_file = resolve_existing(config["data_file"].get<std::string>());
    if (!data_file) {
        std::cerr << "[Помилка] Не знайдено файл даних '" << config["data_file"].get<std::string>() << "'" << std::endl;
        return 1;
    }
    try {
        DataLoader loader(std::make_unique<CsvDataSource>(*data_file), EstablishmentFactory::with_default_types());
        LoadSummary summary = loader.load_into(planner);
        std::cout << "[Дані] Завантажено " << summary.loaded << " закладів з " << *data_file << std::endl;
        for (const auto& issue : summary.issues) {
            std::cerr << "[Дані] Рядок " << issue.line << " пропущено: " << issue.message << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Помилка] Не вдалося прочитати " << *data_file << ": " << e.what() << std::endl;
        return 1;
    }

    httplib::Server svr;

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

    svr.Get("/api/establishments", [&planner](const httplib::Request&, httplib::Response& res) {
        json response = json::array();
        const auto& list = planner.get_establishments();
        auto alert = planner.get_alert_level();

        for (size_t i = 0; i < list.size(); ++i) {
            const auto& est = list[i];
            auto coords = est->get_coordinates();
            int cap = est->get_shelter().get_capacity();
            int occ = est->get_shelter().get_current_occupancy();
            int freeShelter = cap - occ;

            json item = {
                {"id", static_cast<int>(i + 1)},
                {"name", est->get_name()},
                {"type", est->get_type_name()},
                {"lat", coords.get_latitude()},
                {"lng", coords.get_longitude()},
                {"generator", est->has_backup_power()},
                {"shelter_capacity", cap},
                {"shelter_free", freeShelter > 0 ? freeShelter : 0},
                {"accessible", est->is_accessible(alert)}
            };
            response.push_back(item);
        }

        res.set_content(response.dump(4), "application/json");
    });

    svr.Post("/api/alert", [&planner](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string level = body.value("level", "none");

            if (level == "red") {
                planner.set_alert_level(AlertLevel::Red);
            } else if (level == "yellow") {
                planner.set_alert_level(AlertLevel::Yellow);
            } else {
                planner.set_alert_level(AlertLevel::None);
            }

            json reply = {{"status", "success"}, {"level", level}};
            res.set_content(reply.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error": "Invalid JSON"})", "application/json");
        }
    });

    svr.Post("/api/reports", [&planner](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            int estId = body.value("id", -1);
            bool hasPower = body.value("generator", true);
            int addOccupancy = body.value("occupied", 0);

            std::cout << "[Звіт отримано] ID: " << estId
                      << ", Генератор: " << (hasPower ? "Так" : "Ні")
                      << ", Зміна кількості людей: " << addOccupancy << std::endl;

            json reply = {{"status", "received"}};
            res.set_content(reply.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error": "Invalid request"})", "application/json");
        }
    });

    svr.Get("/api/nearest-shelter", [&planner](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("lat") || !req.has_param("lng")) {
            res.status = 400;
            res.set_content(R"({"error": "Missing coordinates"})", "application/json");
            return;
        }

        double userLat = std::stod(req.get_param_value("lat"));
        double userLng = std::stod(req.get_param_value("lng"));
        Coordinates userCoords(userLat, userLng);

        const auto& list = planner.get_establishments();
        std::shared_ptr<Establishment> bestShelter = nullptr;
        double minDistance = 1e9;

        std::cout << "\n[Пошук укриття] Клік користувача: " << userLat << ", " << userLng << std::endl;

        for (const auto& est : list) {
            int cap = est->get_shelter().get_capacity();
            int occ = est->get_shelter().get_current_occupancy();
            int freeShelter = cap - occ;

            double dist = userCoords.distance_to(est->get_coordinates());
            std::cout << " -> Перевірка: " << est->get_name()
                      << " | Місць: " << freeShelter << "/" << cap
                      << " | Відстань: " << dist << " км" << std::endl;

            if (cap > 0 && freeShelter > 0) {
                if (dist < minDistance) {
                    minDistance = dist;
                    bestShelter = est;
                }
            }
        }

        if (bestShelter) {
            int freePlaces = bestShelter->get_shelter().get_capacity() - bestShelter->get_shelter().get_current_occupancy();
            std::cout << "[Знайдено!] Найближче: " << bestShelter->get_name() << " (" << minDistance << " км)\n" << std::endl;

            json result = {
                {"found", true},
                {"name", bestShelter->get_name()},
                {"lat", bestShelter->get_coordinates().get_latitude()},
                {"lng", bestShelter->get_coordinates().get_longitude()},
                {"distance_km", std::round(minDistance * 100.0) / 100.0},
                {"free_places", freePlaces}
            };
            res.set_content(result.dump(4), "application/json");
        } else {
            std::cout << "[Не знайдено] Жоден об'єкт не підійшов під критерії укриття.\n" << std::endl;
            res.set_content(R"({"found": false, "message": "Доступних укриттів із вільними місцями не знайдено"})", "application/json");
        }
    });

    std::string host = config["host"].get<std::string>();
    int port = config["port"].get<int>();
    std::cout << "Сервер SafeMap Kyiv працює на http://localhost:" << port << std::endl;
    if (!svr.listen(host, port)) {
        std::cerr << "[Помилка] Не вдалося запустити сервер на порту " << port << std::endl;
        return 1;
    }

    return 0;
}