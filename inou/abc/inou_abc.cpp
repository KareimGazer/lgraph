//
// Created by birdeclipse on 1/30/18.
//
#include "inou/lef/inou_lef.hpp"
#include "inou_abc.hpp"
#include <boost/filesystem.hpp>

Inou_abc_options_pack::Inou_abc_options_pack() {
	Options::get_desc()->add_options()
					("verbose,v", boost::program_options::value(&verbose), "verbose debug info : <true>/<false>");

	Options::get_desc()->add_options()
					("lef_file,lef", boost::program_options::value(&lef_file),
					 "lef_file <filename> for graph");

	Options::get_desc()->add_options()
					("liberty_file,lib", boost::program_options::value(&liberty_file),
					 "liberty_file <filename> for graph");

	boost::program_options::variables_map vm;
	boost::program_options::store(
					boost::program_options::command_line_parser(Options::get_cargc(), Options::get_cargv()).options(
									*Options::get_desc()).allow_unregistered().run(), vm);
	if (vm.count("verbose")) {
		verbose = vm["verbose"].as<std::string>();
	}
	else {
		verbose = "false";
	}

	if (vm.count("lef_file")) {
		lef_file = vm["lef_file"].as<std::string>();
	}
	else {
		lef_file = "";
	}

	if (vm.count("liberty_file")) {
		liberty_file = vm["liberty_file"].as<std::string>();
	}
	else {
		liberty_file = "";
	}

	if (lef_file.empty())
		assert(liberty_file.empty()); //ensure lef are loaded into tech library first



	console->info("verbose: {}; lef_file {}", verbose, lef_file);
}

Inou_abc::Inou_abc() {

}

Inou_abc::~Inou_abc() {

}

std::vector<LGraph *> Inou_abc::generate() {
	std::vector<LGraph *> lgs;
	if (opack.graph_name != "") {
		lgs.push_back(new LGraph(opack.lgdb_path, opack.graph_name, false)); // Do not clear
		if (opack.lef_file != "") {
			Inou_lef::lef_parsing(lgs[0]->get_tech_library(), opack.lef_file);
			lgs[0]->sync(); // sync because Tech Library is loaded
		}
		// No need to sync because it is a reload. Already sync
	}
	else {
		console->error("Please specify the graph name!\n");
		exit(-4);
	}
	return lgs;
}

void Inou_abc::generate(std::vector<const LGraph *> out) {
	if (out.size() == 1) {
		if (is_techmap(out[0])) {
			find_cell_conn(out[0]);
			LGraph *Mapped_Lgraph = new LGraph(opack.lgdb_path, opack.graph_name + "_mapped", true);
			from_abc(Mapped_Lgraph, out[0], to_abc(out[0]));
			Mapped_Lgraph->sync();
			Mapped_Lgraph->print_stats();
		}
	}
}

/************************************************************************
 * Function:  Inou_abc::find_cell_conn()
 * --------------------
 * input arg0 -> const LGraph *g
 *
 * returns: nothing
 *
 * description: compute the netlist topology use recursive_find() ,
 * 							the info is stored in
 * 							node_conn comb_conn;
 * 							node_conn latch_conn;
 * 							node_conn PO_conn;
 * 							block_conn subgraph_conn;
 ***********************************************************************/

void Inou_abc::find_latch_conn(const LGraph *g) {
	for (const auto &idx : latch_id) {
		topology_info pid;
		const Tech_cell *tcell = g->get_tlibrary()->get_const_cell(g->tmap_id_get(idx));
		if (opack.verbose == "true")
			fmt::print("\nLatch_Op_ID NodeID:{} has direct input from Node: \n", idx);
		for (const auto &input : g->inp_edges(idx)) {
			if (input.get_inp_pin().get_pid() == tcell->get_pin_id("D")) {
				if (opack.verbose == "true")
					fmt::print("{} pin input port ID {}", tcell->get_name(input.get_inp_pin().get_pid()),
					           input.get_inp_pin().get_pid());
				int bit_index[2] = {0, 0};
				recursive_find(g, &input, pid, bit_index);
			}
				/*********************************************************************
				 * The following code is to keep the clock nets and reset nets that
				 * fanin to the flops
				 * ABC treats all sequential cells as Latches
				 * ABC cannot handle these signals
				 * Although industrial standard flops usually have more pins such as
				 * scan_en, si, etc..,
				 * Due to the limitation of ABC synthesis,
				 * this build only support "clock" pin and "reset" pin,
				 *********************************************************************/
			else if (input.get_inp_pin().get_pid() == tcell->get_pin_id("C")) {
				topology_info clock_pid;
				int bit_index[2] = {0, 0};
				recursive_find(g, &input, clock_pid, bit_index);
				Index_ID clk_idx = clock_pid[0].idx;

				char clk_name[100];
				if (g->is_graph_input(clk_idx)) {
					sprintf(clk_name, "%s", g->get_node_wirename(clk_idx));
				}
				else {
					sprintf(clk_name, "generated_clock_id_%ld", clk_idx);
				}
				std::string clock_name(clk_name);
				clock_id[clock_name] = clk_idx;
				skew_group_map[clock_name].insert(idx);
			}
			else if (input.get_inp_pin().get_pid() == tcell->get_pin_id("R")) {
				topology_info reset_pid;
				int bit_index[2] = {0, 0};
				recursive_find(g, &input, reset_pid, bit_index);
				Index_ID reset_idx = reset_pid[0].idx;
				char rst_name[100];
				if (g->is_graph_input(reset_idx)) {
					sprintf(rst_name, "%s", g->get_node_wirename(reset_idx));
				}
				else {
					sprintf(rst_name, "generated_reset_id_%ld", reset_idx);
				}
				std::string reset_name(rst_name);
				reset_id[reset_name] = reset_idx;
				reset_group_map[reset_name].insert(idx);
			}
		}
		assert(pid.size() == 1);// ensure that D pin have fanin and only store D pin info
		latch_conn[idx] = std::move(pid);
	}
}

void Inou_abc::find_combinational_conn(const LGraph *g) {

	for (const auto &idx : combinational_id) {
		if (opack.verbose == "true")
			fmt::print("\nComb_Op_ID NodeID:{} has direct input from Node: \n", idx);
		std::vector<const Edge *> inp_edges(16);
		topology_info pid;
		const Tech_cell *tcell = g->get_tlibrary()->get_const_cell(g->tmap_id_get(idx));
		int port_count = 0;
		for (const auto &input : g->inp_edges(idx)) {
			assert(input.get_inp_pin().get_pid() < 16);
			inp_edges[input.get_inp_pin().get_pid()] = &input;
			port_count++;
		}
		for (int i = 0; i < port_count; i++) {
			if (inp_edges[i] == nullptr)
				break;
			if (opack.verbose == "true")
				fmt::print("{} pin input port ID {}", tcell->get_name(inp_edges[i]->get_inp_pin().get_pid()),
				           inp_edges[i]->get_inp_pin().get_pid());
			int bit_index[2] = {0, 0};
			recursive_find(g, inp_edges[i], pid, bit_index);
		}
		assert(pid.size() == port_count); // ensure that every port have fanin
		comb_conn[idx] = std::move(pid);
	}
}

void Inou_abc::find_graphio_output_conn(const LGraph *g) {
	for (const auto &idx : graphio_output_id) {
		if (opack.verbose == "true")
			fmt::print("\nGraphIO_output_ID NodeID:{} has direct input from Node: \n", idx);
		topology_info pid;
		int width = g->get_bits(idx);
		int index = 0;
		for(const auto &input : g->inp_edges(idx)) {
			for (index = 0; index < width; index++) {
				int bit_index[2] = {index, index};
				recursive_find(g, &input, pid, bit_index);
			}
		}
		assert(index == pid.size());
		primary_output_conn[idx] = std::move(pid);
	}
}

void Inou_abc::find_subgraph_conn(const LGraph *g) {

	for(const auto &idx : subgraph_id) {
		if (opack.verbose == "true")
			fmt::print("\nSubGraph_Op NodeID:{} has direct input from Node: \n", idx);
		std::unordered_map<Port_ID ,const Edge *> inp_edges;
		std::unordered_map<Port_ID ,topology_info> subgraph_pid;

		for(const auto &input : g->inp_edges(idx)) {
			Port_ID inp_id = input.get_inp_pin().get_pid();
			inp_edges[inp_id] = &input;
		}

		for(const auto &input : inp_edges) {
			if (opack.verbose == "true")
				fmt::print("\n------------------------------------------------ \n", idx);
			topology_info pid;
			auto node_idx = input.second->get_idx();
			auto width = g->get_bits(node_idx);
			int index = 0;
			if(width > 1) {
				for(index = 0; index < width; index++) {
					int bit_index[2] = {index, index};
					recursive_find(g,input.second, pid, bit_index);
				}
			}
			else {
				int bit_index[2] = {0, 0};
				recursive_find(g,input.second, pid, bit_index);
			}
			subgraph_pid[input.first] = std::move(pid);
		}
		subgraph_conn[idx] = std::move(subgraph_pid);
	}
}

void Inou_abc::find_cell_conn(const LGraph *g) {

	fmt::print("\n******************************************************************\n");
	fmt::print("Begin Computing Netlist Topology Based On Lgraph\n");
	fmt::print("******************************************************************\n");
	find_latch_conn(g);
	find_combinational_conn(g);
	find_graphio_output_conn(g);
	find_subgraph_conn(g);
	fmt::print("\n******************************************************************\n");
	fmt::print("Finish Computing Netlist Topology Based On Lgraph\n");
	fmt::print("******************************************************************\n");

}

/************************************************************************
 * Function:  Inou_abc::recursive_find()
 * --------------------
 * input arg0 -> const LGraph *g
 * input arg1 -> const Edge *input
 * input arg2 -> topology_info &pid
 * input arg3 -> int *bit_addr
 *
 * returns: nothing
 *
 * description: recursively traverse input nodes untill finds its src
 * 						an src is U32Const_Op StrConst_Op GraphIO_Op TechMap_Op
 * 						FIXME: More Join & Pick Node means (a way to reduce ?)
 * 						"deeper" traverse depth and recursion
 ***********************************************************************/
void Inou_abc::recursive_find(const LGraph *g, const Edge *input, topology_info &pid, int *bit_addr) {

	Index_ID this_idx = input->get_idx();
	Node_Type_Op this_node_type = g->node_type_get(this_idx).op;
	if (this_node_type == U32Const_Op) {
		if (opack.verbose == "true")
			fmt::print("\t U32Const_Op_NodeID:{},bit [{}:{}] portid : {} \n",
			           input->get_idx(), bit_addr[0], bit_addr[1], input->get_out_pin().get_pid());

		index_offset info = {this_idx, input->get_out_pin().get_pid(),{bit_addr[0], bit_addr[1]}};
		pid.push_back(info);
	}
	else if (this_node_type == StrConst_Op) {
		if (opack.verbose == "true")
			fmt::print("\t StrConst_Op_NodeID:{},bit [{}:{}]  portid : {} \n",
			           this_idx, bit_addr[0], bit_addr[1], input->get_out_pin().get_pid());

		index_offset info = {this_idx, input->get_out_pin().get_pid(), {bit_addr[0], bit_addr[1]}};
		pid.push_back(info);
	}
	else if (this_node_type == GraphIO_Op) {
		if (g->is_graph_output(this_idx)) {
			for (const auto &pre_inp : g->inp_edges(this_idx)) {
				recursive_find(g, &pre_inp, pid, bit_addr);
			}
		}
		else {
			if (opack.verbose == "true")
				fmt::print("\t GraphIO_Op_NodeID:{},bit [{}:{}] portid : {} \n",
				           this_idx, bit_addr[0], bit_addr[1], input->get_out_pin().get_pid());
			index_offset info = {this_idx, input->get_out_pin().get_pid(), {bit_addr[0], bit_addr[1]}};
			pid.push_back(info);
		}
	}
	else if (this_node_type == SubGraph_Op) {
		char namebuffer[255];
		if (opack.verbose == "true")
			fmt::print("\t SubGraph_Op:{},bit [{}:{}] portid : {} \n",
			           this_idx, bit_addr[0], bit_addr[1], input->get_out_pin().get_pid());
		index_offset info = {this_idx, input->get_out_pin().get_pid(), {bit_addr[0], bit_addr[1]}};
		pid.push_back(info);

		sprintf(namebuffer, "%%subgraph_output_%ld_%d_%d%%",this_idx,input->get_out_pin().get_pid(),bit_addr[0]);
		const auto it = subgraph_generated_output_wire.find(info);
		if(it == subgraph_generated_output_wire.end()){
			subgraph_generated_output_wire[info] = std::string(namebuffer);
		}
	}
	else if (this_node_type == TechMap_Op) {
		if (opack.verbose == "true")
			fmt::print("\t NodeID:{},bit [{}:{}] portid : {} \n",
			           this_idx, 0, 0, input->get_out_pin().get_pid());
		index_offset info = {this_idx, input->get_out_pin().get_pid(), {0, 0}};
		pid.push_back(info);
	}
	else if (this_node_type == Join_Op) {
		int width = 0;
		int width_pre = 0;
		uint32_t Join_width = g->get_bits(this_idx);
		std::vector<const Edge *> Join(Join_width);
		for (const auto &pre_inp : g->inp_edges(this_idx)) {
			Join[pre_inp.get_inp_pin().get_pid()] = &pre_inp;
		}
		for (Port_ID i = 0; i < Join_width; i++) {
			if (Join[i] == nullptr)
				break;
			width_pre = width;
			width += g->get_bits(Join[i]->get_idx());
			if (bit_addr[0] + 1 <= width) {
				bit_addr[0] = bit_addr[0] - width_pre;
				bit_addr[1] = bit_addr[1] - width_pre;
				recursive_find(g, Join[i], pid, bit_addr);
				break;
			}
		}
	}
	else if (this_node_type == Pick_Op) {
		int width = 0;
		int offset = 0;
		int pick_width = g->get_bits(this_idx);
		int upper = 0;
		int lower = 0;
		for (const auto &pre_inp : g->inp_edges(this_idx)) {
			switch (pre_inp.get_inp_pin().get_pid()) {
				case 0: {
					width = g->get_bits(pre_inp.get_idx());
					break;
				}
				case 1: {
					assert (g->node_type_get(pre_inp.get_idx()).op == U32Const_Op);
					offset = g->node_value_get(pre_inp.get_idx());
					break;
				}
				default:
					break;
			}
		}
		upper = offset + pick_width - 1;
		lower = offset;
		assert(bit_addr[1] - bit_addr[0] + 1 <= width);
		assert(upper - lower + 1 <= width);
		for (const auto &pre_inp : g->inp_edges(this_idx)) {
			if (pre_inp.get_inp_pin().get_pid() == 0) {
				bit_addr[0] += lower;
				bit_addr[1] += lower;
				recursive_find(g, &pre_inp, pid, bit_addr);
			}
		}
	}
}

/************************************************************************
 * Function:  Inou_abc::is_techmap
 * --------------------
 * input arg0 -> const LGraph *g
 *
 * returns: true/false
 *
 * description: iterate the lgraph to see it is a valid graph to pass abc
 *
 ***********************************************************************/
bool Inou_abc::is_techmap(const LGraph *g) {

	bool is_valid_input = true;

	for (const auto &idx : g->fast()) {
		switch (g->node_type_get(idx).op) {
			case TechMap_Op: {
				const Tech_cell *tcell = g->get_tlibrary()->get_const_cell(g->tmap_id_get(idx));
				if (is_latch(tcell)) {
					latch_id.push_back(idx);
				}
				else {
					combinational_id.push_back(idx);
				}
				break;
			}
			case Pick_Op: {
				int width = 0;
				int pick_width = g->get_bits(idx);
				int offset = 0;
				for (const auto &c : g->inp_edges(idx)) {
					switch (c.get_inp_pin().get_pid()) {
						case 0: {
							width = c.get_bits();
							break;
						}
						case 1: {
							offset = g->node_value_get(c.get_idx());
							break;
						}
						default: {
							fmt::print("\tPick_Op has more than two inputs!!\n");
							assert(false);
							is_valid_input = false;
						}
					}
				}
				int upper = offset + pick_width - 1;
				int lower = offset;
				assert(upper - lower + 1 <= width);
			}
			case Join_Op: {
				int width = g->get_bits(idx);
				if (width > 1) {
					/**********************************************************************************************
					 * If a Join_Op node has output width > 1:
					 * 	1. MUST NOT be the fanin of a TechMap_Op node (it should traverse through a Pick_OP!!)
					 * 	2. MUST NOT be the fanin of U32Const_Op | StrConst_Op node (undefined behavior!!)
					 * 	3. The width of GraphIO_Op nodes MUST Match (it should traverse through a Pick_OP!!)
					 **********************************************************************************************/
					for (const auto &out: g->out_edges(idx)) {
						switch (g->node_type_get(out.get_idx()).op) {
							case TechMap_Op : {
								const Tech_cell *tcell = g->get_tlibrary()->get_const_cell(g->tmap_id_get(out.get_idx()));
								std::string cell_name = tcell->get_name();
								console->error("nodeID:{} type:Join_Op has output to idx:{} cell_name: {}; mismatch in data width!\n",
												idx, out.get_idx(), cell_name);
								is_valid_input = false;
								break;
							}
							case U32Const_Op: {
								console->error("nodeID:{} type:Join_Op has output to a U32ConstNode:{}; undefined behavior!\n",
								               idx, out.get_idx());
								is_valid_input = false;
								break;
							}
							case StrConst_Op: {
								console->error("nodeID:{} type:Join_Op has output to a StrConstNode:{}; undefined behavior!\n",
								               idx, out.get_idx());
								is_valid_input = false;
								break;
							}
							case GraphIO_Op: {
								if (g->get_bits(out.get_idx()) != width) {
									console->error("nodeID:{} type:Join_Op has output to a GraphIONode:{}; mismatch in data width!\n",
									               idx, out.get_idx());
									is_valid_input = false;
								}
								break;
							}
							default: {
								break;
							}
						}
					}
				}
			}
			case U32Const_Op: {
				break;
			}
			case StrConst_Op: {
				break;
			}
			case GraphIO_Op: {
				if (g->is_graph_input(idx))
					graphio_input_id.push_back(idx);
				else
					graphio_output_id.push_back(idx);
				break;
			}
			case SubGraph_Op: {
				subgraph_id.push_back(idx);
				break;
			}
			default: {
				console->error("nodeID:{} op is not supported, opType is {}\n",
				               idx, g->node_type_get(idx).get_name().c_str());
				is_valid_input = false;
				break;
			}
		}
	}
	return is_valid_input;
}