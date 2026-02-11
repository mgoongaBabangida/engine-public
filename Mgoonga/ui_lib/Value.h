#pragma once

// Value.h
#pragma once
#include <variant>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <type_traits>

namespace UI_lib {

  struct Value {
    struct Map;  // fwd
    struct Vec;  // fwd

    using Object = std::unordered_map<std::string, Value>;
    using Array = std::vector<Value>;

    // Wraps so we can forward-declare in the variant cleanly
    struct Map { Object data; };
    struct Vec { Array  data; };

    using Variant = std::variant<
      std::monostate,    // null / not set
      int64_t,
      double,
      bool,
      std::string,
      Map,
      Vec
    >;

    Variant data;

    // ----- implicit constructors keep old code working -----
    Value() : data(std::monostate{}) {}
    Value(int32_t v) : data(int64_t(v)) {}
    Value(int64_t v) : data(v) {}
    Value(double v) : data(v) {}
    Value(float v) : data(double(v)) {}
    Value(bool v) : data(v) {}
    Value(const char* s) : data(std::string(s)) {}
    Value(std::string s) : data(std::move(s)) {}

    // Object/Array builders
    static Value object() { Value v; v.data = Map{ Object{} }; return v; }
    static Value array() { Value v; v.data = Vec{ Array{} };  return v; }

    static Value object(std::initializer_list<std::pair<std::string, Value>> init) {
      Value v; v.data = Map{ Object{} };
      auto& m = std::get<Map>(v.data).data;
      for (auto& kv : init) m.emplace(kv.first, kv.second);
      return v;
    }
    static Value array(std::initializer_list<Value> init) {
      Value v; v.data = Vec{ Array{} };
      auto& a = std::get<Vec>(v.data).data;
      a.insert(a.end(), init.begin(), init.end());
      return v;
    }

    // ----- typed accessors (return nullptr if wrong type) -----
    const int64_t* asInt()    const { return std::get_if<int64_t>(&data); }
    const double* asDouble() const { return std::get_if<double>(&data); }
    const bool* asBool()   const { return std::get_if<bool>(&data); }
    const std::string* asStr()    const { return std::get_if<std::string>(&data); }
    const Map* asMap()    const { return std::get_if<Map>(&data); }
    const Vec* asVec()    const { return std::get_if<Vec>(&data); }

    int64_t* asInt() { return std::get_if<int64_t>(&data); }
    double* asDouble() { return std::get_if<double>(&data); }
    bool* asBool() { return std::get_if<bool>(&data); }
    std::string* asStr() { return std::get_if<std::string>(&data); }
    Map* asMap() { return std::get_if<Map>(&data); }
    Vec* asVec() { return std::get_if<Vec>(&data); }

    // ----- convenience for map/array mutation -----
    Value& operator[](const std::string& key) {
      if (!std::holds_alternative<Map>(data)) data = Map{ Object{} };
      return std::get<Map>(data).data[key];
    }
    const Value* get(const std::string& key) const {
      if (!std::holds_alternative<Map>(data)) return nullptr;
      const auto& m = std::get<Map>(data).data;
      auto it = m.find(key);
      return it == m.end() ? nullptr : &it->second;
    }
    void push(Value v) {
      if (!std::holds_alternative<Vec>(data)) data = Vec{ Array{} };
      std::get<Vec>(data).data.emplace_back(std::move(v));
    }
  };

  // Keep the public name the same to avoid changing SignalHub signatures.
  using Payload = Value;
}
