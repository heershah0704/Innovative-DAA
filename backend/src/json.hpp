// json.hpp
// Tiny JSON writer (no library needed). Commas are inserted automatically.
//   JsonWriter j; j.beginObject().key("a").value(1).endObject();  ->  {"a":1}
#pragma once
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

class JsonWriter {
public:
    JsonWriter& beginObject() { comma(); out_ += '{'; first_.push_back(true); return *this; }
    JsonWriter& endObject() { out_ += '}'; first_.pop_back(); return *this; }
    JsonWriter& beginArray() { comma(); out_ += '['; first_.push_back(true); return *this; }
    JsonWriter& endArray() { out_ += ']'; first_.pop_back(); return *this; }

    JsonWriter& key(const std::string& k) { comma(); out_ += quote(k) + ':'; afterKey_ = true; return *this; }

    JsonWriter& value(const std::string& s) { comma(); out_ += quote(s); return *this; }
    JsonWriter& value(const char* s) { return value(std::string(s)); }
    JsonWriter& value(bool b) { comma(); out_ += b ? "true" : "false"; return *this; }
    JsonWriter& value(int n) { comma(); out_ += std::to_string(n); return *this; }
    JsonWriter& value(long long n) { comma(); out_ += std::to_string(n); return *this; }
    JsonWriter& value(unsigned n) { comma(); out_ += std::to_string(n); return *this; }
    JsonWriter& value(double x) {
        comma();
        if (!std::isfinite(x)) { out_ += "null"; return *this; }
        char b[40];
        std::snprintf(b, sizeof b, "%.6g", x);
        out_ += b;
        return *this;
    }
    JsonWriter& null() { comma(); out_ += "null"; return *this; }

    const std::string& str() const { return out_; }

    static std::string quote(const std::string& s) {
        std::string r = "\"";
        for (char c : s) {
            switch (c) {
                case '"': r += "\\\""; break;
                case '\\': r += "\\\\"; break;
                case '\n': r += "\\n"; break;
                case '\r': r += "\\r"; break;
                case '\t': r += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); r += b; }
                    else r += c;
            }
        }
        return r + "\"";
    }

private:
    void comma() {
        if (afterKey_) { afterKey_ = false; return; }
        if (!first_.empty()) {
            if (!first_.back()) out_ += ',';
            first_.back() = false;
        }
    }
    std::string out_;
    std::vector<bool> first_;
    bool afterKey_ = false;
};
