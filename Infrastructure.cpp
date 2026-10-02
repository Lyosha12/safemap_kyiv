#include "Infrastructure.h"
#include <cmath>

using namespace std;

Coordinates::Coordinates(double lat, double lon) : latitude(lat), longitude(lon) {}

double Coordinates::calculate_distance_to(const Coordinates& other) const {
    return 0.0; 
}

bool Coordinates::is_within_radius(const Coordinates& center, double radius) const {
    return calculate_distance_to(center) <= radius;
}

Shelter::Shelter(int cap, bool underground) : capacity(cap), current_occupancy(0), is_underground(underground) {}

bool Shelter::try_admit_people(int count) {
    if (current_occupancy + count <= capacity) {
        current_occupancy += count;
        return true;
    }
    return false;
}

double Shelter::get_occupancy_rate() const {
    return static_cast<double>(current_occupancy) / capacity;
}

void Shelter::evacuate() {
    current_occupancy = 0;
}