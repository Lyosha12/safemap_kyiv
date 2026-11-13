#include "Infrastructure.h"
#include <cmath>

using namespace std;

Coordinates::Coordinates(double lat, double lon) : latitude(lat), longitude(lon) {}

namespace {
constexpr double kEarthRadiusKm = 6371.0;
constexpr double kPi = 3.14159265358979323846;

double to_radians(double degrees) {
    return degrees * kPi / 180.0;
}
}

double Coordinates::calculate_distance_to(const Coordinates& other) const {
    double d_lat = to_radians(other.latitude - latitude);
    double d_lon = to_radians(other.longitude - longitude);
    double a = sin(d_lat / 2) * sin(d_lat / 2) +
               cos(to_radians(latitude)) * cos(to_radians(other.latitude)) *
               sin(d_lon / 2) * sin(d_lon / 2);
    return 2.0 * kEarthRadiusKm * atan2(sqrt(a), sqrt(1.0 - a));
}

bool Coordinates::is_within_radius(const Coordinates& center, double radius) const {
    return calculate_distance_to(center) <= radius;
}

Shelter::Shelter(int cap, bool underground) : capacity(cap), current_occupancy(0), is_underground(underground) {}

bool Shelter::try_admit_people(int count) {
    int updated = current_occupancy + count;
    if (updated >= 0 && updated <= capacity) {
        current_occupancy += count;
        return true;
    }
    return false;
}

double Shelter::get_occupancy_rate() const {
    if (capacity == 0) return 0.0;
    return static_cast<double>(current_occupancy) / capacity;
}

void Shelter::evacuate() {
    current_occupancy = 0;
}