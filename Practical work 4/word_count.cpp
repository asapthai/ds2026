#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
// --- THE MAPREDUCE FRAMEWORK (Simulated) ---
// Key-Value Pair Structure
template <typename K, typename V>
struct KeyValue {
    K key;
    V value;
};
// Abstract Base Class
template <typename K1, typename V1, typename K2, typename V2, typename K3, typename V3>
class MapReduce {
public:
    virtual void Map(const K1& key, const V1& val, std::vector<KeyValue<K2, V2>>& context) = 0;
    virtual void Reduce(const K2& key, const std::vector<V2>& values, std::vector<KeyValue<K3, V3>>& context) = 0;
    // The Engine: Map -> Shuffle -> Reduce
    std::vector<KeyValue<K3, V3>> Run(const std::vector<KeyValue<K1, V1>>& input) {
        std::vector<KeyValue<K2, V2>> intermediate;
        std::vector<KeyValue<K3, V3>> output;
        // 1. MAP PHASE
        for (const auto& item : input) {
            Map(item.key, item.value, intermediate);
        }
        // 2. SHUFFLE PHASE (Sort)
        std::sort(intermediate.begin(), intermediate.end(), 
            [](const KeyValue<K2, V2>& a, const KeyValue<K2, V2>& b) {
                return a.key < b.key;
            });

        // 3. REDUCE PHASE (Group & Reduce)
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
// --- USER IMPLEMENTATION: WORD COUNT ---
// Types: <LineID, LineString> -> <Word, Count> -> <Word, TotalCount>
class WordCountApp : public MapReduce<int, std::string, std::string, int, std::string, int> {
public:
    void Map(const int& key, const std::string& val, std::vector<KeyValue<std::string, int>>& context) override {
        std::stringstream ss(val);
        std::string word;
        while (ss >> word) {
            context.push_back({word, 1});
        }
    }
    void Reduce(const std::string& key, const std::vector<int>& values, std::vector<KeyValue<std::string, int>>& context) override {
        int sum = 0;
        for (int v : values) sum += v;
        context.push_back({key, sum});
    }
};
int main() {
    WordCountApp app;
    // Simulated Input Data
    std::vector<KeyValue<int, std::string>> input = {
        {1, "apple banana apple"},
        {2, "banana orange banana"},
        {3, "apple orange grape"}
    };
    auto results = app.Run(input);
    std::cout << "--- Word Count ---" << std::endl;
    for (const auto& res : results) {
        std::cout << res.key << ": " << res.value << std::endl;
    }
    return 0;
}