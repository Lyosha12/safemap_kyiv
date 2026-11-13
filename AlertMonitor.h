#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include "Establishments.h"

struct AlertSnapshot {
    bool ok = false;
    bool stale = false;
    bool region_alert = false;
    bool neighbor_alert = false;
    std::string region_changed;
    std::string neighbor_changed;
    std::string fetched_at;
    std::string error;
};

AlertLevel level_for(const AlertSnapshot& snapshot);

class IAlertSource {
public:
    virtual ~IAlertSource() = default;
    virtual AlertSnapshot fetch() = 0;
    virtual std::string name() const = 0;
};

class UbillingAlertSource : public IAlertSource {
public:
    UbillingAlertSource(std::string region, std::string neighbor);
    AlertSnapshot fetch() override;
    std::string name() const override { return "ubilling.net.ua/aerialalerts"; }

private:
    std::string region;
    std::string neighbor;
};

class AlertMonitor {
public:
    using Listener = std::function<void(const AlertSnapshot&)>;

    AlertMonitor(std::unique_ptr<IAlertSource> source, std::chrono::seconds interval);
    ~AlertMonitor();

    void start(Listener listener);
    void stop();
    AlertSnapshot snapshot() const;
    std::string source_name() const;

private:
    void run();

    std::unique_ptr<IAlertSource> source;
    std::chrono::seconds interval;
    Listener listener;
    mutable std::mutex state_mutex;
    std::condition_variable wake;
    AlertSnapshot latest;
    std::atomic<bool> running{false};
    std::thread worker;
};
