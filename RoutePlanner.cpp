#include "RoutePlanner.h"

using namespace std;

namespace {

struct TravelProfile {
    double detour_factor;
    double speed_kmh;
};

TravelProfile profile_for(TravelMode mode) {
    switch (mode) {
        case TravelMode::Drive: return {1.4, 30.0};
        default: return {1.3, 5.0};
    }
}

}

RoutePlanner::RoutePlanner() : current_alert_level(AlertLevel::None) {}

void RoutePlanner::set_alert_level(AlertLevel level) {
    current_alert_level = level;
}

void RoutePlanner::add_establishment(shared_ptr<Establishment> est) {
    map_nodes.push_back(est);
}

shared_ptr<Establishment> RoutePlanner::find_nearest_shelter(const Coordinates& from_loc) const {
    return find_nearest_shelter(from_loc, current_alert_level);
}

shared_ptr<Establishment> RoutePlanner::find_nearest_shelter(const Coordinates& from_loc, AlertLevel level) const {
    shared_ptr<Establishment> nearest = nullptr;
    double min_distance = -1.0;

    for (const auto& node : map_nodes) {
        if (node->is_accessible(level) && node->can_shelter_more()) {
            double dist = from_loc.distance_to(node->get_coordinates());
            if (min_distance < 0 || dist < min_distance) {
                min_distance = dist;
                nearest = node;
            }
        }
    }
    return nearest;
}

shared_ptr<Establishment> RoutePlanner::find_by_id(int id) const {
    for (const auto& node : map_nodes) {
        if (node->get_id() == id) return node;
    }
    return nullptr;
}

void RoutePlanner::process_user_report(shared_ptr<Report> report, shared_ptr<Establishment> target) {
    if (target && report) {
        report->apply_update(*target);
    }
}

double RoutePlanner::evaluate_route_safety(shared_ptr<Establishment> destination) const {
    if (current_alert_level == AlertLevel::Red) return 0.0;
    if (destination->is_accessible(current_alert_level)) return 100.0;
    return 50.0;
}

TravelEstimate RoutePlanner::estimate_travel(const Coordinates& from, const Coordinates& to, TravelMode mode) const {
    TravelProfile profile = profile_for(mode);
    double distance_km = from.distance_to(to) * profile.detour_factor;
    return {distance_km, distance_km / profile.speed_kmh * 60.0};
}
