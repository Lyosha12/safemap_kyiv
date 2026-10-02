#ifndef SAFEMAP_KYIV_INFRASTRUCTURE_H
#define SAFEMAP_KYIV_INFRASTRUCTURE_H

#endif //SAFEMAP_KYIV_INFRASTRUCTURE_H
#pragma once

class Coordinates {
private:
    double latitude;
    double longitude;
public:
    Coordinates(double lat, double lon);
    double calculate_distance_to(const Coordinates& other) const;
    bool is_within_radius(const Coordinates& center, double radius) const;
};

class Shelter {
private:
    int capacity;
    int current_occupancy;
    bool is_underground;
public:
    Shelter(int cap, bool underground);
    bool try_admit_people(int count);
    double get_occupancy_rate() const;
    void evacuate();
};