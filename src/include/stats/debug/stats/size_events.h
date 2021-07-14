#pragma once

#include <atomic>

struct Event {
  std::atomic<size_t> n_events{0};
  
  void add() {
    n_events.fetch_add(1, std::memory_order_relaxed);
  }
};

struct EventTime : public Event {
  std::atomic<size_t> total_time{0};
  
  void add(size_t time) {
    Event::add();
    total_time.fetch_add(time, std::memory_order_relaxed);
  }
};

struct SizeEventData : public EventTime {
  std::atomic<size_t> n_bytes{0};

  void add(size_t bytes, size_t time) {
    n_bytes.fetch_add(bytes, std::memory_order_relaxed);
    EventTime::add(time);
  }
};
