//  This file is distributed under the BSD 3-Clause License. See LICENSE for details.
#pragma once

#include "node.hpp"
#include "node_pin.hpp"

class XEdge { // FIXME: s/XEdge/Edge/g
protected:
  friend LGraph;
  friend Node;
  friend Node_pin;
public:
  struct __attribute__((packed)) Compact {
    const uint64_t driver_idx : Index_bits;
    const uint16_t pad1 : 1; // Just to improve alignment of
    const uint64_t sink_idx   : Index_bits;
    const uint16_t pad2 : 1; // Just to improve alignment of
    const uint16_t driver_pid : Port_bits;
    const uint16_t pad3 : 2; // Just to improve alignment of
    const uint16_t sink_pid   : Port_bits;
    const uint16_t pad4 : 2; // Just to improve alignment of

    Compact(const Index_ID &d_idx, const Port_ID &d_pid, const Index_ID &s_idx, const Port_ID &s_pid)
      :driver_idx(d_idx)
      ,pad1(0)
      ,sink_idx(s_idx)
      ,pad2(0)
      ,driver_pid(d_pid)
      ,pad3(0)
      ,sink_pid(s_pid)
      ,pad4(0) {
    };

    bool operator==(const Compact &other) const {
      return (driver_idx == other.driver_idx)
          && (driver_pid == other.driver_pid)
          && (sink_idx   == other.sink_idx  )
          && (sink_pid   == other.sink_pid  );
    }

    template <typename H>
    friend H AbslHashValue(H h, const Compact& s) {
      return H::combine(std::move(h), s.driver_idx, s.sink_idx, s.driver_pid, s.sink_pid);
    };

  };
  template <typename H>
  friend H AbslHashValue(H h, const XEdge& s) {
    return H::combine(std::move(h), s.driver, s.sink);
  };
  Node_pin driver;
  Node_pin sink;

  XEdge(const Node_pin &src_, const Node_pin &dst_);

  bool operator==(const XEdge &other) const { return (driver == other.driver) && (sink == other.sink); }

  bool operator!=(const XEdge &other) const { return (driver != other.driver) || (sink != other.sink); }

  inline Compact get_compact() const {
    return Compact(driver.get_idx(),driver.get_pid(),sink.get_idx(),sink.get_pid());
  }

  void del_edge();

  // BEGIN ATTRIBUTE ACCESSORS

  uint16_t get_bits() const { return driver.get_bits(); }

  // END ATTRIBUTE ACCESSORS
};


