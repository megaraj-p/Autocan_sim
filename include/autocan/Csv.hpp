#ifndef AUTOCAN_CSV_HPP
#define AUTOCAN_CSV_HPP

#include <mutex>
#include <string>
#include <vector>

namespace autocan {

std::vector<std::string> parseCsv(const std::string& line);
std::string escapeCsv(const std::string& value);

class CsvReader {
public:
    explicit CsvReader(const std::string& path);
    bool next(std::vector<std::string>& row);

private:
    std::vector<std::vector<std::string> > rows_;
    std::size_t index_;
};

class CsvWriter {
public:
    CsvWriter(const std::string& path, const std::vector<std::string>& header);
    void append(const std::vector<std::string>& fields);

private:
    std::string path_;
    mutable std::mutex mutex_;
};

} // namespace autocan

#endif
