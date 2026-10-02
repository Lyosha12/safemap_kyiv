#include "Crowdsourcing.h"
#include <iostream>

using namespace std;

User::User(string name) : username(name), trust_score(50.0), reports_submitted(0) {}

void User::increase_trust(double amount) {
    trust_score += amount;
    if (trust_score > 100.0) trust_score = 100.0;
    reports_submitted++;
}

void User::penalize_trust(double amount) {
    trust_score -= amount;
    if (trust_score < 0.0) trust_score = 0.0;
}

bool User::is_reliable() const {
    return trust_score >= 40.0;
}

string User::get_username() const { return username; }

Report::Report(User a) : author(a) {
    timestamp = time(nullptr);
}

bool Report::is_expired(time_t current_time, int max_age_seconds) const {
    return difftime(current_time, timestamp) > max_age_seconds;
}

double Report::calculate_report_weight() const {
    return author.is_reliable() ? 1.5 : 0.5;
}

StatusReport::StatusReport(User a, bool gen, int influx)
    : Report(a), generator_working(gen), observed_shelter_influx(influx) {}

void StatusReport::apply_update(Establishment& target) const {
    if (author.is_reliable()) {
        target.apply_crowdsource_data(generator_working, observed_shelter_influx);
        cout << "[System] " << author.get_username() << " updated establishment status." << endl;
    }
}

int StatusReport::estimate_wait_time() const {
    return observed_shelter_influx * 2;
}

HazardReport::HazardReport(User a, bool blocked)
    : Report(a), is_path_blocked(blocked) {}

void HazardReport::apply_update(Establishment& target) const {
    if (author.is_reliable() && is_path_blocked) {
        target.force_close();
        cout << "[Warning] Path blocked! Establishment force closed by report." << endl;
    }
}

bool HazardReport::check_hazard_severity() const {
    return is_path_blocked;
}