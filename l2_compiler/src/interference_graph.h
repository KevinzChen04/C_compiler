#pragma once
#include "program.h"
#include "liveness.h"
#include "utils.h"
#include <assert.h>
#include <map>
#include <vector>
#include <tuple>
#include <algorithm>
#include <iterator>

namespace L2::program::analyze {

	// Prevents self-edges; attempts to create them will be ignored.
	template<typename N>
	class ColoringGraph {
		public:

		using Node = N;
		using Edge = bool;
		using Color = int;
		struct NodeInfo {
			Node node;
			std::vector<std::size_t> adj_vec; // includes disabled nodes
			std::optional<Color> color;
			int degree = 0; // only counts enabled nodes
			bool is_enabled = true;
		};

		private:

		std::map<Node, std::size_t> node_map;
		std::vector<NodeInfo> data;

		public:

		ColoringGraph(const std::vector<Node> &nodes) : node_map {}, data {} {
			this->data.resize(nodes.size());
			for (std::size_t i = 0; i < nodes.size(); ++i) {
				this->node_map.insert(std::make_pair(nodes[i], i));
				this->data[i].node = nodes[i];
			}
		}

		const std::map<Node, std::size_t> &get_node_map() const {
			return this->node_map;
		}

		const NodeInfo &get_node_info(Node node) const {
			std::size_t u = this->node_map.at(node);
			return this->get_node_info(u);
		}
		const NodeInfo &get_node_info(std::size_t u) const {
			return this->data[u];
		}

		// Checks whether two nodes conflict. Both must be enabled for them to
		// conflict.
		bool check_color_conflict(Node node_a, Node node_b) const {
			std::size_t u = this->node_map.at(node_a);
			std::size_t v = this->node_map.at(node_b);
			return this->check_color_conflict(u, v);
		}
		bool check_color_conflict(std::size_t u, std::size_t v) const {
			auto &u_info = this->data[u];
			auto &v_info = this->data[v];
			return u_info.is_enabled && v_info.is_enabled
				&& u_info.color && v_info.color
				&& *u_info.color == v_info.color;
		}

		// Checks whether a node conflicts with any of its enabled neighbors.
		bool check_color_conflict(Node node) const {
			std::size_t u = this->node_map.at(node);
			return this->check_color_conflict(u);
		}
		bool check_color_conflict(std::size_t u) const {
			if (!this->data[u].is_enabled) {
				return false;
			}

			for (std::size_t v : this->data[u].adj_vec) {
				if (this->check_color_conflict(v, u)) {
					return true;
				}
			}
			return false;
		}

		void add_edge(Node node_a, Node node_b) {
			std::size_t u = this->node_map.at(node_a);
			std::size_t v = this->node_map.at(node_b);
			return this->add_edge(u, v);
		}
		void add_edge(std::size_t u, std::size_t v) {
			if (u == v) {
				return;
			}
			if (this->check_color_conflict(u, v)) {
				std::cerr << "Cannot add an edge between two nodes of the same color\n";
				exit(1);
			}

			NodeInfo &u_info = this->data[u];
			NodeInfo &v_info = this->data[v];
			const auto [ui, uj] = std::equal_range(
				u_info.adj_vec.begin(),
				u_info.adj_vec.end(),
				v
			);
			if (ui == uj) {
				// u's adj list does not have v
				u_info.adj_vec.insert(ui, v);
				if (v_info.is_enabled) {
					u_info.degree += 1;
				}

				// since u != v, by symmetry, v's adj list must not have u
				const auto [vi, vj] = std::equal_range(
					v_info.adj_vec.begin(),
					v_info.adj_vec.end(),
					u
				);
				assert(vi == vj);
				v_info.adj_vec.insert(vi, u);
				if (u_info.is_enabled) {
					v_info.degree += 1;
				}
			}
		}

		void add_clique(const utils::set<Node> &nodes) {
			for (auto it_a = nodes.begin(); it_a != nodes.end(); ++it_a) {
				auto it_b = it_a;
				++it_b;
				for (; it_b != nodes.end(); ++it_b) {
					this->add_edge(*it_a, *it_b);
				}
			}
		}

		// adds all possible edges between a node in group_a and a node in group_b
		// avoids adding self-edges
		void add_total_bipartite(const utils::set<Node> &group_a, const utils::set<Node> &group_b) {
			for (Node node_a : group_a) {
				for (Node node_b : group_b) {
					if (node_a != node_b) {
						this->add_edge(node_a, node_b);
					}
				}
			}
		}

		// bool get_edge(Node u, Node v) const {
		// 	assert(u < this->node_map.size() && v < this->node_map.size());
		// 	auto &[u_adj_vec, u_enabled] = this->data[u];
		// 	return this->data[v].enabled
		// 		&& u_enabled
		// 		&& std::binary_search(u_adj_vec.begin(), u_adj_vec.end(), v);
		// }

		void disable_node(Node node) {
			std::size_t u = this->node_map.at(node);
			if (!this->data[u].is_enabled) {
				return;
			}

			this->data[u].is_enabled = false;
			for (std::size_t neighbor_idx : this->data[u].adj_vec) {
				this->data[neighbor_idx].degree -= 1;
			}
		}

		// Enables a node with the specified color.
		// Will error if there are any color conflicts.
		void attempt_enable_with_color(Node node, std::optional<Color> color) {
			std::size_t u = this->node_map.at(node);
			NodeInfo &node_info = this->data[u];
			bool prev_enabled = node_info.is_enabled;
			node_info.color = color;
			node_info.is_enabled = true;
			if (this->check_color_conflict(u)) {
				std::cerr << "Error: attempted to give a node a color that conflicts.\n";
				exit(1);
			}
			if (!prev_enabled) {
				for (std::size_t neighbor_idx : node_info.adj_vec) {
					this->data[neighbor_idx].degree += 1;
				}
			}
		}

		void verify_no_conflicts() const {
			for (std::size_t i = 0; i < this->data.size(); ++i) {
				if (this->check_color_conflict(i)) {
					std::cerr << "Error: color conflict\n";
					exit(1);
				}
			}
		}

		// assumes that every node is colored
		std::map<Node, Color> get_coloring() const {
			std::map<Node, Color> result;
			for (const NodeInfo &node_info : this->data) {
				result.insert(std::make_pair(node_info.node, *node_info.color));
			}
			return result;
		}

		std::string to_string() const {
			std::string result;
			for (const NodeInfo &node_info : this->data) {
				// if (node_info.is_enabled) {
				// 	result += "[";
				// } else {
				// 	result += "-";
				// }
				result += node_info.node->to_string() + " " /* + std::to_string(node_info.degree) + " " */;
				for (std::size_t neighbor_index : node_info.adj_vec) {
					// if (this->data[neighbor_index].is_enabled) {
					// 	result += "[";
					// }
					result += this->data[neighbor_index].node->to_string();
					// if (this->data[neighbor_index].is_enabled) {
					// 	result += "]";
					// }
					result += " ";
				}
				result += "\n";
			}
			return result;
		}
	};

	using VariableGraph = ColoringGraph<const Variable *>;

	VariableGraph generate_interference_graph(
		L2Function &l2_function,
		const InstructionsAnalysisResult &inst_analysis,
		const std::vector<const Register *> &register_color_table
	);

	// Given a GoloringGraph, tries to color it with the colors 0..num_colors.
	// Pre-colored nodes are allowed.
	// Returns none if it could color the graph,
	// else returns a vector of the Variables that could not be colored.
	std::vector<VariableGraph::Node> attempt_color_graph(
		VariableGraph &graph,
		const std::vector<const Register *> &register_color_table
	);
}