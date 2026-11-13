#include "EstablishmentFactory.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

using namespace std;

namespace {

string to_lower_ascii(string value) {
    transform(value.begin(), value.end(), value.begin(),
              [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return value;
}

}

bool parse_flag(const string& raw, bool fallback) {
    string value = to_lower_ascii(raw);
    if (value == "1" || value == "true" || value == "yes" || value == "y" || value == "так") return true;
    if (value == "0" || value == "false" || value == "no" || value == "n" || value == "ні") return false;
    return fallback;
}

bool EstablishmentRecord::extra_flag(const string& key, bool fallback) const {
    auto it = extras.find(key);
    return it == extras.end() ? fallback : parse_flag(it->second, fallback);
}

double EstablishmentRecord::extra_number(const string& key, double fallback) const {
    auto it = extras.find(key);
    if (it == extras.end() || it->second.empty()) return fallback;
    try {
        return stod(it->second);
    } catch (const exception&) {
        return fallback;
    }
}

EstablishmentFactory EstablishmentFactory::with_default_types() {
    EstablishmentFactory factory;

    factory.register_type("supermarket", [](const EstablishmentRecord& r) {
        return make_shared<Supermarket>(r.id, Coordinates(r.lat, r.lng), r.name,
                                        Shelter(r.shelter_capacity, r.underground), r.generator,
                                        r.extra_flag("food_reserves", true));
    });
    factory.register_type("grocery", [](const EstablishmentRecord& r) {
        return make_shared<GroceryStore>(r.id, Coordinates(r.lat, r.lng), r.name,
                                         Shelter(r.shelter_capacity, r.underground), r.generator);
    });
    factory.register_type("post_office", [](const EstablishmentRecord& r) {
        return make_shared<PostOffice>(r.id, Coordinates(r.lat, r.lng), r.name,
                                       Shelter(r.shelter_capacity, r.underground), r.generator,
                                       r.extra_number("max_parcel_kg", 30.0));
    });
    factory.register_type("pharmacy", [](const EstablishmentRecord& r) {
        return make_shared<Pharmacy>(r.id, Coordinates(r.lat, r.lng), r.name,
                                     Shelter(r.shelter_capacity, r.underground), r.generator,
                                     r.extra_flag("open_24h"));
    });
    factory.register_type("shelter", [](const EstablishmentRecord& r) {
        return make_shared<PublicShelter>(r.id, Coordinates(r.lat, r.lng), r.name,
                                          Shelter(r.shelter_capacity, r.underground), r.generator);
    });
    factory.register_type("cafe", [](const EstablishmentRecord& r) {
        return make_shared<Cafe>(r.id, Coordinates(r.lat, r.lng), r.name,
                                 Shelter(r.shelter_capacity, r.underground), r.generator,
                                 r.extra_flag("wifi"));
    });
    factory.register_type("gas_station", [](const EstablishmentRecord& r) {
        return make_shared<GasStation>(r.id, Coordinates(r.lat, r.lng), r.name,
                                       Shelter(r.shelter_capacity, r.underground), r.generator,
                                       r.extra_flag("open_24h"));
    });

    return factory;
}

void EstablishmentFactory::register_type(const string& key, Creator creator) {
    creators[to_lower_ascii(key)] = move(creator);
}

bool EstablishmentFactory::supports(const string& key) const {
    return creators.count(to_lower_ascii(key)) > 0;
}

vector<string> EstablishmentFactory::supported_types() const {
    vector<string> keys;
    for (const auto& entry : creators) keys.push_back(entry.first);
    return keys;
}

shared_ptr<Establishment> EstablishmentFactory::create(const EstablishmentRecord& record) const {
    auto it = creators.find(to_lower_ascii(record.type));
    if (it == creators.end()) {
        throw invalid_argument("невідомий тип закладу '" + record.type + "'");
    }
    auto establishment = it->second(record);
    establishment->set_origin(record.address, record.source);
    return establishment;
}
