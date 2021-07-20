//#define debug_distance_indexing
//#define debug_snarl_traversal
//#define debug_distances

#include "snarl_distance_index.hpp"

using namespace std;
using namespace handlegraph;
namespace vg {


make_distance_index(bdsg::SnarlDistanceIndex* distance_index, const HandleGraph* graph, const HandleGraphSnarlFinder* snarl_finder, size_t size_limit) {
    distance_index->set_snarl_size_limit(size_limit);

    //Build the temporary distance index from the graph
    SnarlDisanceIndex::TemporaryDistanceIndex temp_index = make_temporary_index(graph, snarl_finder, size_limit);

    //And fill in the permanent distance index
    vector<const TemporaryDistanceIndex*> indexes;
    indexes.emplace_back(&temp_index);
    get_snarl_tree_records(indexes, graph);
}
SnarlDistanceIndex::TemporaryDistanceIndex::TemporaryDistanceIndex make_temporary_distance_index(
    const HandleGraph* graph, const HandleGraphSnarlFinder* snarl_finder, size_t size_limit) :
    min_node_id(graph->min_node_id()), max_node_id(graph->max_node_id()) {

#ifdef debug_distance_indexing
    cerr << "Creating new distance index for nodes between " << graph->min_node_id() << " and " << graph->max_node_id() << endl;

#endif
    //Construct the distance index using the snarl decomposition
    //traverse_decomposition will visit all structures (including trivial snarls), calling
    //each of the given functions for the start and ends of the snarls and chains
    temp_node_records.resize(max_node_id-min_node_id+1);



    //Stores unfinished records, as type of record and offset into appropriate vector
    //(temp_node/snarl/chain_records)
    vector<pair<temp_record_t, size_t>> stack;

    //There may be components of the root that are connected to each other. Each connected component will
    //get put into a (fake) root-level snarl, but we don't know what those components will be initially,
    //since the decomposition just puts them in the same root snarl. This is used to group the root-level
    //components into connected components that will later be used to make root snarls
    structures::UnionFind root_snarl_component_uf (0);


    /*Go through the decomposition top down and record the connectivity of the snarls and chains
     * Distances will be added later*/

    snarl_finder->traverse_decomposition(
    [&](handle_t chain_start_handle) {
        /*This gets called when a new chain is found, starting at the start handle going into chain
         * For the first node in a chain, create a chain record and fill in the first node.
         * Also add the first node record
         */
#ifdef debug_distance_indexing
        cerr << "  Starting new chain at " << graph->get_id(chain_start_handle) << (graph->get_is_reverse(chain_start_handle) ? " reverse" : " forward") << endl;
        //We shouldn't have seen this node before
        //assert(temp_node_records[graph->get_id(chain_start_handle)-min_node_id].node_id == 0);
#endif

        //Fill in node in chain
        stack.emplace_back(TEMP_CHAIN, temp_chain_records.size());
        id_t node_id = graph->get_id(chain_start_handle);
        temp_chain_records.emplace_back();
        auto& temp_chain = temp_chain_records.back();
        temp_chain.start_node_id = node_id; 
        temp_chain.start_node_rev = graph->get_is_reverse(chain_start_handle);
        temp_chain.children.emplace_back(TEMP_NODE, node_id);

        //And the node record itself
        auto& temp_node = temp_node_records.at(node_id-min_node_id);
        temp_node.node_id = node_id;
        temp_node.node_length = graph->get_length(chain_start_handle);
        temp_node.reversed_in_parent = graph->get_is_reverse(chain_start_handle);
        temp_node.parent = stack.back(); //The parent is this chain

    },
    [&](handle_t chain_end_handle) {
        /*This gets called at the end of a chain, facing out
         * Record the chain's end node. The node record itself would have been added as part of the snarl
         * Also record the chain's parent here
         */

        //Done with this chain
        pair<temp_record_t, size_t> chain_index = stack.back();
        stack.pop_back();

        assert(chain_index.first == TEMP_CHAIN);
        TemporaryChainRecord& temp_chain_record = temp_chain_records.at(chain_index.second);
        id_t node_id = graph->get_id(chain_end_handle);

        //Fill in node in chain
        temp_chain_record.end_node_id = node_id;
        temp_chain_record.end_node_rev = graph->get_is_reverse(chain_end_handle);
        temp_chain_record.end_node_length = graph->get_length(chain_end_handle);

        //Check if this is a trivial chain that represents a node
        if (temp_chain_record.children.size() == 1 && temp_chain_record.start_node_id == temp_chain_record.end_node_id) {
            temp_chain_record.is_trivial = true;
            temp_chain_record.start_node_rev = false;
            temp_chain_record.end_node_rev = false;
        }

        //TODO: Add root-level snarls
        if (stack.empty()) {
            //If this was the last thing on the stack, then this was a root

            //Check to see if there is anything connected to the ends of the chain
            vector<id_t> reachable_nodes;
            graph->follow_edges(graph->get_handle(temp_chain_record.start_node_id, !temp_chain_record.start_node_rev),
                false, [&] (const handle_t& next) {
                    if (graph->get_id(next) != temp_chain_record.start_node_id &&
                        graph->get_id(next) != temp_chain_record.end_node_id) {
                        reachable_nodes.emplace_back(graph->get_id(next));
                    }
                });
            graph->follow_edges(graph->get_handle(temp_chain_record.end_node_id, temp_chain_record.end_node_rev),
                false, [&] (const handle_t& next) {
                    if (graph->get_id(next) != temp_chain_record.start_node_id &&
                        graph->get_id(next) != temp_chain_record.end_node_id) {
                        reachable_nodes.emplace_back(graph->get_id(next));
                    }
                });
            if (reachable_nodes.size() && (temp_chain_record.is_trivial || temp_chain_record.start_node_id != temp_chain_record.end_node_id)) {
                //If we can reach anything leaving the chain (besides the chain itself), then it is part of a root snarl
                //Note that if the chain's start and end node are the same, then it will always be a single component
#ifdef debug_distance_indexing
                cerr << "                 This chain is part of the root but connects with something else in the root"<<endl;
#endif
                bool new_component = true;

                //Add this to the union find
                root_snarl_component_uf.resize(root_snarl_component_uf.size() + 1);
                //And remember that it's in a connected component of the root
                temp_chain_record.root_snarl_index = root_snarl_components.size();
                root_snarl_components.emplace_back(chain_index);
                for (id_t next_id : reachable_nodes) {
                    //For each node that this is connected to, check if we've already seen it and if we have, then
                    //union this chain and that node's chain
                    TemporaryNodeRecord& node_record = temp_node_records[next_id-min_node_id];
                    if (node_record.node_id != 0) {
                        //If we've already seen this node, union it with the new one
                        //If we can see it by walking out from this top-level chain, then it must also be a
                        //top-level chain (or node pretending to be a chain)
                        assert(node_record.parent.first == TEMP_CHAIN);
                        size_t other_i = temp_chain_records[node_record.parent.second].root_snarl_index;
                        assert(other_i != std::numeric_limits<size_t>::max());
                        root_snarl_component_uf.union_groups(other_i, temp_chain_record.root_snarl_index);
#ifdef debug_distance_indexing
                        cerr << "        Union this chain with " << temp_chain_records[node_record.parent.second].start_node_id << " " << temp_chain_records[node_record.parent.second].end_node_id << endl;
#endif
                    } else {
                        new_component = false;
                    }
                }
                if (new_component) {
                    root_structure_count += 1;
                }
            } else {
                //If this chain isn't connected to anything else, then it is a single component of the root
                temp_chain_record.parent = make_pair(TEMP_ROOT, 0);
                components.emplace_back(chain_index);
                root_structure_count += 1;
            }
        } else {
            //The last thing on the stack is the parent of this chain, which must be a snarl
            temp_chain_record.parent = stack.back();
            auto& parent_snarl_record = temp_snarl_records.at(temp_chain_record.parent.second);
            temp_chain_record.rank_in_parent = parent_snarl_record.children.size() + 2;
            parent_snarl_record.children.emplace_back(chain_index);
        }

#ifdef debug_distance_indexing
        cerr << "  Ending new " << (temp_chain_record.is_trivial ? "trivial " : "") <<  "chain " << structure_start_end_as_string(chain_index)
             << endl << "    that is a child of " << structure_start_end_as_string(temp_chain_record.parent) << endl;
#endif
    },
    [&](handle_t snarl_start_handle) {
        /*This gets called at the beginning of a new snarl facing in
         * Create a new snarl record and fill in the start node.
         * The node record would have been created as part of the chain, or as the end node
         * of the previous snarl
         */

#ifdef debug_distance_indexing
        cerr << "  Starting new snarl at " << graph->get_id(snarl_start_handle) << (graph->get_is_reverse(snarl_start_handle) ? " reverse" : " forward") << endl;
#endif
        stack.emplace_back(TEMP_SNARL, temp_snarl_records.size());
        temp_snarl_records.emplace_back();
        temp_snarl_records.back().start_node_id = graph->get_id(snarl_start_handle);
        temp_snarl_records.back().start_node_rev = graph->get_is_reverse(snarl_start_handle);
        temp_snarl_records.back().start_node_length = graph->get_length(snarl_start_handle);
    },
    [&](handle_t snarl_end_handle){
        /*This gets called at the end of the snarl facing out
         * Fill in the end node of the snarl, its parent, and record the snarl as a child of its
         * parent chain
         * Also create a node record
         */
        pair<temp_record_t, size_t> snarl_index = stack.back();
        stack.pop_back();
        assert(snarl_index.first == TEMP_SNARL);
        assert(stack.back().first == TEMP_CHAIN);
        TemporarySnarlRecord& temp_snarl_record = temp_snarl_records[snarl_index.second];
        id_t node_id = graph->get_id(snarl_end_handle);

        //Record the end node in the snarl
        temp_snarl_record.end_node_id = node_id;
        temp_snarl_record.end_node_rev = graph->get_is_reverse(snarl_end_handle);
        temp_snarl_record.end_node_length = graph->get_length(snarl_end_handle);
        temp_snarl_record.node_count = temp_snarl_record.children.size();
        if (temp_snarl_record.children.size() == 0) {
            temp_snarl_record.is_trivial = true;
        }
        //Record the snarl as a child of its chain
        if (stack.empty()) {
            assert(false);
            //TODO: The snarl should always be the child of a chain
            //If this was the last thing on the stack, then this was a root
            //TODO: I'm not sure if this would get put into a chain or not
            temp_snarl_record.parent = make_pair(TEMP_ROOT, 0);
            root_structure_count += 1;
            components.emplace_back(snarl_index);
        } else {
            //This is the child of a chain
            assert(stack.back().first == TEMP_CHAIN);
            temp_snarl_record.parent = stack.back();
            auto& temp_chain = temp_chain_records.at(stack.back().second);
            temp_chain.children.emplace_back(snarl_index);
            temp_chain.children.emplace_back(TEMP_NODE, node_id);

        }

        //Record the node itself. This gets done for the start of the chain, and ends of snarls
        TemporaryNodeRecord& temp_node_record = temp_node_records.at(node_id-min_node_id);
        temp_node_record.node_id = node_id;
        temp_node_record.node_length = graph->get_length(snarl_end_handle);
        temp_node_record.reversed_in_parent = graph->get_is_reverse(snarl_end_handle);
        temp_node_record.parent = stack.back();

        //TODO: This isn't actually counting everything
        index_size += SnarlRecord::record_size(DISTANCED_SNARL, temp_snarl_record.node_count);

#ifdef debug_distance_indexing
        cerr << "  Ending new snarl " << structure_start_end_as_string(snarl_index)
             << endl << "    that is a child of " << structure_start_end_as_string(temp_snarl_record.parent) << endl;
#endif
    });

    /*
     * We finished going through everything that exists according to the snarl decomposition, but
     * it's still missing tips, which will be discovered when filling in the snarl distances,
     * and root-level snarls, which we'll add now by combining the chain components in root_snarl_components
     * into snarls defined by root_snarl_component_uf
     * The root-level snarl is a fake snarl that doesn't exist according to the snarl decomposition,
     * but is an extra layer that groups together components of the root that are connected
     */

    vector<vector<size_t>> root_snarl_component_indexes = root_snarl_component_uf.all_groups();
    for (vector<size_t>& root_snarl_indexes : root_snarl_component_indexes) {
#ifdef debug_distance_indexing
        cerr << "Create a new root snarl from components" << endl;
#endif
        //For each of the root snarls
        components.emplace_back(TEMP_SNARL, temp_snarl_records.size());
        temp_snarl_records.emplace_back();
        TemporarySnarlRecord& temp_snarl_record = temp_snarl_records.back();
        temp_snarl_record.is_root_snarl = true;
        temp_snarl_record.parent = make_pair(TEMP_ROOT, 0);


        for (size_t chain_i : root_snarl_indexes) {
            //For each chain component of this root-level snarl
            assert(root_snarl_components[chain_i].first == TEMP_CHAIN);
            TemporaryChainRecord& temp_chain_record = temp_chain_records[root_snarl_components[chain_i].second];
            temp_chain_record.parent = make_pair(TEMP_SNARL, temp_snarl_records.size() - 1);
            temp_chain_record.rank_in_parent = temp_snarl_record.children.size();
            temp_chain_record.reversed_in_parent = false;

            temp_snarl_record.children.emplace_back(root_snarl_components[chain_i]);
        }
        temp_snarl_record.node_count = temp_snarl_record.children.size();
    }

    /*Now go through the decomposition again to fill in the distances
     * This traverses all chains in reverse order that we found them in, so bottom up
     * Each chain and snarl already knows its parents and children, except for single nodes
     * that are children of snarls. These nodes were not in chains will have their node
     * records created here
     */

#ifdef debug_distance_indexing
    cerr << "Filling in the distances in snarls" << endl;
#endif
    for (int i = temp_chain_records.size()-1 ; i >= 0 ; i--) {

        TemporaryChainRecord& temp_chain_record = temp_chain_records[i];
#ifdef debug_distance_indexing
        cerr << "  At "  << (temp_chain_record.is_trivial ? " trivial " : "") << " chain " << structure_start_end_as_string(make_pair(TEMP_CHAIN, i)) << endl;
#endif

        //Add the first values for the prefix sum and backwards loop vectors
        temp_chain_record.prefix_sum.emplace_back(0);
        temp_chain_record.backward_loops.emplace_back(std::numeric_limits<int64_t>::max());
        temp_chain_record.chain_components.emplace_back(0);


        /*First, go through each of the snarls in the chain in the forward direction and
         * fill in the distances in the snarl. Also fill in the prefix sum and backwards
         * loop vectors here
         */
        size_t curr_component = 0; //which component of the chain are we in
        for (size_t chain_child_i = 0 ; chain_child_i < temp_chain_record.children.size() ; chain_child_i++ ){
            const pair<temp_record_t, size_t>& chain_child_index = temp_chain_record.children[chain_child_i];
            //Go through each of the children in the chain, skipping nodes
            //The snarl may be trivial, in which case don't fill in the distances
#ifdef debug_distance_indexing
            cerr << "    Looking at child " << structure_start_end_as_string(chain_child_index) << endl;
#endif

            if (chain_child_index.first == TEMP_SNARL){
                //This is where all the work gets done. Need to go through the snarl and add
                //all distances, then add distances to the chain that this is in
                //The parent chain will be the last thing in the stack
                TemporarySnarlRecord& temp_snarl_record = temp_snarl_records.at(chain_child_index.second);

                //Fill in this snarl's distances
                populate_snarl_index(temp_snarl_record, chain_child_index, size_limit, graph);

                int64_t length;
                bool new_component = false;
                //TODO: Double check these orientations
                if (temp_snarl_record.distances.count(make_pair(make_pair(0, false), make_pair(1, false)))){
                    length = temp_snarl_record.distances.at(make_pair(make_pair(0, false), make_pair(1, false)));
                } else if (temp_snarl_record.distances.count(make_pair(make_pair(1, false), make_pair(0, false)))){
                    length = temp_snarl_record.distances.at(make_pair(make_pair(1, false), make_pair(0, false)));
                } else {
                    //The snarl is not start-end connected
                    length = std::numeric_limits<int64_t>::max();
                    //Start a new component
                    curr_component ++;
                    new_component=true;
#ifdef debug_distance_indexing
            cerr << "      This snarl is not start-end connected, starting new chain component " << endl;
#endif
                }
                temp_snarl_record.min_length = length;

                //Get the loop distances for the snarl
                temp_snarl_record.loop_start =
                    temp_snarl_record.distances.count(make_pair(make_pair(0, false), make_pair(0, false)))
                  ? temp_snarl_record.distances.at(make_pair(make_pair(0, false), make_pair(0, false)))
                  : std::numeric_limits<int64_t>::max();
                temp_snarl_record.loop_end =
                    temp_snarl_record.distances.count(make_pair(make_pair(1, false), make_pair(1, false)))
                  ? temp_snarl_record.distances.at(make_pair(make_pair(1, false), make_pair(1, false)))
                  : std::numeric_limits<int64_t>::max();

                //And get the distance values for the end node of the snarl in the chain
                if (new_component) {
                    //If this snarl wasn't start-end connected, then we start tracking the distance vectors
                    //here

                    //If this is the second component of the multicomponent chain, then remember the minimum length
                    if (curr_component == 1) {
                        temp_chain_record.min_length = temp_chain_record.prefix_sum.back();
                    }
                    temp_chain_record.prefix_sum.emplace_back(0);
                    temp_chain_record.backward_loops.emplace_back(temp_snarl_record.loop_end);
                } else {
                    temp_chain_record.prefix_sum.emplace_back(sum({temp_chain_record.prefix_sum.back(),
                        temp_snarl_record.min_length, temp_snarl_record.start_node_length}));
                    temp_chain_record.backward_loops.emplace_back(std::min(temp_snarl_record.loop_end,
                        sum({temp_chain_record.backward_loops.back()
                        , 2 * (temp_snarl_record.start_node_length + temp_snarl_record.min_length)})));
                }
                temp_chain_record.chain_components.emplace_back(curr_component);
                if (chain_child_i == temp_chain_record.children.size() - 2 && temp_snarl_record.min_length == std::numeric_limits<int64_t>::max()) {
                    temp_chain_record.loopable = false;
                }
            }
        } //Finished walking through chain
        if (temp_chain_record.start_node_id == temp_chain_record.end_node_id && temp_chain_record.chain_components.back() != 0) {
            //If this is a looping, multicomponent chain, the start/end node could end up in separate chain components
            //despite being the same node.
            //Since the first component will always be 0, set the first node's component to be whatever the last
            //component was
            temp_chain_record.chain_components[0] = temp_chain_record.chain_components.back();

        }
        temp_chain_record.min_length = !temp_chain_record.is_trivial && temp_chain_record.start_node_id == temp_chain_record.end_node_id
                        ? sum({temp_chain_record.prefix_sum.back(), temp_chain_record.min_length})
                        : sum({temp_chain_record.prefix_sum.back() , temp_chain_record.end_node_length});

        assert(temp_chain_record.prefix_sum.size() == temp_chain_record.backward_loops.size());
        assert(temp_chain_record.prefix_sum.size() == temp_chain_record.chain_components.size());


        /*Now that we've gone through all the snarls in the chain, fill in the forward loop vector
         * by going through the chain in the backwards direction
         */
        temp_chain_record.forward_loops.resize(temp_chain_record.prefix_sum.size(),
                                               std::numeric_limits<int64_t>::max());
        if (temp_chain_record.start_node_id == temp_chain_record.end_node_id && temp_chain_record.children.size() > 1) {
            //If this is a looping chain, then check the first snarl for a loop
            TemporarySnarlRecord& temp_snarl_record = temp_snarl_records.at(temp_chain_record.children.at(1).second);
            temp_chain_record.forward_loops[temp_chain_record.forward_loops.size()-1] = temp_snarl_record.loop_start;


            //Also check if the reverse loop values would be improved if we went around again
            if (temp_chain_record.backward_loops.back() < temp_chain_record.backward_loops.front()) {
                temp_chain_record.backward_loops[0] = temp_chain_record.backward_loops.back();
            }
        }

        size_t node_i = temp_chain_record.prefix_sum.size() - 2;
        // We start at the next to last node because we need to look at this record and the next one.

        for (int j = (int)temp_chain_record.children.size() - 1 ; j >= 0 ; j--) {
            auto& child = temp_chain_record.children.at(j);
            if (child.first == TEMP_SNARL){
                TemporarySnarlRecord& temp_snarl_record = temp_snarl_records.at(child.second);
                if (temp_chain_record.chain_components.at(node_i) != temp_chain_record.chain_components.at(node_i+1)){
                    //If this is a new chain component, then add the loop distance from the snarl
                    temp_chain_record.forward_loops.at(node_i) = temp_snarl_record.loop_start;
                } else {
                    temp_chain_record.forward_loops.at(node_i) =
                        std::min(sum({temp_chain_record.forward_loops.at(node_i+1) , 2* temp_snarl_record.min_length,
                                      2*temp_snarl_record.end_node_length}), temp_snarl_record.loop_start);
                }
                node_i --;
            }
        }
    }

#ifdef debug_distance_indexing
    cerr << "Filling in the distances in root snarls" << endl;
#endif
    for (pair<temp_record_t, size_t>& component_index : components) {
        if (component_index.first == TEMP_SNARL) {
            TemporarySnarlRecord& temp_snarl_record = temp_snarl_records.at(component_index.second);
            populate_snarl_index(temp_snarl_record, component_index, size_limit, graph);
            temp_snarl_record.min_length = std::numeric_limits<int64_t>::max();//TODO: This is true but might be better to store it as something else so we can bit compress later
        }
    }
}


/*Fill in the snarl index.
 * The index will already know its boundaries and everything knows their relationships in the
 * snarl tree. This needs to fill in the distances and the ranks of children in the snarl
 * The rank of a child is arbitrary, except that the start node will always be 0 and the end node
 * will always be the node count+1 (since node count doesn't count the boundary nodes)
 */
void SnarlDistanceIndex::TemporaryDistanceIndex::populate_snarl_index(
                TemporaryDistanceIndex::TemporarySnarlRecord& temp_snarl_record,
                pair<temp_record_t, size_t> snarl_index, size_t size_limit, const HandleGraph* graph) {
#ifdef debug_distance_indexing
    cerr << "Getting the distances for snarl " << structure_start_end_as_string(snarl_index) << endl;
    assert(snarl_index.first == TEMP_SNARL);
#endif




    /*Helper function to find the ancestor of a node that is a child of this snarl */
    auto get_ancestor_of_node = [&](pair<temp_record_t, size_t> curr_index) {

        //This is a child that isn't a node, so it must be a chain
        if (curr_index.second == temp_snarl_record.start_node_id || 
            curr_index.second == temp_snarl_record.end_node_id) {
            return curr_index;
        }

        //Otherwise, walk up until we hit the current snarl
        pair<temp_record_t, size_t> parent_index = temp_node_records.at(curr_index.second-min_node_id).parent;
        while (parent_index != snarl_index) {
            curr_index=parent_index;
            parent_index = parent_index.first == TEMP_SNARL ? temp_snarl_records.at(parent_index.second).parent
                                                            : temp_chain_records.at(parent_index.second).parent;
#ifdef debug_distance_indexing
            assert(parent_index.first != TEMP_ROOT); 
#endif
        }
        
        return curr_index;
    };


    /*Now go through each of the children and add distances from that child to everything reachable from it
     * Start a dijkstra traversal from each node side in the snarl and record all distances
     */

    //Add the start and end nodes to the list of children so that we include them in the traversal 
    //TODO: Copying the list
    vector<pair<temp_record_t, size_t>> all_children = temp_snarl_record.children;
    if (!temp_snarl_record.is_root_snarl) {


        all_children.emplace_back(TEMP_NODE, temp_snarl_record.start_node_id);
        all_children.emplace_back(TEMP_NODE, temp_snarl_record.end_node_id);
    }

    while (!all_children.empty()) {
        const pair<temp_record_t, size_t> start_index = std::move(all_children.back());
        all_children.pop_back();

        //Check if this node is a tip
        if ((start_index.first == TEMP_NODE 
             && start_index.second != temp_snarl_record.start_node_id 
             && start_index.second != temp_snarl_record.end_node_id) 
            || 
            (start_index.first == TEMP_CHAIN && temp_chain_records.at(start_index.second).is_trivial)) {
            id_t node_id = start_index.first == TEMP_NODE ? start_index.second : temp_chain_records.at(start_index.second).start_node_id;
            size_t rank = start_index.first == TEMP_NODE ? temp_node_records.at(start_index.second-min_node_id).rank_in_parent 
                                                          : temp_chain_records.at(start_index.second).rank_in_parent;
            
            bool has_edges = false;
            graph->follow_edges(graph->get_handle(node_id, false), false, [&](const handle_t next_handle) {
                has_edges = true;
            });
            if (!has_edges) {
                temp_node_records.at(node_id-min_node_id).is_tip = true;
                temp_snarl_record.tippy_child_ranks.insert(rank);
            }
            has_edges = false;
            graph->follow_edges(graph->get_handle(node_id, true), false, [&](const handle_t next_handle) {
                has_edges = true;
            });
            if (!has_edges) {
                temp_node_records.at(node_id-min_node_id).is_tip = true;
                temp_snarl_record.tippy_child_ranks.insert(rank);
            }
        }

        bool start_is_tip = start_index.first == TEMP_NODE 
                      ? temp_node_records.at(start_index.second-min_node_id).is_tip 
                      : temp_chain_records.at(start_index.second).is_tip;

        size_t start_rank = start_index.first == TEMP_NODE 
                ? temp_node_records.at(start_index.second-min_node_id).rank_in_parent
                : temp_chain_records.at(start_index.second).rank_in_parent;


        if (start_index.first == TEMP_NODE && start_index.second == temp_snarl_record.start_node_id) {
            start_rank = 0;
        } else if (start_index.first == TEMP_NODE && start_index.second == temp_snarl_record.end_node_id) {
            start_rank = 1;
        } //TODO:
          //else {
          //  assert(start_rank != 0 && start_rank != 1);
          //}

        if ( (temp_snarl_record.node_count < size_limit || size_limit == 0) && !start_is_tip &&
             !start_rank == 0 && ! start_rank == 1) {
            //If we don't care about internal distances, and we also are not at a boundary or tip
            continue;
        }

        //Start from either direction for all nodes, but only going in for start and end
        vector<bool> directions;
        if (start_index.first == TEMP_NODE && start_index.second == temp_snarl_record.start_node_id) {
            directions.emplace_back(temp_snarl_record.start_node_rev);
        } else if (start_index.first == TEMP_NODE && start_index.second == temp_snarl_record.end_node_id){
            directions.emplace_back(!temp_snarl_record.end_node_rev);
        } else {
            directions.emplace_back(true);
            directions.emplace_back(false);
        }
        for (bool start_rev : directions) {
            //Start a dijkstra traversal from start_index going in the direction indicated by start_rev
            //Record the distances to each node (child of the snarl) found

#ifdef debug_distance_indexing
            cerr << "  Starting from child " << structure_start_end_as_string(start_index)
                 << " going " << (start_rev ? "rev" : "fd") << endl;
#endif

            //Define a NetgraphNode as the value for the priority queue:
            // <distance, <<type of node, index into temp_node/chain_records>, direction>
            using NetgraphNode = pair<int64_t, pair<pair<temp_record_t, size_t>, bool>>; 
            auto cmp = [] (const NetgraphNode a, const NetgraphNode b) {
                return a.first > b.first;
            };
            std::priority_queue<NetgraphNode, vector<NetgraphNode>, decltype(cmp)> queue(cmp);
            unordered_set<pair<pair<temp_record_t, size_t>, bool>> seen_nodes;
            queue.push(make_pair(0, make_pair(start_index, start_rev)));

            while (!queue.empty()) {

                int64_t current_distance = queue.top().first;
                pair<temp_record_t, size_t> current_index = queue.top().second.first;
                bool current_rev = queue.top().second.second;
                seen_nodes.emplace(queue.top().second);
                queue.pop();

                //The handle that we need to follow to get the next reachable nodes
                //If the current node is a node, then its just the node. Otherwise, it's the 
                //opposite side of the child chain
                handle_t current_end_handle = current_index.first == TEMP_NODE ? 
                        graph->get_handle(current_index.second, current_rev) :
                        (current_rev ? graph->get_handle(temp_chain_records[current_index.second].start_node_id, 
                                                        !temp_chain_records[current_index.second].start_node_rev) 
                                  : graph->get_handle(temp_chain_records[current_index.second].end_node_id, 
                                                      temp_chain_records[current_index.second].end_node_rev));
#ifdef debug_distance_indexing
                        cerr << "    at child " << structure_start_end_as_string(current_index) << " going "
                             << (current_rev ? "rev" : "fd") << " at actual node " << graph->get_id(current_end_handle) 
                             << (graph->get_is_reverse(current_end_handle) ? "rev" : "fd") << endl;
#endif
                graph->follow_edges(current_end_handle, false, [&](const handle_t next_handle) {
                    //At each of the nodes reachable from the current one, fill in the distance from the start
                    //node to the next node (current_distance). If this handle isn't leaving the snarl,
                    //add the next nodes along with the distance to the end of the next node
                    auto& node_record = temp_node_records.at(graph->get_id(next_handle)-min_node_id);
//TODO: The snarl decomposition should find tips now
//                    if (node_record.node_id == 0) {
//#ifdef debug_distance_indexing
//                        cerr << "Adding a tip " <<  graph->get_id(next_handle) << endl;
//#endif
//                        //If we haven't seen this node before, it means that it was a tip
//                        node_record.node_id = graph->get_id(next_handle);
//                        node_record.node_length = graph->get_length(next_handle);
//                        node_record.rank_in_parent = temp_snarl_record.node_count+2;
//                        node_record.reversed_in_parent = false;
//                        node_record.parent = snarl_index; 
//                        node_record.is_tip = true;
//
//                        //also update the parent
//                        temp_snarl_record.node_count ++;
//                        temp_snarl_record.is_trivial = false;
//                        temp_snarl_record.children.emplace_back(TEMP_NODE, graph->get_id(next_handle));
//                        temp_snarl_record.tippy_child_ranks.insert(node_record.rank_in_parent);
//
//                        //TODO: Is it bad to change the list as we're walking through it?
//                        all_children.emplace_back(TEMP_NODE, graph->get_id(next_handle)); 
//                    }

                    //The index of the snarl's child that next_handle represents
                    pair<temp_record_t, size_t> next_index = get_ancestor_of_node(make_pair(TEMP_NODE, graph->get_id(next_handle))); 

                    bool next_is_tip = start_index.first == TEMP_NODE 
                              ? temp_node_records.at(start_index.second-min_node_id).is_tip 
                              : temp_chain_records.at(start_index.second).is_tip;

                    //The rank and orientation of next in the snarl
                    size_t next_rank = next_index.first == TEMP_NODE 
                            ? node_record.rank_in_parent
                            : temp_chain_records[next_index.second].rank_in_parent;
                    if (next_index.first == TEMP_NODE && next_index.second == temp_snarl_record.start_node_id) {
                        next_rank = 0;
                    } else if (next_index.first == TEMP_NODE && next_index.second == temp_snarl_record.end_node_id) {
                        next_rank = 1;
                    } //TODO: This won't be true of root snarls 
                      //else {
                      //  assert(next_rank != 0 && next_rank != 1);
                      //}
                    bool next_rev = next_index.first == TEMP_NODE || temp_chain_records[next_index.second].is_trivial 
                            ? graph->get_is_reverse(next_handle) 
                            : graph->get_id(next_handle) == temp_chain_records[next_index.second].end_node_id;

                    if (size_limit != 0 &&
                        (temp_snarl_record.node_count < size_limit ||
                         (start_rank == 0 || start_rank == 1 || next_rank == 0 || next_rank == 1))) {
                        //If we are looking at all distances or we are looking at tips or boundaries

                        //Set the distance
                        pair<size_t, bool> start = !temp_snarl_record.is_root_snarl && (start_rank == 0 || start_rank == 1) 
                            ? make_pair(start_rank, false) : make_pair(start_rank, !start_rev);
                        pair<size_t, bool> next = !temp_snarl_record.is_root_snarl && (next_rank == 0 || next_rank == 1) 
                            ? make_pair(next_rank, false) : make_pair(next_rank, next_rev);
                        if (!temp_snarl_record.distances.count(make_pair(start, next)) ) {

                            temp_snarl_record.distances[make_pair(start, next)] = current_distance;
#ifdef debug_distance_indexing
                            cerr << "           Adding distance between ranks " << start.first << " " << start.second << " and " << next.first << " " << next.second << ": " << current_distance << endl;
#endif
                        }
                    }


                    if (seen_nodes.count(make_pair(next_index, next_rev)) == 0 &&
                        graph->get_id(next_handle) != temp_snarl_record.start_node_id &&
                        graph->get_id(next_handle) != temp_snarl_record.end_node_id) {
                        //If this isn't leaving the snarl, then add the next node to the queue, 
                        //along with the distance to traverse it
                        int64_t next_node_len = next_index.first == TEMP_NODE ? graph->get_length(next_handle) :
                                        temp_chain_records[next_index.second].min_length;
                        queue.push(make_pair(current_distance + next_node_len, 
                                       make_pair(next_index, next_rev)));
                    }
                    if (next_index.first == TEMP_CHAIN) {
                        int64_t loop_distance = next_rev ? temp_chain_records[next_index.second].backward_loops.back() 
                                                         : temp_chain_records[next_index.second].forward_loops.front();
                        if (loop_distance != std::numeric_limits<int64_t>::max() &&
                            seen_nodes.count(make_pair(next_index, !next_rev)) == 0 &&
                            graph->get_id(next_handle) != temp_snarl_record.start_node_id &&
                            graph->get_id(next_handle) != temp_snarl_record.end_node_id) {
                            //If the next node can loop back on itself, then add the next node in the opposite direction
                            int64_t next_node_len = loop_distance + 2 * graph->get_length(next_handle);
                            queue.push(make_pair(current_distance + next_node_len, 
                                           make_pair(next_index, !next_rev)));
                        }
                    }
#ifdef debug_distance_indexing
                    cerr << "        reached child " << structure_start_end_as_string(next_index) << "going " 
                         << (next_rev ? "rev" : "fd") << " with distance " << current_distance << " for ranks " << start_rank << " " << next_rank << endl;
#endif
                });
            }
        }
    }
}

}
