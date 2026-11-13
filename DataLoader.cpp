#include "DataLoader.h"

#include <algorithm>
#include <exception>
#include <set>
#include <stdexcept>
#include "rapidcsv.h"

using namespace std;

namespace {

constexpr size_t kHeaderLines = 1;

class RowReader {
public:
    RowReader(const rapidcsv::Document& doc, size_t row) : doc(doc), row(row) {}

    string text(const string& column) const {
        int idx = doc.GetColumnIdx(column);
        if (idx < 0) return {};
        return doc.GetCell<string>(static_cast<size_t>(idx), row);
    }

    template <typename T>
    T number(const string& column) const {
        string raw = text(column);
        if (raw.empty()) throw invalid_argument("порожнє значення в колонці '" + column + "'");
        size_t used = 0;
        T value{};
        try {
            value = convert<T>(raw, used);
        } catch (const logic_error&) {
            used = 0;
        }
        if (used == 0 || used != raw.size()) {
            throw invalid_argument("некоректне число '" + raw + "' в колонці '" + column + "'");
        }
        return value;
    }

    template <typename T>
    T number_or(const string& column, T fallback) const {
        return text(column).empty() ? fallback : number<T>(column);
    }

private:
    template <typename T>
    static T convert(const string& raw, size_t& used);

    const rapidcsv::Document& doc;
    size_t row;
};

template <>
int RowReader::convert<int>(const string& raw, size_t& used) { return stoi(raw, &used); }

template <>
double RowReader::convert<double>(const string& raw, size_t& used) { return stod(raw, &used); }

void validate_record(const EstablishmentRecord& r) {
    if (r.name.empty()) throw invalid_argument("порожня назва");
    if (r.lat < -90.0 || r.lat > 90.0) throw out_of_range("широта поза межами [-90, 90]");
    if (r.lng < -180.0 || r.lng > 180.0) throw out_of_range("довгота поза межами [-180, 180]");
    if (r.shelter_capacity < 0) throw out_of_range("від'ємна місткість укриття");
}

}

CsvDataSource::CsvDataSource(string file_path) : path(move(file_path)) {}

const vector<string>& CsvDataSource::required_columns() {
    static const vector<string> columns = {"id", "type", "name", "lat", "lng"};
    return columns;
}

string CsvDataSource::describe() const {
    return "CSV " + path;
}

SourceData CsvDataSource::load() const {
    rapidcsv::Document doc(path,
                           rapidcsv::LabelParams(0, -1),
                           rapidcsv::SeparatorParams(',', true),
                           rapidcsv::ConverterParams(),
                           rapidcsv::LineReaderParams(true, '#', true));

    for (const auto& column : required_columns()) {
        if (doc.GetColumnIdx(column) < 0) {
            throw runtime_error("у файлі " + path + " немає обов'язкової колонки '" + column + "'");
        }
    }

    static const set<string> known = {"id", "type", "name", "lat", "lng", "shelter_capacity",
                                      "underground", "generator", "address", "source"};
    vector<string> extra_columns;
    for (const auto& column : doc.GetColumnNames()) {
        if (!column.empty() && known.count(column) == 0) extra_columns.push_back(column);
    }

    SourceData data;
    for (size_t row = 0; row < doc.GetRowCount(); ++row) {
        size_t line = row + kHeaderLines + 1;
        try {
            RowReader reader(doc, row);
            EstablishmentRecord record;
            record.id = reader.number<int>("id");
            record.type = reader.text("type");
            record.name = reader.text("name");
            record.lat = reader.number<double>("lat");
            record.lng = reader.number<double>("lng");
            record.shelter_capacity = reader.number_or<int>("shelter_capacity", 0);
            record.underground = parse_flag(reader.text("underground"));
            record.generator = parse_flag(reader.text("generator"));
            record.address = reader.text("address");
            record.source = reader.text("source");
            for (const auto& column : extra_columns) {
                string value = reader.text(column);
                if (!value.empty()) record.extras[column] = value;
            }
            validate_record(record);
            data.records.emplace_back(line, move(record));
        } catch (const exception& e) {
            data.issues.push_back({line, e.what()});
        }
    }
    return data;
}

DataLoader::DataLoader(unique_ptr<IDataSource> src, EstablishmentFactory f)
    : source(move(src)), factory(move(f)) {}

LoadSummary DataLoader::load_into(RoutePlanner& planner) const {
    SourceData data = source->load();
    LoadSummary summary;
    summary.issues = move(data.issues);

    set<int> seen_ids;
    for (const auto& [line, record] : data.records) {
        if (!seen_ids.insert(record.id).second) {
            summary.issues.push_back({line, "повторний id " + to_string(record.id)});
            continue;
        }
        try {
            planner.add_establishment(factory.create(record));
            ++summary.loaded;
        } catch (const exception& e) {
            summary.issues.push_back({line, e.what()});
        }
    }
    sort(summary.issues.begin(), summary.issues.end(),
         [](const LoadIssue& a, const LoadIssue& b) { return a.line < b.line; });
    return summary;
}
