#include "RoutePlanner.h"
#include <iostream>

using namespace std;

RoutePlanner::RoutePlanner() : current_alert_level(AlertLevel::None) {}

void RoutePlanner::set_alert_level(AlertLevel level) {
    current_alert_level = level;
    cout << "Alert level updated." << endl;
}

void RoutePlanner::add_establishment(shared_ptr<Establishment> est) {
    map_nodes.push_back(est);
}

shared_ptr<Establishment> RoutePlanner::find_nearest_shelter(const Coordinates& from_loc) const {
    shared_ptr<Establishment> nearest = nullptr;
    double min_distance = -1.0;

    for (const auto& node : map_nodes) {
        if (node->is_accessible(current_alert_level) && node->can_shelter_more()) {
            double dist = node->distance_to(MapNode(0, from_loc));
            if (min_distance < 0 || dist < min_distance) {
                min_distance = dist;
                nearest = node;
            }
        }
    }
    return nearest;
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