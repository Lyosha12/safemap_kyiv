#ifndef SAFEMAP_KYIV_CROWDSOURCING_H
#define SAFEMAP_KYIV_CROWDSOURCING_H

#endif //SAFEMAP_KYIV_CROWDSOURCING_H
#pragma once
#include <string>
#include <ctime>
#include "Establishments.h"

class User {
private:
    std::string username;
    double trust_score;
    int reports_submitted;
public:
    User(std::string name);

    void increase_trust(double amount);
    void penalize_trust(double amount);
    bool is_reliable() const;
    std::string get_username() const;
};

class Report {
protected:
    User author;
    std::time_t timestamp;
public:
    Report(User a);
    virtual ~Report() = default;

    virtual void apply_update(Establishment& target) const = 0;

    bool is_expired(std::time_t current_time, int max_age_seconds) const;
    double calculate_report_weight() const;
};

class StatusReport : public Report {
private:
    bool generator_working;
    int observed_shelter_influx;
public:
    StatusReport(User a, bool gen, int influx);

    void apply_update(Establishment& target) const override;
    int estimate_wait_time() const;
};

class HazardReport : public Report {
private:
    bool is_path_blocked;
public:
    HazardReport(User a, bool blocked);

    void apply_update(Establishment& target) const override;
    bool check_hazard_severity() const;
};