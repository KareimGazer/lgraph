//  This file is distributed under the BSD 3-Clause License. See LICENSE for details.

#include "lgedgeiter.hpp"
#include "lgraph.hpp"

bool test0() {
  LGraph *g = LGraph::create("lgdb_core_test", "test0", "test");

  auto n1 = g->create_node(SubGraph_Op);
  auto n2 = g->create_node(SubGraph_Op);

  auto driver_pin20  = n1.setup_driver_pin(20);
  auto sink_pin120 = n2.setup_sink_pin(120);

  for(auto &out : n1.out_edges()) {
    I(false);
    (void)out; // just to silence the warning
  }
  for(auto &out : n2.out_edges()) {
    I(false);
    (void)out; // just to silence the warning
  }

  g->add_edge(driver_pin20,sink_pin120);
  int conta=0;
  for(auto &out : n1.out_edges()) {
    conta++;
    (void)out;
  }
  I(conta==1);
  for(auto &out : n2.out_edges()) {
    I(false);
    (void)out; // just to silence the warning
  }

  for(int i=30;i<330;i++) {
    auto s = n1.setup_driver_pin(i);
    g->add_edge(s,sink_pin120);
  }
  conta = 0;
  for(auto &out : n1.out_edges()) {
    conta++;
    (void)out;
  }
  I(conta == (1+300));

  for(int i=200;i<1200;i++) {
    auto s = n1.setup_driver_pin((i&31) + 20);
    auto p = n2.setup_sink_pin(i);
    g->add_edge(s,p);
  }
  conta = 0;
  for(auto &out : n1.out_edges()) {
    conta++;
    (void)out;
  }
  I(conta == (1+300+1000));
  conta = 0;
  for(auto &out : n2.inp_edges()) {
    conta++;
    (void)out;
  }
  I(conta == (1+300+1000));

  g->close();

  return true;
}

bool test1() {
  LGraph *g = LGraph::create("lgdb_core_test", "test", "test");

  auto n1 = g->create_node(SubGraph_Op);
  auto n2 = g->create_node(SubGraph_Op);

  auto dpin = n1.setup_driver_pin(20);
  auto spin = n2.setup_sink_pin(25);

  g->add_edge(dpin, spin);

  for(auto &out : n1.out_edges()) {
    assert(out.sink == spin);
    assert(out.driver == dpin);

    assert(out.sink.get_pid() == 25);
    assert(out.driver.get_pid() == 20);

    assert(out.driver.is_input() == false);
    assert(out.sink.is_input() == true);
  }

  for(auto &inp : n2.inp_edges()) {
    assert(inp.sink == spin);
    assert(inp.driver == dpin);

    assert(inp.sink.get_pid() == 25);
    assert(inp.driver.get_pid() == 20);

    assert(inp.driver.is_input() == false);
    assert(inp.sink.is_input() == true);
  }

  g->close();

  return true;
}

bool test20() {
  LGraph *g = LGraph::create("lgdb_core_test", "test20", "test");

  auto n1 = g->create_node(SubGraph_Op);
  auto n2 = g->create_node(SubGraph_Op);

  auto dpin = n1.setup_driver_pin(0);
  auto spin = n2.setup_sink_pin(3);

  g->add_edge(dpin, spin, 33);

  for(auto &inp : n2.inp_edges()) {
    inp.del_edge();
  }

  for(auto &inp : n2.inp_edges()) {
    assert(false);
    (void)inp; // just to silence the warning
  }

  for(auto &out : n1.out_edges()) {
    assert(false);
    (void)out; // just to silence the warning
  }

  g->close();

  return true;
}

bool test21() {

  LGraph *g = LGraph::create("lgdb_core_test", "test21", "test");

  auto n1 = g->create_node(SubGraph_Op);
  auto n2 = g->create_node(SubGraph_Op);

  auto dpin = n1.setup_driver_pin(0);
  auto spin = n2.setup_sink_pin(0);

  g->add_edge(dpin, spin, 33);

  for(auto &inp : n2.inp_edges()) {
    assert(inp.get_bits() == 33);
    assert(inp.driver.get_bits() == 33);
  }

  for(auto &out : n1.out_edges()) {
    assert(out.get_bits() == 33);
    assert(out.driver.get_bits() == 33);
    out.del_edge();
  }

  for(auto &inp : n2.inp_edges()) {
    assert(false);
    (void)inp; // just to silence the warning
  }

  for(auto &out : n1.out_edges()) {
    assert(false);
    (void)out; // just to silence the warning
  }

  g->close();

  return true;
}

bool test2() {

  LGraph *g = LGraph::create("lgdb_core_test", "test2", "test");

  auto n1 = g->create_node(SubGraph_Op);
  auto n2 = g->create_node(SubGraph_Op);

  auto dpin = n1.setup_driver_pin(20);
  auto spin = n2.setup_sink_pin(30);

  g->add_edge(dpin, spin, 33);

  for(auto &inp : n2.inp_edges()) {
    inp.del_edge();
  }

  for(auto &inp : n2.inp_edges()) {
    assert(false);
    (void)inp; // just to silence the warning
  }

  for(auto &out : n1.out_edges()) {
    assert(false);
    (void)out; // just to silence the warning
  }

  g->close();

  return true;
}

bool test22() {

  LGraph *g = LGraph::create("lgdb_core_test", "test22", "test");

  auto n1 = g->create_node(SubGraph_Op);
  auto n2 = g->create_node(SubGraph_Op);

  auto dpin = n1.setup_driver_pin(20);
  auto spin = n2.setup_sink_pin(0);

  g->add_edge(dpin, spin, 33);

  for(auto &out : n1.out_edges()) {
    out.del_edge();
  }

  for(auto &inp : n2.inp_edges()) {
    assert(false);
    (void)inp; // just to silence the warning
  }

  for(auto &out : n1.out_edges()) {
    assert(false);
    (void)out; // just to silence the warning
  }

  g->close();

  return true;
}

bool test3() {

  LGraph *g = LGraph::create("lgdb_core_test", "test3", "test");

  auto n1 = g->create_node(SubGraph_Op);
  auto n2 = g->create_node(SubGraph_Op);

  g->add_edge(n1.setup_driver_pin(20), n2.setup_sink_pin(25));

  for(auto &inp : n2.inp_edges()) {
    inp.del_edge();
  }

  for(auto &inp : n2.inp_edges()) {
    assert(false);
    (void)inp; // just to silence the warning
  }

  for(auto &out : n1.out_edges()) {
    assert(false);
    (void)out; // just to silence the warning
  }

  n2.del_node();

  for(auto nid : g->fast()) {
    assert(nid != n2.get_compact().nid);
  }

  g->close();

  return true;
}

int main() {
  test0();
  test1();
  test20();
  test21();
  test2();
  test22();
  test3();

  return 0;
}
