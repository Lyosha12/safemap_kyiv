#include "AlertMonitor.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#include "httplib.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

namespace {

string now_string() {
    time_t now = time(nullptr);
    tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    ostringstream out;
    out << put_time(&local, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

}

AlertLevel level_for(const AlertSnapshot& snapshot) {
    if (!snapshot.ok) return AlertLevel::None;
    if (snapshot.region_alert) return AlertLevel::Red;
    if (snapshot.neighbor_alert) return AlertLevel::Yellow;
    return AlertLevel::None;
}

UbillingAlertSource::UbillingAlertSource(string region_name, string neighbor_name)
    : region(move(region_name)), neighbor(move(neighbor_name)) {}

AlertSnapshot UbillingAlertSource::fetch() {
    AlertSnapshot snapshot;
    snapshot.fetched_at = now_string();
#ifdef SAFEMAP_REAL_ALERTS
    httplib::SSLClient client("ubilling.net.ua", 443);
    client.set_connection_timeout(10);
    client.set_read_timeout(15);
    client.set_follow_location(true);

    httplib::Headers headers = {{"User-Agent", "SafeMap-student-project"}};
    auto response = client.Get("/aerialalerts/", headers);
    if (!response) {
        snapshot.error = "немає з'єднання з сервісом тривог (" + httplib::to_string(response.error()) + ")";
        return snapshot;
    }
    if (response->status != 200) {
        snapshot.error = "сервіс тривог відповів кодом " + to_string(response->status);
        return snapshot;
    }

    json body = json::parse(response->body, nullptr, false);
    if (body.is_discarded() || !body.contains("states") || !body["states"].is_object()) {
        snapshot.error = "сервіс тривог повернув некоректні дані";
        return snapshot;
    }

    const json& states = body["states"];
    if (!states.contains(region)) {
        snapshot.error = "регіон '" + region + "' не знайдено у відповіді сервісу";
        return snapshot;
    }

    snapshot.region_alert = states[region].value("alertnow", false);
    snapshot.region_changed = states[region].value("changed", string());
    if (!neighbor.empty() && states.contains(neighbor)) {
        snapshot.neighbor_alert = states[neighbor].value("alertnow", false);
        snapshot.neighbor_changed = states[neighbor].value("changed", string());
    }
    snapshot.ok = true;
#else
    snapshot.error = "сервер зібрано без OpenSSL, реальна тривога недоступна";
#endif
    return snapshot;
}

AlertMonitor::AlertMonitor(unique_ptr<IAlertSource> src, chrono::seconds poll_interval)
    : source(move(src)), interval(poll_interval) {}

AlertMonitor::~AlertMonitor() {
    stop();
}

void AlertMonitor::start(Listener on_update) {
    if (running.exchange(true)) return;
    listener = move(on_update);
    worker = thread(&AlertMonitor::run, this);
}

void AlertMonitor::stop() {
    {
        lock_guard<mutex> lock(state_mutex);
        if (!running.exchange(false)) return;
    }
    wake.notify_all();
    if (worker.joinable()) worker.join();
}

AlertSnapshot AlertMonitor::snapshot() const {
    lock_guard<mutex> lock(state_mutex);
    return latest;
}

string AlertMonitor::source_name() const {
    return source->name();
}

void AlertMonitor::run() {
    while (running) {
        AlertSnapshot fresh = source->fetch();
        AlertSnapshot published;
        {
            lock_guard<mutex> lock(state_mutex);
            if (!fresh.ok && latest.ok) {
                latest.stale = true;
                latest.error = fresh.error;
            } else {
                latest = fresh;
            }
            published = latest;
        }
        if (listener) listener(published);

        unique_lock<mutex> lock(state_mutex);
        wake.wait_for(lock, interval, [this] { return !running; });
    }
}
