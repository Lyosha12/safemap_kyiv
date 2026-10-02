#include "Establishments.h"

using namespace std;

MapNode::MapNode(int id, Coordinates loc) : id(id), location(loc) {}

double MapNode::distance_to(const MapNode& other) const {
    return location.calculate_distance_to(other.get_location());
}

Coordinates MapNode::get_location() const {
    return location;
}

Establishment::Establishment(int id, Coordinates loc, string n, Shelter s, bool gen)
    : MapNode(id, loc), name(n), base_shelter(s), has_generator(gen), is_temporarily_closed(false) {}

void Establishment::toggle_generator(bool state) {
    has_generator = state;
}

void Establishment::force_close() {
    is_temporarily_closed = true;
}

void Establishment::apply_crowdsource_data(bool gen_state, int shelter_influx) {
    has_generator = gen_state;
    base_shelter.try_admit_people(shelter_influx);
}

bool Establishment::can_shelter_more() const {
    return base_shelter.get_occupancy_rate() < 1.0;
}

Supermarket::Supermarket(int id, Coordinates loc, string n, Shelter s, bool gen, bool food)
    : Establishment(id, loc, n, s, gen), has_food_reserves(food) {}

bool Supermarket::is_accessible(AlertLevel level) const {
    if (is_temporarily_closed) return false;
    if (level == AlertLevel::Red) return false;
    if (level == AlertLevel::Yellow) return has_generator;
    return true;
}

void Supermarket::restock_reserves() {
    has_food_reserves = true;
}

PostOffice::PostOffice(int id, Coordinates loc, string n, Shelter s, bool gen, double max_weight)
    : Establishment(id, loc, n, s, gen), max_parcel_weight(max_weight) {}

bool PostOffice::is_accessible(AlertLevel level) const {
    if (is_temporarily_closed) return false;
    return level == AlertLevel::None;
}

bool PostOffice::can_accept_parcel(double weight) const {
    return weight <= max_parcel_weight;
}