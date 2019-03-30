//  This file is distributed under the BSD 3-Clause License. See LICENSE for details.

#include "lgraph.hpp"

#include "edge.hpp"

static_assert(sizeof(XEdge::Compact) == 12);
static_assert(sizeof(Node_pin::Compact) == 4);
static_assert(sizeof(Node::Compact) == 4);

XEdge::XEdge(const Node_pin &src_, const Node_pin &dst_)
  : driver(src_)
  , sink(dst_) {

  I(sink.is_sink());
  I(driver.is_driver());

  I(driver.get_hid()    == sink.get_hid());
  I(driver.get_lgraph() == sink.get_lgraph());
}

void XEdge::del_edge() {

  bool deleted = driver.get_lgraph()->del_edge(driver,sink);
  I(deleted);

}


