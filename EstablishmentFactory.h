#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "Establishments.h"

struct EstablishmentRecord {
    int id = 0;
    std::string type;
    std::string name;
    double lat = 0.0;
    double lng = 0.0;
    int shelter_capacity = 0;
    bool underground = false;
    bool generator = false;
    std::string address;
    std::string source;
    std::map<std::string, std::string> extras;

    bool extra_flag(const std::string& key, bool fallback = false) const;
    double extra_number(const std::string& key, double fallback = 0.0) const;
};

bool parse_flag(const std::string& raw, bool fallback = false);
bool is_flag(const std::string& raw);

class EstablishmentFactory {
public:
    using Creator = std::function<std::shared_ptr<Establishment>(const EstablishmentRecord&)>;

    static EstablishmentFactory with_default_types();

    void register_type(const std::string& key, Creator creator);
    bool supports(const std::string& key) const;
    std::vector<std::string> supported_types() const;
    std::shared_ptr<Establishment> create(const EstablishmentRecord& record) const;

private:
    std::map<std::string, Creator> creators;
};
