#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include "httplib.h"
#include "json.hpp"
#include "RoutePlanner.h"
#include "Establishments.h"
#include "Infrastructure.h"
#include "Crowdsourcing.h"

using json = nlohmann::json;

int main() {
    RoutePlanner planner;

    auto silpo = std::make_shared<Supermarket>(
        1,
        Coordinates(50.4385, 30.5234),
        "Сільпо (ТРЦ Gulliver)",
        Shelter(150, true),
        true,
        true
    );
    planner.add_establishment(silpo);

    auto novaPoshta = std::make_shared<PostOffice>(
        2,
        Coordinates(50.4511, 30.5211),
        "Нова Пошта №1",
        Shelter(0, false),
        false,
        30.0
    );
    planner.add_establishment(novaPoshta);

    auto oceanPlaza = std::make_shared<Supermarket>(
        3,
        Coordinates(50.4128, 30.5245),
        "Ашан (Ocean Plaza)",
        Shelter(300, true),
        true,
        true
    );
    planner.add_establishment(oceanPlaza);

    httplib::Server svr;

    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });

    svr.Options(R"(/api/.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        std::string html_path = "../index.html";
        std::ifstream file(html_path);
        if (!file.is_open()) {
            html_path = "index.html";
            file.open(html_path);
        }

        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            res.set_content(buffer.str(), "text/html; charset=utf-8");
        } else {
            res.status = 404;
            res.set_content("<h1>Файл index.html не знайдено!</h1>", "text/html; charset=utf-8");
        }
    });

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

    std::cout << "Сервер SafeMap Kyiv працює на http://localhost:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}