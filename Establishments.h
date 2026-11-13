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
    int get_id() const { return id; }
};

class Establishment : public MapNode {
protected:
    std::string name;
    Shelter base_shelter;
    bool has_generator;
    bool is_temporarily_closed;
    std::string address;
    std::string data_source;
    std::string opening_hours;

public:
    Establishment(int id, Coordinates loc, std::string n, Shelter s, bool gen);
    virtual bool is_accessible(AlertLevel level) const = 0;
    void toggle_generator(bool state);
    void force_close();
    void apply_crowdsource_data(bool gen_state, int shelter_influx);
    bool can_shelter_more() const;
    void set_origin(std::string addr, std::string source);
    void set_opening_hours(std::string schedule) { opening_hours = std::move(schedule); }
    virtual std::string get_name() const { return name; }
    virtual std::string get_type_name() const { return "Заклад"; }
    virtual std::string get_type_key() const = 0;
    Coordinates get_coordinates() const { return location; }
    bool has_backup_power() const { return has_generator; }
    Shelter get_shelter() const { return base_shelter; }
    const std::string& get_address() const { return address; }
    const std::string& get_data_source() const { return data_source; }
    const std::string& get_opening_hours() const { return opening_hours; }
};

class Supermarket : public Establishment {
private:
    bool has_food_reserves;

public:
    Supermarket(int id, Coordinates loc, std::string n, Shelter s, bool gen, bool food);
    bool is_accessible(AlertLevel level) const override;
    std::string get_type_name() const override { return "Супермаркет"; }
    std::string get_type_key() const override { return "supermarket"; }
    void restock_reserves();
};

class GroceryStore : public Supermarket {
public:
    GroceryStore(int id, Coordinates loc, std::string n, Shelter s, bool gen);
    std::string get_type_name() const override { return "Продукти"; }
    std::string get_type_key() const override { return "grocery"; }
};

class PostOffice : public Establishment {
private:
    double max_parcel_weight;

public:
    PostOffice(int id, Coordinates loc, std::string n, Shelter s, bool gen, double max_weight);
    bool is_accessible(AlertLevel level) const override;
    std::string get_type_name() const override { return "Пошта"; }
    std::string get_type_key() const override { return "post_office"; }
    bool can_accept_parcel(double weight) const;
};

class Pharmacy : public Establishment {
private:
    bool open_24h;

public:
    Pharmacy(int id, Coordinates loc, std::string n, Shelter s, bool gen, bool round_the_clock);
    bool is_accessible(AlertLevel level) const override;
    std::string get_type_name() const override { return "Аптека"; }
    std::string get_type_key() const override { return "pharmacy"; }
};

class PublicShelter : public Establishment {
public:
    PublicShelter(int id, Coordinates loc, std::string n, Shelter s, bool gen);
    bool is_accessible(AlertLevel level) const override;
    std::string get_type_name() const override { return "Укриття"; }
    std::string get_type_key() const override { return "shelter"; }
};

class Cafe : public Establishment {
private:
    bool has_wifi;

public:
    Cafe(int id, Coordinates loc, std::string n, Shelter s, bool gen, bool wifi);
    bool is_accessible(AlertLevel level) const override;
    std::string get_type_name() const override { return "Кафе"; }
    std::string get_type_key() const override { return "cafe"; }
    bool offers_wifi() const { return has_wifi; }
};

class GasStation : public Establishment {
private:
    bool open_24h;

public:
    GasStation(int id, Coordinates loc, std::string n, Shelter s, bool gen, bool round_the_clock);
    bool is_accessible(AlertLevel level) const override;
    std::string get_type_name() const override { return "АЗС"; }
    std::string get_type_key() const override { return "gas_station"; }
};