#include "tracer.h"

namespace IR::tracer {
    Vec<Vec<double>> make_link_matrix(const Vec<Uptr<BasicBlock>> &blocks) {
        // map each BasicBlock * to its index
        Map<BasicBlock *, int> block_index_map;
        for (int i = 0; i < blocks.size(); ++i) {
            block_index_map.insert_or_assign(blocks[i].get(), i);
        }

        Vec<Vec<double>> result;
        for (const Uptr<BasicBlock> &block : blocks) {
            // make a vector of zeros
            Vec<double> &row = result.emplace_back(blocks.size());

            // add all its children
            int n = block->get_successors().size();
            for (auto [succ, priority] : block->get_successors()) {
                row[block_index_map.at(succ)] = priority;
            }
        }
        return result;
    }

    void incorporate_damping_factor(Vec<Vec<double>> &transition_matrix, double damping_factor) {
        int num_nodes = transition_matrix.size();
        double rand_chance = 1.0 - damping_factor; // the chance of jumping to a random node
        double distributed_rand_chance = rand_chance / num_nodes; // the chance of jumping to any particular node
        for (Vec<double> &row : transition_matrix) {
            for (double &element : row) {
                element *= damping_factor;
                element += distributed_rand_chance;
            }

            // normalize probabilities
            double sum = 0.0;
            for (double element : row) {
                sum += element;
            }
            // sum is guaranteed to not be zero because of the damping factor
            for (double & element : row) {
                element /= sum;
            }
        }
    }

    void transpose(Vec<Vec<double>> &matrix) {
        int n = matrix.size();
        for (int r = 0; r < n; ++r) {
            for (int c = r + 1; c < n; ++c) {
                std::swap(matrix[r][c], matrix[c][r]);
            }
        }
    }

    Vec<double> find_steady_state(Vec<Vec<double>> transition_matrix) {
        int num_nodes = transition_matrix.size();
        Vec<Vec<double>> &mat = transition_matrix;

        // convert the transition matrix P into A = I - P^T,
        transpose(mat);
        for (int r = 0; r < num_nodes; ++r) {
            for (int c = 0; c < num_nodes; ++c) {
                mat[r][c] *= -1.0;
                if (r == c) {
                    mat[r][c] += 1.0;
                }
            }
        }

        // solve A * s = 0

        // add one equation to stipulate that the steady state vector be normalized
        // mat.emplace_back(num_nodes);

        // iterate through the pivots
        for (int p = 0; p < num_nodes - 1; ++p) {
            // ignore the first p rows

            // find the row with the largest element in the pivot column
            auto max_row_it = std::max_element(
                mat.begin() + p,
                mat.end(),
                [p](const Vec<double> &a, const Vec<double> &b) { return a[p] < b[p]; }
            );

            // move the maximum row to the pth row, to serve as the pivot row
            max_row_it->swap(mat[p]);

            // normalize the pivot row
            double pivot_value = mat[p][p];
            assert(pivot_value != 0.0);
            for (double &element : mat[p]) {
                element /= pivot_value;
            }

            // eliminate all other rows
            for (int r = 0; r < num_nodes; ++r) {
                if (r == p) continue;
                double first_value = mat[r][p];
                for (int c = p; c < num_nodes; ++ c) {
                    mat[r][c] -= first_value * mat[p][c];
                }

                // although it should already be zero, roundoff error might happen
                mat[r][p] = 0;
            }
        }

        // extract the solutions from the free variable
        Vec<double> solution;
        double sum = 0.0;
        for (const Vec<double> &row : mat) {
            double solution_component = -row.back();
            solution.push_back(solution_component);
            sum += solution_component;
        }
        solution.back() = 1.0;
        sum += 1.0;
        for (double &x : solution) {
            x /= sum;
        }
        return solution;
    }

    struct BbEdge {
        double weight;
        BasicBlock *from;
        BasicBlock *to;

        BbEdge(double weight, BasicBlock *from, BasicBlock *to) :
            weight { weight }, from { from }, to { to }
        {}
    };
    bool operator<(const BbEdge &a, const BbEdge &b) {
        return a.weight < b.weight;
    }

    Vec<Trace> trace_cfg(const Vec<Uptr<BasicBlock>> &blocks) {
        // calculate how popular each block is its "rank"
        Vec<Vec<double>> transition_matrix = make_link_matrix(blocks);
        incorporate_damping_factor(transition_matrix, 0.85);
        Vec<double> block_ranks = find_steady_state(mv(transition_matrix));

        // store all the edges by their weight
        std::priority_queue<BbEdge> edges;
        for (int from_index = 0; from_index < blocks.size(); ++from_index) {
            BasicBlock *from_block = blocks[from_index].get();
            for (const auto [succ_block, priority] : from_block->get_successors()) {
                double weight = priority * block_ranks[from_index];
                edges.emplace(weight, from_block, succ_block);
            }
        }

        // create one trace for each block
        using TraceIter = Vec<Trace>::iterator;
        Vec<Trace> traces;
        traces.reserve(blocks.size()); // REQUIRED so that iterators are not invalidated
        Map<BasicBlock *, TraceIter> trace_begins; // tracks the trace that starts with a block
        Map<BasicBlock *, TraceIter> trace_ends; // tracks the trace that ends with a block
        for (const Uptr<BasicBlock> &block : blocks) {
            TraceIter trace_iter = traces.emplace(traces.end());
            trace_iter->block_sequence.push_back(block.get());
            trace_begins.insert_or_assign(block.get(), trace_iter);
            trace_ends.insert_or_assign(block.get(), trace_iter);
        }

        // try to merge traces, using the heaviest edges first
        while (!edges.empty()) {
            BbEdge edge = edges.top();
            edges.pop();

            // std::cout << edge.from->get_name() << " -> " << edge.to->get_name() << " " << edge.weight << "\n";

            auto trace_head_iter = trace_ends.find(edge.from);
            auto trace_tail_iter = trace_begins.find(edge.to);
            if (trace_head_iter == trace_ends.end()
                || trace_tail_iter == trace_begins.end()
                || trace_head_iter->second == trace_tail_iter->second)
            {
                // this means that this edge stitches together two blocks that
                // aren't the endpoints of a trace, or merges a trace to itself,
                // which is not allowed
                // std::cout << "skipping link\n";
                continue;
            }

            // std::cout << "merging link\n";

            // attempt to merge edge.from and edge.to
            Vec<BasicBlock *> &trace_head_seq = trace_head_iter->second->block_sequence;
            Vec<BasicBlock *> &trace_tail_seq = trace_tail_iter->second->block_sequence;
            // move all elements from the tail trace to the head trace
            trace_head_seq.insert(trace_head_seq.end(), trace_tail_seq.begin(), trace_tail_seq.end());
            trace_tail_seq.clear();

            // modify trace_begins and trace_ends to point to the right traces
            trace_begins.erase(trace_tail_iter);
            trace_ends.erase(trace_head_iter);
            trace_ends.insert_or_assign(trace_head_seq.back(), trace_head_iter->second);
        }

        // remove the empty traces
        auto remove_begin = std::remove_if(
            traces.begin(),
            traces.end(),
            [](const Trace &trace) { return trace.block_sequence.empty(); }
        );
        traces.erase(remove_begin, traces.end());

        return traces;
    }
}