#ifndef SAFEMAP_KYIV_ROUTEPLANNER_H
#define SAFEMAP_KYIV_ROUTEPLANNER_H

#endif //SAFEMAP_KYIV_ROUTEPLANNER_H
#pragma once
#include <vector>
#include <memory>
#include <functional>
#include "Establishments.h"
#include "Crowdsourcing.h"

class RoutePlanner {
private:
    std::vector<std::shared_ptr<Establishment>> map_nodes;
    AlertLevel current_alert_level;
public:
    RoutePlanner();

    void set_alert_level(AlertLevel level);
    void add_establishment(std::shared_ptr<Establishment> est);

    template <typename Predicate>
    std::vector<std::shared_ptr<Establishment>> find_safe_nodes(Predicate condition) const {
        std::vector<std::shared_ptr<Establishment>> result;
        for (const auto& node : map_nodes) {
            if (condition(node) && node->is_accessible(current_alert_level)) {
                result.push_back(node);
            }
        }
        return result;
    }

    std::shared_ptr<Establishment> find_nearest_shelter(const Coordinates& from_loc) const;
    void process_user_report(std::shared_ptr<Report> report, std::shared_ptr<Establishment> target);
    double evaluate_route_safety(std::shared_ptr<Establishment> destination) const;
};