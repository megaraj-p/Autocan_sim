#include "autocan/Csv.hpp"

#include <fstream>
#include <stdexcept>

namespace autocan {

std::vector<std::string> parseCsv(const std::string& line) {
    std::vector<std::string> output;
    std::string currentField;
    bool quoted = false;

    for (std::size_t index = 0; index < line.size(); ++index) {
        const char character = line[index];

        if (character == '"') {
            if (quoted && index + 1 < line.size() && line[index + 1] == '"') {
                currentField += '"';
                ++index;
            } else {
                quoted = !quoted;
            }
        } else if (character == ',' && !quoted) {
            output.push_back(currentField);
            currentField.clear();
        } else {
            currentField += character;
        }
    }

    if (quoted) {
        throw std::runtime_error("Malformed CSV quote");
    }

    output.push_back(currentField);
    return output;
}

std::string escapeCsv(const std::string& value) {
    if (value.find_first_of(",\"\n\r") == std::string::npos) {
        return value;
    }

    std::string output = "\"";
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '"') {
            output += "\"\"";
        } else {
            output += value[index];
        }
    }
    output += '"';
    return output;
}

CsvReader::CsvReader(const std::string& path)
    : index_(0) {
    std::ifstream input(path.c_str());
    if (!input) {
        throw std::runtime_error("Cannot open CSV: " + path);
    }

    std::string line;
    if (!std::getline(input, line)) {
        throw std::runtime_error("Empty CSV: " + path);
    }

    while (std::getline(input, line)) {
        if (!line.empty()) {
            rows_.push_back(parseCsv(line));
        }
    }

    if (rows_.empty()) {
        throw std::runtime_error("No CSV data: " + path);
    }
}

bool CsvReader::next(std::vector<std::string>& row) {
    if (rows_.empty()) {
        return false;
    }

    row = rows_[index_];
    index_ = (index_ + 1) % rows_.size();
    return true;
}

CsvWriter::CsvWriter(
    const std::string& path,
    const std::vector<std::string>& header)
    : path_(path) {
    std::ifstream existing(path.c_str());
    if (existing.good()) {
        return;
    }

    std::ofstream output(path.c_str());
    if (!output) {
        throw std::runtime_error("Cannot create log: " + path);
    }

    for (std::size_t index = 0; index < header.size(); ++index) {
        if (index > 0) {
            output << ',';
        }
        output << escapeCsv(header[index]);
    }
    output << '\n';
}

void CsvWriter::append(const std::vector<std::string>& fields) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream output(path_.c_str(), std::ios::app);

    if (!output) {
        throw std::runtime_error("Cannot append log: " + path_);
    }

    for (std::size_t index = 0; index < fields.size(); ++index) {
        if (index > 0) {
            output << ',';
        }
        output << escapeCsv(fields[index]);
    }
    output << '\n';
}

} // namespace autocan
