#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
// --- THE MAPREDUCE FRAMEWORK (Simulated) ---
// (Repeated here so this file is standalone)
template <typename K, typename V>
struct KeyValue {
    K key;
    V value;
};
template <typename K1, typename V1, typename K2, typename V2, typename K3, typename V3>
class MapReduce {
public:
    virtual void Map(const K1& key, const V1& val, std::vector<KeyValue<K2, V2>>& context) = 0;
    virtual void Reduce(const K2& key, const std::vector<V2>& values, std::vector<KeyValue<K3, V3>>& context) = 0;
    std::vector<KeyValue<K3, V3>> Run(const std::vector<KeyValue<K1, V1>>& input) {
        std::vector<KeyValue<K2, V2>> intermediate;
        std::vector<KeyValue<K3, V3>> output;
        for (const auto& item : input) Map(item.key, item.value, intermediate);
        std::sort(intermediate.begin(), intermediate.end(), 
            [](const KeyValue<K2, V2>& a, const KeyValue<K2, V2>& b) { return a.key < b.key; });

        if (intermediate.empty()) return output;
        std::vector<V2> values_group;
        K2 current_key = intermediate[0].key;
        for (const auto& item : intermediate) {
            if (item.key != current_key) {
                Reduce(current_key, values_group, output);
                values_group.clear();
                current_key = item.key;
            }
            values_group.push_back(item.value);
        }
        Reduce(current_key, values_group, output);
        return output;
    }
};
// --- USER IMPLEMENTATION: LONGEST PATH ---
// Input: <ID, PathString>
// Map Output: <"MAX", PathString> (Send all paths to one reducer key)
// Reduce Output: <"Longest", PathString>
class LongestPathApp : public MapReduce<int, std::string, std::string, std::string, std::string, std::string> {
public:
    void Map(const int& key, const std::string& val, std::vector<KeyValue<std::string, std::string>>& context) override {
        // We emit every path with the same key "MAX" so they all go to the same Reducer call.
        context.push_back({"MAX", val});
    }
    void Reduce(const std::string& key, const std::vector<std::string>& values, std::vector<KeyValue<std::string, std::string>>& context) override {
        std::string max_path = "";
        
        // Find the longest string in the list
        for (const auto& path : values) {
            if (path.length() > max_path.length()) {
                max_path = path;
            }
        }
        context.push_back({"Longest", max_path});
    }
};
int main() {
    LongestPathApp app;
    // Simulated Input: List of file paths
    std::vector<KeyValue<int, std::string>> input = {
        {1, "/usr/bin"},
        {2, "/usr/local/bin/python3"},
        {3, "/var/log/apache2/error.log"},
        {4, "/home/user/very/long/path/name/that/should/win/the/contest.txt"}
    };
    auto results = app.Run(input);
    std::cout << "--- Longest Path Result ---" << std::endl;
    for (const auto& res : results) {
        std::cout << "Found: " << res.value << std::endl;
        std::cout << "Length: " << res.value.length() << std::endl;
    }
    return 0;
}