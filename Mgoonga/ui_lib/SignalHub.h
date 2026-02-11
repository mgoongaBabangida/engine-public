// SignalHub.h
#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include <variant>
#include <mutex>

#include "Value.h"

namespace UI_lib {

  //using Payload = std::variant<int64_t, double, bool, std::string>;

  struct SignalHub {
    using Slot = std::function<void(const Payload&)>;

    // subscription table (UI-thread only)
    std::unordered_map<std::string, std::vector<Slot>> subs;

    // lock-protected queue of pending events (multi-producer)
    struct Event { std::string name; Payload payload; };

    std::mutex qmtx;
    std::vector<Event> queue;

    // Subscribe: call on UI thread during init/wiring
    size_t subscribe(const std::string& name, Slot s) {
      auto& v = subs[name];
      v.emplace_back(std::move(s));
      return v.size() - 1;
    }

    // Post from ANY thread (GL/game ok): enqueues only
    void post(const std::string& name, Payload p) {
      std::lock_guard<std::mutex> lk(qmtx);
      queue.push_back(Event{ name, std::move(p) });
    }

    // Pump on UI thread: drain queue and deliver to subscribers
    void pump() {
      // move pending events out under lock
      std::vector<Event> local;
      {
        std::lock_guard<std::mutex> lk(qmtx);
        local.swap(queue);
      }
      // deliver (UI thread)
      for (auto& e : local) {
        auto it = subs.find(e.name);
        if (it == subs.end()) continue;
        for (auto& s : it->second) s(e.payload);
      }
    }
  };

  inline Payload Obj(std::initializer_list<std::pair<std::string, Payload>> init) {
    return Payload::object(init);
  }
  inline Payload Arr(std::initializer_list<Payload> init) {
    return Payload::array(init);
  }

  // Terse getters
  inline std::string PayloadString(const Payload& v, const std::string& def = "") {
    if (auto s = v.asStr()) return *s;
    if (auto i = v.asInt()) return std::to_string(*i);
    if (auto d = v.asDouble()) return std::to_string(*d);
    if (auto b = v.asBool()) return *b ? "true" : "false";
    return def;
  }
  inline int64_t PayloadInt(const Payload& v, int64_t def = 0) {
    if (auto i = v.asInt()) return *i;
    if (auto d = v.asDouble()) return (int64_t)*d;
    return def;
  }

} // namespace UI_lib
