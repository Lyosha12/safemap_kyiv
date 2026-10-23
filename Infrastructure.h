#pragma once

class DetailsDialog;

class Coordinates {
private:
    double latitude;
    double longitude;

public:
    Coordinates(double lat, double lon);

    double calculate_distance_to(const Coordinates& other) const;
    bool is_within_radius(const Coordinates& center, double radius) const;

    double distance_to(const Coordinates& other) const {
        return calculate_distance_to(other);
    }

    double get_latitude() const { return latitude; }
    double get_longitude() const { return longitude; }

    friend class DetailsDialog;
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

    int get_capacity() const { return capacity; }
    int get_current_occupancy() const { return current_occupancy; }
};