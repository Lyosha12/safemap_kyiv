#ifndef SAFEMAP_KYIV_ESTABLISHMENTS_H
#define SAFEMAP_KYIV_ESTABLISHMENTS_H

#endif //SAFEMAP_KYIV_ESTABLISHMENTS_H
#pragma once
#include <string>
#include "Infrastructure.h"

enum class AlertLevel {
    None,
    Yellow,
    Red
};

class MapNode {
protected:
    int id;
    Coordinates location;
public:
    MapNode(int id, Coordinates loc);
    virtual ~MapNode() = default;
    double distance_to(const MapNode& other) const;
    Coordinates get_location() const;
};

class Establishment : public MapNode {
protected:
    std::string name;
    Shelter base_shelter;
    bool has_generator;
    bool is_temporarily_closed;
public:
    Establishment(int id, Coordinates loc, std::string n, Shelter s, bool gen);
    virtual bool is_accessible(AlertLevel level) const = 0;
    void toggle_generator(bool state);
    void force_close();
    void apply_crowdsource_data(bool gen_state, int shelter_influx);
    bool can_shelter_more() const;
};

class Supermarket : public Establishment {
private:
    bool has_food_reserves;
public:
    Supermarket(int id, Coordinates loc, std::string n, Shelter s, bool gen, bool food);
    bool is_accessible(AlertLevel level) const override;
    void restock_reserves();
};

class PostOffice : public Establishment {
private:
    double max_parcel_weight;
public:
    PostOffice(int id, Coordinates loc, std::string n, Shelter s, bool gen, double max_weight);
    bool is_accessible(AlertLevel level) const override;
    bool can_accept_parcel(double weight) const;
};