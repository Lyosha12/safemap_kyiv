#pragma once

#include <memory>
#include <string>
#include <vector>
#include "EstablishmentFactory.h"
#include "RoutePlanner.h"

struct LoadIssue {
    size_t line;
    std::string message;
};

struct SourceData {
    std::vector<std::pair<size_t, EstablishmentRecord>> records;
    std::vector<LoadIssue> issues;
};

class IDataSource {
public:
    virtual ~IDataSource() = default;
    virtual SourceData load() const = 0;
    virtual std::string describe() const = 0;
};

class CsvDataSource : public IDataSource {
public:
    explicit CsvDataSource(std::string file_path);
    SourceData load() const override;
    std::string describe() const override;

    static const std::vector<std::string>& required_columns();

private:
    std::string path;
};

struct RecordOverride {
    size_t line = 0;
    std::string osm_ref;
    int id = 0;
    bool match_by_id = false;
    std::map<std::string, std::string> fields;
};

class RecordOverrides {
public:
    static RecordOverrides from_csv(const std::string& path, std::vector<LoadIssue>& issues);

    bool apply(EstablishmentRecord& record);
    std::vector<RecordOverride> unused() const;
    size_t size() const { return items.size(); }

private:
    bool matches(const RecordOverride& item, const EstablishmentRecord& record) const;

    std::vector<RecordOverride> items;
    std::vector<bool> used;
};

class OverriddenDataSource : public IDataSource {
public:
    OverriddenDataSource(std::unique_ptr<IDataSource> inner, std::shared_ptr<RecordOverrides> overrides);
    SourceData load() const override;
    std::string describe() const override;

private:
    std::unique_ptr<IDataSource> inner;
    std::shared_ptr<RecordOverrides> overrides;
};

struct LoadSummary {
    size_t loaded = 0;
    std::vector<LoadIssue> issues;
};

class DataLoader {
public:
    DataLoader(std::unique_ptr<IDataSource> source, EstablishmentFactory factory);
    LoadSummary load_into(RoutePlanner& planner) const;

private:
    std::unique_ptr<IDataSource> source;
    EstablishmentFactory factory;
};
