//
//  snarl_distance_index.cpp
//
//  Unit tests for SnarlDistanceIndex and related functions
//
//TODO: Tips at the end of the top-level chain?

#include <stdio.h>
#include <iostream>
#include <sstream>
#include <set>
#include "vg/io/json2pb.h"
#include <vg/vg.pb.h>
#include "catch.hpp"
#include "random_graph.hpp"
#include "randomness.hpp"
#include "../snarl_distance_index.hpp"
#include "../integrated_snarl_finder.hpp"
#include "../genotypekit.hpp"
#include "../traversal_finder.hpp"
#include <vg/io/protobuf_emitter.hpp>
#include <vg/io/vpkg.hpp>

//#define debug

namespace vg {
    namespace unittest {
    
        TEST_CASE( "Snarl decomposition can allow traversal of a simple net graph",
                  "[snarl_distance]" ) {
        
        
            // This graph will have a snarl from 1 to 8, a snarl from 2 to 7,
            // and a snarl from 3 to 5, all nested in each other.
            VG graph;
                
            Node* n1 = graph.create_node("GCA");
            Node* n2 = graph.create_node("T");
            Node* n3 = graph.create_node("G");
            Node* n4 = graph.create_node("CTGA");
            Node* n5 = graph.create_node("GCA");
            Node* n6 = graph.create_node("T");
            Node* n7 = graph.create_node("G");
            Node* n8 = graph.create_node("CTGA");
            
            Edge* e1 = graph.create_edge(n1, n2);
            Edge* e2 = graph.create_edge(n1, n8);
            Edge* e3 = graph.create_edge(n2, n3);
            Edge* e4 = graph.create_edge(n2, n6);
            Edge* e5 = graph.create_edge(n3, n4);
            Edge* e6 = graph.create_edge(n3, n5);
            Edge* e7 = graph.create_edge(n4, n5);
            Edge* e8 = graph.create_edge(n5, n7);
            Edge* e9 = graph.create_edge(n6, n7);
            Edge* e10 = graph.create_edge(n7, n8);
            
            //get the snarls
            IntegratedSnarlFinder snarl_finder(graph); 
            SnarlDistanceIndex distance_index(&graph, &snarl_finder);

            //Handle for first node facing in
            net_handle_t n1_fd = distance_index.get_net(graph.get_handle(1, false), &graph); 

            //Make sure that we really got the right handle
            REQUIRE(distance_index.get_handle(n1_fd, &graph) == graph.get_handle(1, false));

            //Handle for top level chain of just one node
            net_handle_t chain1 = distance_index.get_parent(n1_fd);
            REQUIRE(distance_index.is_chain(chain1));
            size_t child_i = 0;
            net_handle_t top_snarl;
            vector<net_handle_t> child_handles;//This should be start node, snarl, end node
            distance_index.for_each_child(chain1, [&](const net_handle_t& child) {
                if (child_i == 0) {
                    REQUIRE(distance_index.is_node(child));
                    REQUIRE(graph.get_id(distance_index.get_handle(child, &graph)) == 1);
                } else if (child_i == 1) {
                    REQUIRE(distance_index.is_snarl(child));
                    child_handles.emplace_back(child);
                    top_snarl = child;
                } else if (child_i == 2) {
                    REQUIRE(distance_index.is_node(child));
                    REQUIRE(graph.get_id(distance_index.get_handle(child, &graph)) == 8);
                } else {
                    //The chain should only contain two nodes and a snarl
                    REQUIRE(false);
                }
                REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                            == distance_index.canonical(chain1));
                child_i++;
                return true;
            });

            SECTION( "The top-level NetGraph has 3 nodes" ) {

                //The snarl 2,7 has one child (not counting the boundary nodes)
                size_t node_count = 0;
                REQUIRE(distance_index.is_snarl(top_snarl));
                distance_index.for_each_child(top_snarl, [&](const net_handle_t& handle) {
                    node_count++;
                    REQUIRE( distance_index.canonical(distance_index.get_parent(handle))
                            == distance_index.canonical(top_snarl));
                    return true;
                });
                
                REQUIRE(node_count == 1);
                
                //SECTION( "The nodes are numbered 1, 2, and 8" ) {
                //    REQUIRE(nodes.count(net_graph.get_handle(1, false)) == 1);
                //    REQUIRE(nodes.count(net_graph.get_handle(2, false)) == 1);
                //    REQUIRE(nodes.count(net_graph.get_handle(8, false)) == 1);
                //}
            }
            
            SECTION( "The top-level NetGraph has 3 edges" ) {
                vector<net_handle_t> handles;
                distance_index.for_each_child(top_snarl, [&](const net_handle_t& handle) {
                    handles.emplace_back(handle);
                    REQUIRE( distance_index.canonical(distance_index.get_parent(handle))
                            == distance_index.canonical(top_snarl));
                    return true;
                });
                handles.emplace_back(distance_index.get_bound(top_snarl, false, true));
                handles.emplace_back(distance_index.get_bound(top_snarl, true, true));
                unordered_set<pair<net_handle_t, net_handle_t>> edges;
                for (net_handle_t& handle : handles) {
                    // Go through the nodes we should have manually.
                
                    // Save all the edges off of each node
                    distance_index.follow_net_edges(handle, &graph, false, [&](const net_handle_t& other) {
                        edges.insert(make_pair(handle, other));
                        return true;
                    });
                    distance_index.follow_net_edges(handle, &graph, true, [&](const net_handle_t& other) {
                        edges.insert(make_pair(other, handle));
                        return true;
                    });
                }
                
                //Double counting I think
                REQUIRE(edges.size() == 6);
            }
        
        }
  
        TEST_CASE( "Snarl decomposition can handle start-start connectivity",
                  "[snarl_distance]" ) {
        
        
            // This graph will have a snarl from 1 to 8, a snarl from 2 to 7,
            // and a snarl from 3 to 5, all nested in each other.
            VG graph;
                
            Node* n1 = graph.create_node("GCA");
            Node* n2 = graph.create_node("T");
            Node* n3 = graph.create_node("G");
            Node* n4 = graph.create_node("CTGA");
            Node* n5 = graph.create_node("GCA");
            Node* n6 = graph.create_node("T");
            Node* n7 = graph.create_node("G");
            Node* n8 = graph.create_node("CTGA");
            
            Edge* e1 = graph.create_edge(n1, n2);
            Edge* e2 = graph.create_edge(n1, n8);
            Edge* e3 = graph.create_edge(n2, n3);
            Edge* e4 = graph.create_edge(n2, n6);
            Edge* e5 = graph.create_edge(n3, n4);
            // Add an extra reversing edge so you can leave out the start
            Edge* eRev = graph.create_edge(n3, n3, false, true);
            Edge* e6 = graph.create_edge(n3, n5);
            Edge* e7 = graph.create_edge(n4, n5);
            Edge* e8 = graph.create_edge(n5, n7);
            Edge* e9 = graph.create_edge(n6, n7);
            Edge* e10 = graph.create_edge(n7, n8);
            
            IntegratedSnarlFinder snarl_finder(graph); 
            SnarlDistanceIndex distance_index(&graph, &snarl_finder);
            
            SECTION( "A connectivity-ignoring net graph ignores connectivity" ) {
            
                //Handle for first node facing in
                net_handle_t n1_fd = distance_index.get_net(graph.get_handle(1, false), &graph); 
                //Handle for top level chain of just one node
                net_handle_t chain1 = distance_index.get_parent(n1_fd);
                REQUIRE(distance_index.is_chain(chain1));
                size_t child_i = 0;
                vector<net_handle_t> child_handles;//This should be start node, snarl, end node
                distance_index.for_each_child(chain1, [&](const net_handle_t& child) {
                    if (child_i == 0) {
                        REQUIRE(distance_index.is_node(child));
                        REQUIRE(graph.get_id(distance_index.get_handle(child, &graph)) == 1);
                    } else if (child_i == 1) {
                        REQUIRE(distance_index.is_snarl(child));
                        child_handles.emplace_back(child);
                    } else if (child_i == 2) {
                        REQUIRE(distance_index.is_node(child));
                        REQUIRE(graph.get_id(distance_index.get_handle(child, &graph)) == 8);
                    } else {
                        //The chain should only contain two nodes and a snarl
                        REQUIRE(false);
                    }
                    REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                            == distance_index.canonical(chain1));
                    child_i++;
                    return true;
                });

                //Get a net_handle for the top-level snarl
                net_handle_t& top_snarl = child_handles[0];
                
                SECTION( "The top-level NetGraph has 3 nodes" ) {

                    size_t node_count = 0;
                    distance_index.for_each_child(top_snarl, [&](const net_handle_t& handle) {
                        node_count++;
                        REQUIRE( distance_index.canonical(distance_index.get_parent(handle))
                            == distance_index.canonical(top_snarl));
                        return true;
                    });
                    
                    REQUIRE(node_count == 1);

                }
                
                SECTION( "The top-level NetGraph has 3 edges" ) {

                    vector<net_handle_t> handles;
                    distance_index.for_each_child(top_snarl, [&](const net_handle_t& handle) {
                        handles.emplace_back(handle);
                        REQUIRE( distance_index.canonical(distance_index.get_parent(handle))
                            == distance_index.canonical(top_snarl));
                        return true;
                    });
                    handles.emplace_back(distance_index.get_bound(top_snarl, false, true));
                    handles.emplace_back(distance_index.get_bound(top_snarl, true, true));

                    unordered_set<pair<net_handle_t, net_handle_t>> edges;
                    for (net_handle_t& handle : handles) {
                        // Go through the nodes we should have manually.
                    
                        // Save all the edges off of each node
                        distance_index.follow_net_edges(handle, &graph, false, [&](const net_handle_t& other) {
                            edges.insert(make_pair(handle, other));
                            return true;
                        });
                        distance_index.follow_net_edges(handle, &graph, true, [&](const net_handle_t& other) {
                            edges.insert(make_pair(other, handle));
                            return true;
                        });
                    }
                    
                    //Double counting I think
                    REQUIRE(edges.size() == 6);
                }
            }
            
            /*
             * TODO: SHould we allow a netgraph that respects internal connectivity?
            SECTION( "A connectivity-respecting net graph allows more traversals" ) {
            
                // Make a connectivity-respecting graph
                NetGraph net_graph(top_snarl.start(), top_snarl.end(), top_chains, top_unary_snarls, &graph, true);
                
                SECTION( "The top-level NetGraph has 3 nodes" ) {
                    unordered_set<handle_t> nodes;
                    size_t node_count = 0;
                    net_graph.for_each_handle([&](const handle_t& handle) {
                        nodes.insert(handle);
                        node_count++;
                    });
                    REQUIRE(nodes.size() == node_count);
                    
                    REQUIRE(nodes.size() == 3);
                    
                    SECTION( "The nodes are numbered 1, 2, and 8" ) {
                        REQUIRE(nodes.count(net_graph.get_handle(1, false)) == 1);
                        REQUIRE(nodes.count(net_graph.get_handle(2, false)) == 1);
                        REQUIRE(nodes.count(net_graph.get_handle(8, false)) == 1);
                    }
                }
                
                SECTION( "The top-level NetGraph has 4 edges" ) {
                    unordered_set<pair<handle_t, handle_t>> edges;
                    for (auto& id : {1, 2, 8}) {
                        // Go through the nodes we should have manually.
                        handle_t handle = net_graph.get_handle(id, false);
                    
                        // Save all the edges off of each node
                        net_graph.follow_edges(handle, false, [&](const handle_t& other) {
                            edges.insert(net_graph.edge_handle(handle, other));
                        });
                        net_graph.follow_edges(handle, true, [&](const handle_t& other) {
                            edges.insert(net_graph.edge_handle(other, handle));
                        });
                    }
                    
                    REQUIRE(edges.size() == 4);
                    
#ifdef debug
                    for (auto& edge : edges) {
                        cerr << net_graph.get_id(edge.first) << " " << net_graph.get_is_reverse(edge.first) << " -> "
                            << net_graph.get_id(edge.second) << " " << net_graph.get_is_reverse(edge.second) << endl;
                    }
#endif
                    
                    SECTION( "One edge is from node 1 to node 2 in reverse" ) {
                        REQUIRE(edges.count(net_graph.edge_handle(net_graph.get_handle(1, false),
                            net_graph.get_handle(2, true))) == 1);
                    }
                }
            
            }
            */
        
        }

        
        TEST_CASE( "Snarl decomposition can handle end-end connectivity",
                  "[snarl_distance]" ) {
        
        
            // This graph will have a snarl from 1 to 8, a snarl from 2 to 7,
            // and a snarl from 3 to 5, all nested in each other.
            VG graph;
                
            Node* n1 = graph.create_node("GCA");
            Node* n2 = graph.create_node("T");
            Node* n3 = graph.create_node("G");
            Node* n4 = graph.create_node("CTGA");
            Node* n5 = graph.create_node("GCA");
            Node* n6 = graph.create_node("T");
            Node* n7 = graph.create_node("G");
            Node* n8 = graph.create_node("CTGA");
            
            Edge* e1 = graph.create_edge(n1, n2);
            Edge* e2 = graph.create_edge(n1, n8);
            Edge* e3 = graph.create_edge(n2, n3);
            Edge* e4 = graph.create_edge(n2, n6);
            Edge* e5 = graph.create_edge(n3, n4);
            // Add an extra reversing edge so you can leave out the end
            Edge* eRev = graph.create_edge(n4, n4, true, false);
            Edge* e6 = graph.create_edge(n3, n5);
            Edge* e7 = graph.create_edge(n4, n5);
            Edge* e8 = graph.create_edge(n5, n7);
            Edge* e9 = graph.create_edge(n6, n7);
            Edge* e10 = graph.create_edge(n7, n8);
            
            IntegratedSnarlFinder snarl_finder(graph); 
            SnarlDistanceIndex distance_index(&graph, &snarl_finder);
            
            
            SECTION( "A connectivity-ignoring net graph ignores connectivity" ) {
            
                //Handle for first node facing in
                net_handle_t n1_fd = distance_index.get_net(graph.get_handle(1, false), &graph); 
                //Handle for top level chain of just one node
                net_handle_t chain1 = distance_index.get_parent(n1_fd);
                REQUIRE(distance_index.is_chain(chain1));
                size_t child_i = 0;
                vector<net_handle_t> child_handles;//This should be start node, snarl, end node
                distance_index.for_each_child(chain1, [&](const net_handle_t& child) {
                    if (child_i == 0) {
                        REQUIRE(distance_index.is_node(child));
                        REQUIRE(graph.get_id(distance_index.get_handle(child, &graph)) == 1);
                    } else if (child_i == 1) {
                        REQUIRE(distance_index.is_snarl(child));
                        child_handles.emplace_back(child);
                    } else if (child_i == 2) {
                        REQUIRE(distance_index.is_node(child));
                        REQUIRE(graph.get_id(distance_index.get_handle(child, &graph)) == 8);
                    } else {
                        //The chain should only contain two nodes and a snarl
                        REQUIRE(false);
                    }
                    REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                            == distance_index.canonical(chain1));
                    child_i++;
                    return true;
                });

                //Get a net_handle for the top-level snarl
                net_handle_t& top_snarl = child_handles[0];
                
                SECTION( "The top-level NetGraph has 3 nodes" ) {

                    size_t node_count = 0;
                    distance_index.for_each_child(top_snarl, [&](const net_handle_t& handle) {
                        node_count++;
                        REQUIRE( distance_index.canonical(distance_index.get_parent(handle))
                            == distance_index.canonical(top_snarl));
                        return true;
                    });
                    
                    REQUIRE(node_count == 1);

                }
                
                SECTION( "The top-level NetGraph has 3 edges" ) {
                    vector<net_handle_t> handles;
                    distance_index.for_each_child(top_snarl, [&](const net_handle_t& handle) {
                        handles.emplace_back(handle);
                        REQUIRE( distance_index.canonical(distance_index.get_parent(handle))
                            == distance_index.canonical(top_snarl));
                        return true;
                    });
                    handles.emplace_back(distance_index.get_bound(top_snarl, false, true));
                    handles.emplace_back(distance_index.get_bound(top_snarl, true, true));

                    unordered_set<pair<net_handle_t, net_handle_t>> edges;
                    for (net_handle_t& handle : handles) {
                        // Go through the nodes we should have manually.
                    
                        // Save all the edges off of each node
                        distance_index.follow_net_edges(handle, &graph, false, [&](const net_handle_t& other) {
                            edges.insert(make_pair(handle, other));
                            return true;
                        });
                        distance_index.follow_net_edges(handle, &graph, true, [&](const net_handle_t& other) {
                            edges.insert(make_pair(other, handle));
                            return true;
                        });
                    }
                    
                    //Double counting I think
                    REQUIRE(edges.size() == 6);
                }
            }
            
            /*
            SECTION( "A connectivity-respecting net graph allows more traversals" ) {
            
                // Make a connectivity-respecting graph
                NetGraph net_graph(top_snarl.start(), top_snarl.end(), top_chains, top_unary_snarls, &graph, true);
                
                SECTION( "The top-level NetGraph has 3 nodes" ) {
                    unordered_set<handle_t> nodes;
                    size_t node_count = 0;
                    net_graph.for_each_handle([&](const handle_t& handle) {
                        nodes.insert(handle);
                        node_count++;
                    });
                    REQUIRE(nodes.size() == node_count);
                    
                    REQUIRE(nodes.size() == 3);
                    
                    SECTION( "The nodes are numbered 1, 2, and 8" ) {
                        REQUIRE(nodes.count(net_graph.get_handle(1, false)) == 1);
                        REQUIRE(nodes.count(net_graph.get_handle(2, false)) == 1);
                        REQUIRE(nodes.count(net_graph.get_handle(8, false)) == 1);
                    }
                }
                
                SECTION( "The top-level NetGraph has 4 edges" ) {
                    unordered_set<pair<handle_t, handle_t>> edges;
                    for (auto& id : {1, 2, 8}) {
                        // Go through the nodes we should have manually.
                        handle_t handle = net_graph.get_handle(id, false);
                    
                        // Save all the edges off of each node
                        net_graph.follow_edges(handle, false, [&](const handle_t& other) {
                            edges.insert(net_graph.edge_handle(handle, other));
                        });
                        net_graph.follow_edges(handle, true, [&](const handle_t& other) {
                            edges.insert(net_graph.edge_handle(other, handle));
                        });
                    }
                    
                    REQUIRE(edges.size() == 4);
             
#ifdef debug
                    for (auto& edge : edges) {
                        cerr << net_graph.get_id(edge.first) << " " << net_graph.get_is_reverse(edge.first) << " -> "
                            << net_graph.get_id(edge.second) << " " << net_graph.get_is_reverse(edge.second) << endl;
                    }
#endif
                    
                    SECTION( "One edge is from node 2 in reverse to node 8" ) {
                        REQUIRE(edges.count(net_graph.edge_handle(net_graph.get_handle(2, true),
                            net_graph.get_handle(8, false))) == 1);
                    }
                }
            
            }
            */
        
        } 
        
        /*TOOD: This had a weird snarl decomposition
        TEST_CASE( "Snarl decomposition can handle disconnected chain bounds",
                  "[snarl_distance]" ) {
        
        
            // We will have a snarl 1 to 2, and within it a chain of 3 to 4 and
            // 4 to 5 (both trivial)
            VG graph;
                
            Node* n1 = graph.create_node("GCA");
            Node* n2 = graph.create_node("T");
            Node* n3 = graph.create_node("G");
            Node* n4 = graph.create_node("CTGA");
            Node* n5 = graph.create_node("GCA");
            
            Edge* e1 = graph.create_edge(n1, n2);
            Edge* e2 = graph.create_edge(n3, n4);
            Edge* e3 = graph.create_edge(n4, n5);
            
            for (bool do_start : {false, true}) {
            
                // Attach only start or end of chain to parent snarl
                Edge* e4 = do_start ? graph.create_edge(n1, n3) : graph.create_edge(n5, n2);
                
                //get the snarls
                IntegratedSnarlFinder snarl_finder(graph); 
                SnarlDistanceIndex distance_index(&graph, &snarl_finder);
                
                //Handle for first node facing in
                net_handle_t n1_fd = distance_index.get_net(graph.get_handle(1, false), &graph); 
                //Handle for top level chain of just one node
                net_handle_t chain1 = distance_index.get_parent(n1_fd);
                REQUIRE(distance_index.is_chain(chain1));
                size_t child_i = 0;
                vector<net_handle_t> child_handles;//This should be start node, snarl, end node
                distance_index.for_each_child(chain1, [&](const net_handle_t& child) {
                    if (child_i == 0) {
                        REQUIRE(distance_index.is_node(child));
                        REQUIRE(graph.get_id(distance_index.get_handle(child, &graph)) == 1);
                    } else if (child_i == 1) {
                        REQUIRE(distance_index.is_snarl(child));
                        child_handles.emplace_back(child);
                    } else if (child_i == 2) {
                        REQUIRE(distance_index.is_node(child));
                        REQUIRE(graph.get_id(distance_index.get_handle(child, &graph)) == 8);
                    } else {
                        //The chain should only contain two nodes and a snarl
                        REQUIRE(false);
                    }
                    child_i++;
                });
                //Get a net_handle for the top-level snarl
                net_handle_t& top_snarl = child_handles[0];
                
                SECTION( "The top-level NetGraph has 1 node" ) {

                    size_t node_count = 0;
                    distance_index.for_each_child(child_handles[1], [&](const net_handle_t& handle) {
                        node_count++;
                    });
                    
                    REQUIRE(node_count == 1);

                }
                 
                SECTION( "The top-level NetGraph has 2 edges" ) {
                    vector<net_handle_t> handles;
                    distance_index.for_each_child(top_snarl, [&](const net_handle_t& handle) {
                        handles.emplace_back(handle);
                    });
                    handles.emplace_back(distance_index.get_bound(top_snarl, false, true));
                    handles.emplace_back(distance_index.get_bound(top_snarl, true, true));


                    unordered_set<pair<net_handle_t, net_handle_t>> edges;
                    for (net_handle_t& handle : handles) {
                        // Go through the nodes we should have manually.
                    
                        // Save all the edges off of each node
                        distance_index.follow_net_edges(handle, &graph, false, [&](const net_handle_t& other) {
                            edges.insert(make_pair(handle, other));
                            return true;
                        });
                        distance_index.follow_net_edges(handle, &graph, true, [&](const net_handle_t& other) {
                            edges.insert(make_pair(other, handle));
                            return true;
                        });
                    }
                    
                    //Double counting I think
                    REQUIRE(edges.size() == 4);
                }
            }
        }
        
        TEST_CASE( "Snarl decomposition can handle no connectivity",
                  "[snarl_distance]" ) {
        
        
            // This graph will have a snarl from 1 to 8, a snarl from 2 to 7,
            // and a snarl from 3 to 5, all nested in each other.
            VG graph;
                
            Node* n1 = graph.create_node("GCA");
            Node* n2 = graph.create_node("T");
            Node* n3 = graph.create_node("G");
            Node* n4 = graph.create_node("CTGA");
            Node* n5 = graph.create_node("GCA");
            Node* n6 = graph.create_node("T");
            Node* n7 = graph.create_node("G");
            Node* n8 = graph.create_node("CTGA");
            
            Edge* e1 = graph.create_edge(n1, n2);
            Edge* e2 = graph.create_edge(n1, n8);
            Edge* e3 = graph.create_edge(n2, n3);
            Edge* e4 = graph.create_edge(n2, n6);
            Edge* e5 = graph.create_edge(n3, n4);
            Edge* e6 = graph.create_edge(n3, n5);
            Edge* e7 = graph.create_edge(n4, n5);
            // Make node 7's start connect to what node 2's end connects to, so the 2 to 7 snarl has two children.
            Edge* e8 = graph.create_edge(n7, n3, true, false);
            Edge* e9 = graph.create_edge(n7, n6, true, false);
            Edge* e10 = graph.create_edge(n7, n8);
            
            //get the snarls
            IntegratedSnarlFinder snarl_finder(graph); 
            SnarlDistanceIndex distance_index(&graph, &snarl_finder);
            
            SECTION( "A connectivity-ignoring net graph ignores connectivity" ) {
            
                //Handle for first node facing in
                net_handle_t n1_fd = distance_index.get_net(graph.get_handle(1, false), &graph); 
                //Handle for top level chain of just one node
                net_handle_t chain1 = distance_index.get_parent(n1_fd);
                REQUIRE(distance_index.is_chain(chain1));
                size_t child_i = 0;
                vector<net_handle_t> child_handles;//This should be start node, snarl, end node
                distance_index.for_each_child(chain1, [&](const net_handle_t& child) {
                    if (child_i == 0) {
                        REQUIRE(distance_index.is_node(child));
                        REQUIRE(graph.get_id(distance_index.get_handle(child, &graph)) == 1);
                    } else if (child_i == 1) {
                        REQUIRE(distance_index.is_snarl(child));
                        child_handles.emplace_back(child);
                    } else if (child_i == 2) {
                        REQUIRE(distance_index.is_node(child));
                        REQUIRE(graph.get_id(distance_index.get_handle(child, &graph)) == 8);
                    } else {
                        //The chain should only contain two nodes and a snarl
                        REQUIRE(false);
                    }
                    child_i++;
                });

                //Get a net_handle for the top-level snarl
                net_handle_t& top_snarl = child_handles[0];
                
                SECTION( "The top-level NetGraph has 3 nodes" ) {

                    size_t node_count = 0;
                    distance_index.for_each_child(top_snarl, [&](const net_handle_t& handle) {
                        node_count++;
                    });
                    
                    REQUIRE(node_count == 1);

                }
                
                SECTION( "The top-level NetGraph has 3 edges" ) {
                    vector<net_handle_t> handles;
                    distance_index.for_each_child(top_snarl, [&](const net_handle_t& handle) {
                        handles.emplace_back(handle);
                    });
                    handles.emplace_back(distance_index.get_bound(top_snarl, false, true));
                    handles.emplace_back(distance_index.get_bound(top_snarl, true, true));


                    unordered_set<pair<net_handle_t, net_handle_t>> edges;
                    for (net_handle_t& handle : handles) {
                        // Go through the nodes we should have manually.
                    
                        // Save all the edges off of each node
                        distance_index.follow_net_edges(handle, &graph, false, [&](const net_handle_t& other) {
                            edges.insert(make_pair(handle, other));
                            return true;
                        });
                        distance_index.follow_net_edges(handle, &graph, true, [&](const net_handle_t& other) {
                            edges.insert(make_pair(other, handle));
                            return true;
                        });
                    }
                    
                    //Double counting I think
                    REQUIRE(edges.size() == 6);
                }
            }
            
            SECTION( "A connectivity-respecting net graph doesn't allow through-traversal" ) {
            
                // Make a connectivity-respecting graph
                NetGraph net_graph(top_snarl.start(), top_snarl.end(), top_chains, top_unary_snarls, &graph, true);
                
                SECTION( "The top-level NetGraph has 3 nodes" ) {
                    unordered_set<handle_t> nodes;
                    size_t node_count = 0;
                    net_graph.for_each_handle([&](const handle_t& handle) {
                        nodes.insert(handle);
                        node_count++;
                    });
                    REQUIRE(nodes.size() == node_count);
                    
                    REQUIRE(nodes.size() == 3);
                    
                    SECTION( "The nodes are numbered 1, 2, and 8" ) {
                        REQUIRE(nodes.count(net_graph.get_handle(1, false)) == 1);
                        REQUIRE(nodes.count(net_graph.get_handle(2, false)) == 1);
                        REQUIRE(nodes.count(net_graph.get_handle(8, false)) == 1);
                    }
                }
                
                SECTION( "The top-level NetGraph has 3 edges looking around node 2" ) {
                    unordered_set<pair<handle_t, handle_t>> edges;
                    for (auto& id : {1, 8}) {
                        // Go through the nodes we should have manually.
                        handle_t handle = net_graph.get_handle(id, false);
                    
                        // Save all the edges off of each node
                        net_graph.follow_edges(handle, false, [&](const handle_t& other) {
                            edges.insert(net_graph.edge_handle(handle, other));
                        });
                        net_graph.follow_edges(handle, true, [&](const handle_t& other) {
                            edges.insert(net_graph.edge_handle(other, handle));
                        });
                    }
                    
                    REQUIRE(edges.size() == 3);
             
#ifdef debug
                    for (auto& edge : edges) {
                        cerr << net_graph.get_id(edge.first) << " " << net_graph.get_is_reverse(edge.first) << " -> "
                            << net_graph.get_id(edge.second) << " " << net_graph.get_is_reverse(edge.second) << endl;
                    }
#endif
                }
                
                SECTION( "The top-level NetGraph has 0 edges looking at node 2" ) {
                    unordered_set<pair<handle_t, handle_t>> edges;
                    for (auto& id : {2}) {
                        // Go through the nodes we should have manually.
                        handle_t handle = net_graph.get_handle(id, false);
                    
                        // Save all the edges off of each node
                        net_graph.follow_edges(handle, false, [&](const handle_t& other) {
                            edges.insert(net_graph.edge_handle(handle, other));
                        });
                        net_graph.follow_edges(handle, true, [&](const handle_t& other) {
                            edges.insert(net_graph.edge_handle(other, handle));
                        });
                    }
                    
                    REQUIRE(edges.size() == 0);
             
#ifdef debug
                    for (auto& edge : edges) {
                        cerr << net_graph.get_id(edge.first) << " " << net_graph.get_is_reverse(edge.first) << " -> "
                            << net_graph.get_id(edge.second) << " " << net_graph.get_is_reverse(edge.second) << endl;
                    }
#endif
                }
            
            }
        
        }
        */
        
        /*  
        TEST_CASE( "NetGraph finds all edges correctly when traversing in all directions",
                  "[snarl_distance]" ) {
        
        
            // This graph will have a snarl from 1 to 8, a snarl from 2 to 7,
            // and a snarl from 3 to 5, all nested in each other.
            VG graph;
                
            Node* n1 = graph.create_node("GCA");
            Node* n2 = graph.create_node("T");
            Node* n3 = graph.create_node("G");
            Node* n4 = graph.create_node("CTGA");
            Node* n5 = graph.create_node("GCA");
            Node* n6 = graph.create_node("T");
            Node* n7 = graph.create_node("G");
            Node* n8 = graph.create_node("CTGA");
            
            Edge* e1 = graph.create_edge(n1, n2);
            Edge* e2 = graph.create_edge(n1, n8);
            Edge* e3 = graph.create_edge(n2, n3);
            Edge* e4 = graph.create_edge(n2, n6);
            Edge* e5 = graph.create_edge(n3, n4);
            // Add an extra reversing edge so you can leave out the start
            Edge* eRev = graph.create_edge(n3, n3, false, true);
            // Add an extra reversing edge so you can leave out the end
            Edge* eRev2 = graph.create_edge(n5, n5, false, true);
            Edge* e6 = graph.create_edge(n3, n5);
            Edge* e7 = graph.create_edge(n4, n5);
            Edge* e8 = graph.create_edge(n5, n7);
            Edge* e9 = graph.create_edge(n6, n7);
            Edge* e10 = graph.create_edge(n7, n8);
            
            // Define the snarls for the top level
            Snarl top_snarl;
            top_snarl.mutable_start()->set_node_id(n1->id());
            top_snarl.mutable_end()->set_node_id(n8->id());
            
            // We have a chain with a snarl in it
            vector<vector<pair<Snarl, bool>>> top_chains;
            top_chains.emplace_back();
            auto& top_chain1 = top_chains.back();
            top_chain1.emplace_back();
            auto& nested_snarl1 = top_chain1.back().first;
            
            // And that snarl has these characteristics
            nested_snarl1.mutable_start()->set_node_id(n2->id());
            nested_snarl1.mutable_end()->set_node_id(n7->id());
            nested_snarl1.set_type(UNCLASSIFIED);
            *nested_snarl1.mutable_parent() = top_snarl;
            nested_snarl1.set_directed_acyclic_net_graph(true);
            // Because we can reverse inside a child, we are start-start and end-end reachable
            nested_snarl1.set_start_self_reachable(true);
            nested_snarl1.set_end_self_reachable(true);
            nested_snarl1.set_start_end_reachable(true);
            
            // We have an empty vector of top-level unary snarls.
            // TODO: should we have any?
            vector<Snarl> top_unary_snarls;
            
            // Make a connectivity-respecting graph
            NetGraph net_graph(top_snarl.start(), top_snarl.end(), top_chains, top_unary_snarls, &graph, true);
        
            // Make a handle to the nested child snarl
            handle_t nested_fwd = net_graph.get_handle(n2->id(), false);
            
            // ANd reverse it
            auto nested_rev = net_graph.flip(nested_fwd);
            
            SECTION( "Forward handle sees 1 fwd and 8 rev on its left" ) {
                unordered_set<handle_t> seen;
                net_graph.follow_edges(nested_fwd, true, [&](const handle_t& other) {
                    seen.insert(other);
                });
                
                REQUIRE(seen.size() == 2);
                REQUIRE(seen.count(net_graph.get_handle(n1->id(), false)) == 1);
                REQUIRE(seen.count(net_graph.get_handle(n8->id(), true)) == 1);
            }
            
            SECTION( "Forward handle sees 1 rev and 8 fwd on its right" ) {
                unordered_set<handle_t> seen;
                net_graph.follow_edges(nested_fwd, false, [&](const handle_t& other) {
                    seen.insert(other);
                });
                
                REQUIRE(seen.size() == 2);
                REQUIRE(seen.count(net_graph.get_handle(n1->id(), true)) == 1);
                REQUIRE(seen.count(net_graph.get_handle(n8->id(), false)) == 1);
            }
            
            SECTION( "Reverse handle sees 1 fwd and 8 rev on its left" ) {
                unordered_set<handle_t> seen;
                net_graph.follow_edges(nested_rev, true, [&](const handle_t& other) {
                    seen.insert(other);
                });
                
                REQUIRE(seen.size() == 2);
                REQUIRE(seen.count(net_graph.get_handle(n1->id(), false)) == 1);
                REQUIRE(seen.count(net_graph.get_handle(n8->id(), true)) == 1);
            }
            
            SECTION( "Reverse handle sees 1 rev and 8 fwd on its right" ) {
                unordered_set<handle_t> seen;
                net_graph.follow_edges(nested_rev, false, [&](const handle_t& other) {
                    seen.insert(other);
                });
                
                REQUIRE(seen.size() == 2);
                REQUIRE(seen.count(net_graph.get_handle(n1->id(), true)) == 1);
                REQUIRE(seen.count(net_graph.get_handle(n8->id(), false)) == 1);
            }
        
        }
        
        TEST_CASE( "NetGraph can handle nested unary snarls",
                  "[snarl_distance]" ) {
        
        
            // This graph will have a snarl from 1 to 8, a nested unary snarl on 2, and a nested unary snarl on 4.
            VG graph;
                
            Node* n1 = graph.create_node("GCA");
            Node* n2 = graph.create_node("T");
            Node* n3 = graph.create_node("G");
            Node* n4 = graph.create_node("CTGA");
            Node* n5 = graph.create_node("GCA");
            Node* n6 = graph.create_node("T");
            Node* n7 = graph.create_node("G");
            Node* n8 = graph.create_node("CTGA");
            
            Edge* e1 = graph.create_edge(n1, n2);
            Edge* e2 = graph.create_edge(n1, n4);
            Edge* e3 = graph.create_edge(n1, n8);
            Edge* e4 = graph.create_edge(n2, n3);
            Edge* e5 = graph.create_edge(n4, n5);
            Edge* e6 = graph.create_edge(n4, n6);
            Edge* e7 = graph.create_edge(n5, n7);
            Edge* e8 = graph.create_edge(n6, n7);
            Edge* e9 = graph.create_edge(n7, n7, false, true);
            Edge* e10 = graph.create_edge(n8, n4, true, false);
            Edge* e11 = graph.create_edge(n8, n2, true, false);
            
            // Define the snarls for the top level
            Snarl top_snarl;
            top_snarl.mutable_start()->set_node_id(n1->id());
            top_snarl.mutable_end()->set_node_id(n8->id());
            
            // We have no chains
            vector<vector<pair<Snarl, bool>>> top_chains;
            
            // We havetwo child unary snarls.
            vector<Snarl> top_unary_snarls;

            top_unary_snarls.emplace_back();
            auto& nested_snarl1 = top_unary_snarls.back(); 
            
            // The first one is a tip of nodes 2 and 3, based at node 2
            nested_snarl1.mutable_start()->set_node_id(n2->id());
            nested_snarl1.mutable_end()->set_node_id(n2->id());
            nested_snarl1.mutable_end()->set_backward(true);
            nested_snarl1.set_type(UNARY);
            *nested_snarl1.mutable_parent() = top_snarl;
            nested_snarl1.set_directed_acyclic_net_graph(true);
            // We aren't actually traversable at all
            nested_snarl1.set_start_self_reachable(false);
            nested_snarl1.set_end_self_reachable(false);
            nested_snarl1.set_start_end_reachable(false);
            
            top_unary_snarls.emplace_back();
            auto& nested_snarl2 = top_unary_snarls.back(); 
            
            // The second one is a big assemblage rooted at 4 with a child snarl and a reversing edge in it.
            nested_snarl2.mutable_start()->set_node_id(n4->id());
            nested_snarl2.mutable_end()->set_node_id(n4->id());
            nested_snarl2.mutable_end()->set_backward(true);
            nested_snarl2.set_type(UNARY);
            *nested_snarl2.mutable_parent() = top_snarl;
            // The reversal is actually in a child unary snarl.
            nested_snarl2.set_directed_acyclic_net_graph(true);
            // We are traversable in all directions
            nested_snarl2.set_start_self_reachable(true);
            nested_snarl2.set_end_self_reachable(true);
            nested_snarl2.set_start_end_reachable(true);
            
            SECTION( "A connectivity-ignoring net graph ignores connectivity" ) {
            
                // Make a net graph
                NetGraph net_graph(top_snarl.start(), top_snarl.end(), top_chains, top_unary_snarls, &graph, false);
                
                SECTION( "The top-level NetGraph has 4 nodes" ) {
                    unordered_set<handle_t> nodes;
                    size_t node_count = 0;
                    net_graph.for_each_handle([&](const handle_t& handle) {
                        nodes.insert(handle);
                        node_count++;
                    });
                    REQUIRE(nodes.size() == node_count);
                    
                    REQUIRE(nodes.size() == 4);
                    
                    SECTION( "The nodes are numbered 1, 2, 4, and 8" ) {
                        REQUIRE(nodes.count(net_graph.get_handle(1, false)) == 1);
                        REQUIRE(nodes.count(net_graph.get_handle(2, false)) == 1);
                        REQUIRE(nodes.count(net_graph.get_handle(4, false)) == 1);
                        REQUIRE(nodes.count(net_graph.get_handle(8, false)) == 1);
                    }
                }
                
                SECTION( "The top-level NetGraph has 5 edges" ) {
                    unordered_set<pair<handle_t, handle_t>> edges;
                    for (auto& id : {1, 2, 4, 8}) {
                        // Go through the nodes we should have manually.
                        handle_t handle = net_graph.get_handle(id, false);
                    
                        // Save all the edges off of each node
                        net_graph.follow_edges(handle, false, [&](const handle_t& other) {
                            edges.insert(net_graph.edge_handle(handle, other));
                        });
                        net_graph.follow_edges(handle, true, [&](const handle_t& other) {
                            edges.insert(net_graph.edge_handle(other, handle));
                        });
                    }
#ifdef debug
                    for (auto& edge : edges) {
                        cerr << net_graph.get_id(edge.first) << " " << net_graph.get_is_reverse(edge.first) << " -> "
                            << net_graph.get_id(edge.second) << " " << net_graph.get_is_reverse(edge.second) << endl;
                    }
#endif
                    
                    REQUIRE(edges.size() == 5);
                    
                    // The edges are 1 to 8, 1 to 2, 1 to 4, 2 rev to 8, and 4 rev to 8.
                    SECTION( "All of the expected edges exist" ) {
                        REQUIRE(edges.count(net_graph.edge_handle(net_graph.get_handle(1, false),
                            net_graph.get_handle(8, false))) == 1);
                        REQUIRE(edges.count(net_graph.edge_handle(net_graph.get_handle(1, false),
                            net_graph.get_handle(2, false))) == 1);
                        REQUIRE(edges.count(net_graph.edge_handle(net_graph.get_handle(1, false),
                            net_graph.get_handle(4, false))) == 1);
                        REQUIRE(edges.count(net_graph.edge_handle(net_graph.get_handle(2, true),
                            net_graph.get_handle(8, false))) == 1);
                        REQUIRE(edges.count(net_graph.edge_handle(net_graph.get_handle(4, true),
                            net_graph.get_handle(8, false))) == 1);
                    }
                }
            }
            
             SECTION( "A connectivity-respecting net graph respects connectivity" ) {
            
                // Make a net graph
                NetGraph net_graph(top_snarl.start(), top_snarl.end(), top_chains, top_unary_snarls, &graph, true);
                
                SECTION( "The top-level NetGraph has 4 nodes" ) {
                    unordered_set<handle_t> nodes;
                    size_t node_count = 0;
                    net_graph.for_each_handle([&](const handle_t& handle) {
                        nodes.insert(handle);
                        node_count++;
                    });
                    REQUIRE(nodes.size() == node_count);
                    
                    REQUIRE(nodes.size() == 4);
                    
                    SECTION( "The nodes are numbered 1, 2, 4, and 8" ) {
                        REQUIRE(nodes.count(net_graph.get_handle(1, false)) == 1);
                        REQUIRE(nodes.count(net_graph.get_handle(2, false)) == 1);
                        REQUIRE(nodes.count(net_graph.get_handle(4, false)) == 1);
                        REQUIRE(nodes.count(net_graph.get_handle(8, false)) == 1);
                    }
                }
                
                SECTION( "The top-level NetGraph has 5 edges when looking at its ends" ) {
                    unordered_set<pair<handle_t, handle_t>> edges;
                    for (auto& id : {1, 8}) {
                        // Go through the nodes we should have manually.
                        handle_t handle = net_graph.get_handle(id, false);
                    
                        // Save all the edges off of each node
                        net_graph.follow_edges(handle, false, [&](const handle_t& other) {
                            edges.insert(net_graph.edge_handle(handle, other));
                        });
                        net_graph.follow_edges(handle, true, [&](const handle_t& other) {
                            edges.insert(net_graph.edge_handle(other, handle));
                        });
                    }
#ifdef debug
                    for (auto& edge : edges) {
                        cerr << net_graph.get_id(edge.first) << " " << net_graph.get_is_reverse(edge.first) << " -> "
                            << net_graph.get_id(edge.second) << " " << net_graph.get_is_reverse(edge.second) << endl;
                    }
#endif
                    
                    REQUIRE(edges.size() == 5);
                    
                    // The edges are 1 to 8, 1 to 2, 1 to 4, 2 rev to 8, and 4 rev to 8.
                    SECTION( "All of the expected edges exist" ) {
                        REQUIRE(edges.count(net_graph.edge_handle(net_graph.get_handle(1, false),
                            net_graph.get_handle(8, false))) == 1);
                        REQUIRE(edges.count(net_graph.edge_handle(net_graph.get_handle(1, false),
                            net_graph.get_handle(2, false))) == 1);
                        REQUIRE(edges.count(net_graph.edge_handle(net_graph.get_handle(1, false),
                            net_graph.get_handle(4, false))) == 1);
                        REQUIRE(edges.count(net_graph.edge_handle(net_graph.get_handle(2, true),
                            net_graph.get_handle(8, false))) == 1);
                        REQUIRE(edges.count(net_graph.edge_handle(net_graph.get_handle(4, true),
                            net_graph.get_handle(8, false))) == 1);
                    }
                }
                
                SECTION( "The top-level NetGraph only has edges for the connected child when looking at its child snarls" ) {
                    unordered_set<pair<handle_t, handle_t>> edges;
                    for (auto& id : {2, 4}) {
                        // Go through the nodes we should have manually.
                        handle_t handle = net_graph.get_handle(id, false);
                    
                        // Save all the edges off of each node
                        net_graph.follow_edges(handle, false, [&](const handle_t& other) {
                            edges.insert(net_graph.edge_handle(handle, other));
                        });
                        net_graph.follow_edges(handle, true, [&](const handle_t& other) {
                            edges.insert(net_graph.edge_handle(other, handle));
                        });
                    }
#ifdef debug
                    for (auto& edge : edges) {
                        cerr << net_graph.get_id(edge.first) << " " << net_graph.get_is_reverse(edge.first) << " -> "
                            << net_graph.get_id(edge.second) << " " << net_graph.get_is_reverse(edge.second) << endl;
                    }
#endif
                    
                    // We ignore the edges we would get if we could do a real
                    // predecessor lookup. In this mode we only allow real
                    // traversals, so looking left from entering the unary snarl
                    // gives you nothing, not the edges you could have come from.
                    REQUIRE(edges.size() == 2);
                    
                    // The edges are the fake traversal-allowing ones: 4 forward to 8 forward, and 4 forward to 1 reverse.
                    SECTION( "All of the expected edges exist" ) {
                        REQUIRE(edges.count(net_graph.edge_handle(net_graph.get_handle(4, false),
                            net_graph.get_handle(8, false))) == 1);
                        REQUIRE(edges.count(net_graph.edge_handle(net_graph.get_handle(4, false),
                            net_graph.get_handle(1, true))) == 1);
                    }
                }
            }
            
        }
        
        TEST_CASE( "NetGraph can find hard-to-reach snarl contents",
                  "[snarl_distance]" ) {
        
        
            // This graph will have a snarl from 1 to 8, a nested unary snarl on 2, and a nested unary snarl on 4.
            // Node 3 will be attached only through these non-traversable unary snarls.
            VG graph;
                
            Node* n1 = graph.create_node("GCA");
            Node* n2 = graph.create_node("T");
            Node* n3 = graph.create_node("G");
            Node* n4 = graph.create_node("CTGA");
            Node* n5 = graph.create_node("GCA");
            Node* n6 = graph.create_node("T");
            Node* n7 = graph.create_node("G");
            Node* n8 = graph.create_node("CTGA");
            
            Edge* e1 = graph.create_edge(n1, n2);
            Edge* e2 = graph.create_edge(n1, n4);
            Edge* e3 = graph.create_edge(n1, n8);
            Edge* e4 = graph.create_edge(n3, n2);
            Edge* e5 = graph.create_edge(n4, n5);
            Edge* e6 = graph.create_edge(n4, n6);
            Edge* e7 = graph.create_edge(n5, n7);
            Edge* e8 = graph.create_edge(n6, n7);
            Edge* e9 = graph.create_edge(n8, n4, true, false);
            Edge* e10 = graph.create_edge(n3, n4);
            
            // Define the snarls for the top level
            Snarl top_snarl;
            top_snarl.mutable_start()->set_node_id(n1->id());
            top_snarl.mutable_end()->set_node_id(n8->id());
            
            // We have no chains
            vector<vector<pair<Snarl, bool>>> top_chains;
            
            // We havetwo child unary snarls.
            vector<Snarl> top_unary_snarls;

            top_unary_snarls.emplace_back();
            auto& nested_snarl1 = top_unary_snarls.back(); 
            
            // The first one is a tip of just node 2, based at node 2
            nested_snarl1.mutable_start()->set_node_id(n2->id());
            nested_snarl1.mutable_end()->set_node_id(n2->id());
            nested_snarl1.mutable_end()->set_backward(true);
            nested_snarl1.set_type(UNARY);
            *nested_snarl1.mutable_parent() = top_snarl;
            nested_snarl1.set_directed_acyclic_net_graph(true);
            // We aren't actually traversable at all
            nested_snarl1.set_start_self_reachable(false);
            nested_snarl1.set_end_self_reachable(false);
            nested_snarl1.set_start_end_reachable(false);
            
            top_unary_snarls.emplace_back();
            auto& nested_snarl2 = top_unary_snarls.back(); 
            
            // The second one is a big assemblage rooted at 4 that also isn't traversable
            nested_snarl2.mutable_start()->set_node_id(n4->id());
            nested_snarl2.mutable_end()->set_node_id(n4->id());
            nested_snarl2.mutable_end()->set_backward(true);
            nested_snarl2.set_type(UNARY);
            *nested_snarl2.mutable_parent() = top_snarl;
            nested_snarl2.set_directed_acyclic_net_graph(true);
            // We aren't actually traversable at all
            nested_snarl2.set_start_self_reachable(false);
            nested_snarl2.set_end_self_reachable(false);
            nested_snarl2.set_start_end_reachable(false);
            
            SECTION( "A connectivity-ignoring net graph lets you find all the nodes" ) {
            
                // Make a net graph
                NetGraph net_graph(top_snarl.start(), top_snarl.end(), top_chains, top_unary_snarls, &graph, false);
                
                SECTION( "The top-level NetGraph has 5 nodes" ) {
                    unordered_set<handle_t> nodes;
                    size_t node_count = 0;
                    net_graph.for_each_handle([&](const handle_t& handle) {
                        nodes.insert(handle);
                        node_count++;
                    });
                    REQUIRE(nodes.size() == node_count);
                    
                    REQUIRE(nodes.size() == 5);
                    
                }
                
            }
            
            SECTION( "A connectivity-respecting net graph still lets you find all the nodes" ) {
            
                // Make a net graph
                NetGraph net_graph(top_snarl.start(), top_snarl.end(), top_chains, top_unary_snarls, &graph, true);
                
                SECTION( "The top-level NetGraph has 5 nodes" ) {
                    unordered_set<handle_t> nodes;
                    size_t node_count = 0;
                    net_graph.for_each_handle([&](const handle_t& handle) {
                        nodes.insert(handle);
                        node_count++;
                    });
                    REQUIRE(nodes.size() == node_count);
                    
                    REQUIRE(nodes.size() == 5);
                    
                }
                
            }
            
        }
        */
    
        TEST_CASE( "Distance index's snarl functions return expected answers",
                  "[snarl_distance][bug]" ) {
            
            SECTION( "SnarlManager can be constructed with cactus ultrabubbles") {
                
                VG graph;
                
                Node* n1 = graph.create_node("GCA");
                Node* n2 = graph.create_node("T");
                Node* n3 = graph.create_node("G");
                Node* n4 = graph.create_node("CTGA");
                
                graph.create_edge(n1, n2);
                graph.create_edge(n1, n3);
                graph.create_edge(n2, n4);
                graph.create_edge(n3, n4);
                
                IntegratedSnarlFinder snarl_finder(graph);
                SnarlDistanceIndex distance_index(&graph, &snarl_finder);
                
            }
            
            SECTION( "SnarlManager can correctly navigate tree relationships") {
                
                VG graph;
                
                Node* n1 = graph.create_node("GCA");
                Node* n2 = graph.create_node("T");
                Node* n3 = graph.create_node("G");
                Node* n4 = graph.create_node("CTGA");
                Node* n5 = graph.create_node("GCA");
                Node* n6 = graph.create_node("T");
                Node* n7 = graph.create_node("G");
                Node* n8 = graph.create_node("CTGA");
                
                Edge* e1 = graph.create_edge(n1, n2);
                Edge* e2 = graph.create_edge(n1, n8);
                Edge* e3 = graph.create_edge(n2, n3);
                Edge* e4 = graph.create_edge(n2, n6);
                Edge* e5 = graph.create_edge(n3, n4);
                Edge* e6 = graph.create_edge(n3, n5);
                Edge* e7 = graph.create_edge(n4, n5);
                Edge* e8 = graph.create_edge(n5, n7);
                Edge* e9 = graph.create_edge(n6, n7);
                Edge* e10 = graph.create_edge(n7, n8);
                
                IntegratedSnarlFinder snarl_finder(graph);
                SnarlDistanceIndex distance_index(&graph, &snarl_finder);

                //The root contains one connected component
                net_handle_t root_handle = distance_index.get_root();
                net_handle_t top_chain_handle;
                size_t component_count = 0;
                distance_index.for_each_child(root_handle, [&](const net_handle_t& child) {
                    component_count += 1;
                    top_chain_handle = child;
                    REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                            == distance_index.canonical(root_handle));
                });
                REQUIRE(component_count == 1);
                REQUIRE(distance_index.is_chain(top_chain_handle));


                //The top connected component is a chain with one snarl
                net_handle_t top_snarl_handle;
                size_t child_i = 0;
                distance_index.for_each_child(top_chain_handle, [&](const net_handle_t& child) {
                    if (child_i == 0) {
                        REQUIRE(distance_index.is_node(child));
                        REQUIRE((graph.get_id(distance_index.get_handle(child, &graph)) == n1->id() ||
                                graph.get_id(distance_index.get_handle(child, &graph)) == n8->id()));
                    } else if (child_i == 1) {
                        REQUIRE(distance_index.is_snarl(child));
                        top_snarl_handle = child;
                    } else if (child_i == 2) {
                        REQUIRE(distance_index.is_node(child));
                        REQUIRE((graph.get_id(distance_index.get_handle(child, &graph)) == n1->id() ||
                                graph.get_id(distance_index.get_handle(child, &graph)) == n8->id()));
                    } else {
                        //The chain should only contain two nodes and a snarl
                        REQUIRE(false);
                    }
                    REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                            == distance_index.canonical(top_chain_handle));
                    child_i++;
                });
                REQUIRE(child_i == 3);


                //Get graph handle for bounds of the top snarl facing in
                handle_t top_snarl_start = distance_index.get_handle(
                    distance_index.get_bound(top_snarl_handle, false, true), &graph);
                handle_t top_snarl_end  = distance_index.get_handle(
                    distance_index.get_bound(top_snarl_handle, true, true), &graph);
                
                bool top_level_correct = ((graph.get_id(top_snarl_start) == n1->id() &&
                                           graph.get_id(top_snarl_end) == n8->id() &&
                                           !graph.get_is_reverse(top_snarl_start) &&
                                           graph.get_is_reverse(top_snarl_end)) ||
                                          (graph.get_id(top_snarl_start) == n8->id() &&
                                           graph.get_id(top_snarl_end) == n1->id() &&
                                           !graph.get_is_reverse(top_snarl_start) &&
                                           graph.get_is_reverse(top_snarl_end)));
                REQUIRE(top_level_correct);


                //There should be two middle level chains, a trivial chain 6 and a chain
                //thats a snarl from 2 to 7
                
                net_handle_t middle_level_chain;
                size_t middle_chain_count = 0;
                distance_index.for_each_child(top_snarl_handle, [&](const net_handle_t& child) {
                    middle_level_chain = child;
                    middle_chain_count ++;
                    REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                            == distance_index.canonical(top_snarl_handle));
                });
                REQUIRE(middle_chain_count == 1);

                net_handle_t middle_level_snarl;
                size_t middle_snarl_count = 0;
                distance_index.for_each_child(middle_level_chain, [&](const net_handle_t& child) {
                    if (middle_snarl_count == 0 || middle_snarl_count == 2) {
                        REQUIRE(distance_index.is_node(child));
                    } else {
                        REQUIRE(distance_index.is_snarl(child));
                        middle_level_snarl = child;
                    }
                    REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                            == distance_index.canonical(middle_level_chain));
                    middle_snarl_count ++;
                });
                REQUIRE(middle_snarl_count == 3); //Actually two nodes and a snarl
                REQUIRE(distance_index.is_snarl(middle_level_snarl));

                //Get graph handle for bounds of snarl facing in
                handle_t middle_snarl_start = distance_index.get_handle(
                    distance_index.get_bound(middle_level_snarl, false, true), &graph);
                handle_t middle_snarl_end  = distance_index.get_handle(
                    distance_index.get_bound(middle_level_snarl, true, true), &graph);
                
                bool middle_level_correct = ((graph.get_id(middle_snarl_start) == n2->id() &&
                                           graph.get_id(middle_snarl_end) == n7->id() &&
                                           !graph.get_is_reverse(middle_snarl_start) &&
                                           graph.get_is_reverse(middle_snarl_end)) ||
                                          (graph.get_id(middle_snarl_start) == n7->id() &&
                                           graph.get_id(middle_snarl_end) == n2->id() &&
                                           graph.get_is_reverse(middle_snarl_start) &&
                                           !graph.get_is_reverse(middle_snarl_end)));
                REQUIRE(middle_level_correct);
                

                //There should be one bottom level snarl should be from 2 to 7
                
                net_handle_t bottom_level_chain;
                net_handle_t bottom_level_node;
                size_t bottom_level_chain_count = 0;
                distance_index.for_each_child(middle_level_snarl, [&](const net_handle_t& child) {
                    if (distance_index.is_trivial_chain(child)){
                        distance_index.for_each_child(child, [&](const net_handle_t& grandchild) {
                            bottom_level_node = grandchild;
                        });
                    } else {
                        bottom_level_chain = child;
                    }
                    bottom_level_chain_count ++;
                    REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                            == distance_index.canonical(middle_level_snarl));
                });
                REQUIRE(bottom_level_chain_count == 2);

                REQUIRE(graph.get_id(distance_index.get_handle(bottom_level_node, &graph)) 
                        == n6->id());
                vector<net_handle_t> bottom_level_snarls;
                distance_index.for_each_child(bottom_level_chain, [&](const net_handle_t& child) {
                    bottom_level_snarls.emplace_back(child);
                    REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                            == distance_index.canonical(bottom_level_chain));
                });
                REQUIRE(bottom_level_snarls.size() == 3); //Actually two nodes and a snarl

                //Get graph handle for bounds of snarl facing in
                handle_t bottom_snarl_start = distance_index.get_handle(
                    distance_index.get_bound(bottom_level_snarls[1], false, true), &graph);
                handle_t bottom_snarl_end  = distance_index.get_handle(
                    distance_index.get_bound(bottom_level_snarls[1], true, true), &graph);
                
                bool bottom_level_correct = ((graph.get_id(bottom_snarl_start) == n3->id() &&
                                           graph.get_id(bottom_snarl_end) == n5->id() &&
                                           !graph.get_is_reverse(bottom_snarl_start) &&
                                           graph.get_is_reverse(bottom_snarl_end)) ||
                                          (graph.get_id(bottom_snarl_start) == n5->id() &&
                                           graph.get_id(bottom_snarl_end) == n3->id() &&
                                           graph.get_is_reverse(bottom_snarl_start) &&
                                           !graph.get_is_reverse(bottom_snarl_end)));
                REQUIRE(bottom_level_correct);
                
            }
            
            SECTION( "SnarlManager can correctly extract the contents of a snarl") {
                
                VG graph;
                
                Node* n0 = graph.create_node("CA");
                Node* n1 = graph.create_node("GCA");
                Node* n2 = graph.create_node("C");
                Node* n3 = graph.create_node("A");
                Node* n4 = graph.create_node("CTGA");
                Node* n5 = graph.create_node("GCA");
                Node* n6 = graph.create_node("T");
                Node* n7 = graph.create_node("G");
                Node* n8 = graph.create_node("AGTA");
                Node* n9 = graph.create_node("CCC");
                
                Edge* e0 = graph.create_edge(n0, n1);
                Edge* e1 = graph.create_edge(n1, n2);
                Edge* e2 = graph.create_edge(n1, n8);
                Edge* e3 = graph.create_edge(n2, n3);
                Edge* e4 = graph.create_edge(n2, n6);
                Edge* e5 = graph.create_edge(n3, n4);
                Edge* e6 = graph.create_edge(n3, n5);
                Edge* e7 = graph.create_edge(n4, n5);
                Edge* e8 = graph.create_edge(n5, n7);
                Edge* e9 = graph.create_edge(n6, n7);
                Edge* e10 = graph.create_edge(n7, n8);
                Edge* e11 = graph.create_edge(n8, n9);
                
                IntegratedSnarlFinder snarl_finder(graph);
                SnarlDistanceIndex distance_index(&graph, &snarl_finder);
                
                //The root contains one connected component
                net_handle_t root_handle = distance_index.get_root();
                net_handle_t top_chain_handle;
                size_t component_count = 0;
                distance_index.for_each_child(root_handle, [&](const net_handle_t& child) {
                    component_count += 1;
                    top_chain_handle = child;
                    REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                            == distance_index.canonical(root_handle));
                });
                REQUIRE(component_count == 1);


                //The top connected component is a chain with one snarl and four boundaries
                net_handle_t top_snarl_handle;
                size_t child_i = 0;
                distance_index.for_each_child(top_chain_handle, [&](const net_handle_t& child) {

                    if (child_i == 0) {
                        REQUIRE(distance_index.is_node(child));
                        REQUIRE((graph.get_id(distance_index.get_handle(child, &graph)) == n0->id() ||
                                graph.get_id(distance_index.get_handle(child, &graph)) == n9->id()));
                    } else if (child_i == 1) {
                        REQUIRE(distance_index.is_node(child));
                        REQUIRE((graph.get_id(distance_index.get_handle(child, &graph)) == n1->id() ||
                                graph.get_id(distance_index.get_handle(child, &graph)) == n8->id()));
                    } else if (child_i == 2) {
                        REQUIRE(distance_index.is_snarl(child));
                        top_snarl_handle = child;
                    } else if (child_i == 3) {
                        REQUIRE(distance_index.is_node(child));
                        REQUIRE((graph.get_id(distance_index.get_handle(child, &graph)) == n1->id() ||
                                graph.get_id(distance_index.get_handle(child, &graph)) == n8->id()));
                    } else if (child_i == 4) {
                        REQUIRE(distance_index.is_node(child));
                        REQUIRE((graph.get_id(distance_index.get_handle(child, &graph)) == n0->id() ||
                                graph.get_id(distance_index.get_handle(child, &graph)) == n9->id()));} else {
                        //The chain should only contain two nodes and a snarl
                        REQUIRE(false);
                    }
                    REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                            == distance_index.canonical(top_chain_handle));
                    child_i++;
                });
                REQUIRE(child_i == 5);


                //Get graph handle for bounds of the top snarl facing in
                handle_t top_snarl_start = distance_index.get_handle(
                    distance_index.get_bound(top_snarl_handle, false, true), &graph);
                handle_t top_snarl_end  = distance_index.get_handle(
                    distance_index.get_bound(top_snarl_handle, true, true), &graph);
                
                bool top_level_correct = ((graph.get_id(top_snarl_start) == n1->id() &&
                                           graph.get_id(top_snarl_end) == n8->id() &&
                                           !graph.get_is_reverse(top_snarl_start) &&
                                           graph.get_is_reverse(top_snarl_end)) ||
                                          (graph.get_id(top_snarl_start) == n8->id() &&
                                           graph.get_id(top_snarl_end) == n1->id() &&
                                           graph.get_is_reverse(top_snarl_start) &&
                                           !graph.get_is_reverse(top_snarl_end)));
                REQUIRE(top_level_correct);

                //Check that for the top level snarl (1,8), we have all three edges
                net_handle_t snarl_start = 
                    distance_index.get_bound(top_snarl_handle, false, true);
                size_t edge_count = 0;
                distance_index.follow_net_edges(snarl_start, &graph, false, [&](const net_handle_t& other) {
                    if (distance_index.is_chain(other)) {
                    //TODO: It should be 2 but I think it might be ok if it reaches 7 since it's the end of the snarl. Same below
                        id_t start_id = graph.get_id(distance_index.get_handle(
                                distance_index.get_bound(other, false, true), &graph));
                        id_t end_id = graph.get_id(distance_index.get_handle(
                                distance_index.get_bound(other, true, true), &graph));
                        REQUIRE(((start_id == n2->id() && end_id == n7->id()) ||
                                (start_id == n7->id() && end_id == n2->id())));
                    } else {
                        REQUIRE(distance_index.is_sentinel(other));
                        REQUIRE(graph.get_id(distance_index.get_handle(other, &graph)) == n8->id());
                    }
                    edge_count ++;
                    return true;
                });
                REQUIRE(edge_count == 2);

                net_handle_t snarl_end  = distance_index.get_bound(top_snarl_handle, true, true);
                edge_count = 0;
                distance_index.follow_net_edges(snarl_end, &graph, false, [&](const net_handle_t& other) {
                    if (distance_index.is_chain(other)) {
                        id_t start_id = graph.get_id(distance_index.get_handle(
                                distance_index.get_bound(other, false, true), &graph));
                        id_t end_id = graph.get_id(distance_index.get_handle(
                                distance_index.get_bound(other, true, true), &graph));
                        REQUIRE(((start_id == n2->id() && end_id == n7->id()) ||
                                (start_id == n7->id() && end_id == n2->id())));
                    } else {
                        REQUIRE(distance_index.is_sentinel(other));
                        REQUIRE(graph.get_id(distance_index.get_handle(other, &graph)) == n1->id());
                    }
                    edge_count++;
                    return true;
                });
                REQUIRE(edge_count == 2);

                //Got through the children of the top snarl, should just be chain 2,7
                distance_index.for_each_child(top_snarl_handle, [&](const net_handle_t& child) {
                    //Count how many edges leaving this child, should only reach the boundaries
                    size_t count = 0;
                    distance_index.follow_net_edges(child, &graph, false, [&](const net_handle_t& other) {
                        REQUIRE(distance_index.is_sentinel(other));
                        id_t id = graph.get_id(distance_index.get_handle(other, &graph));
                        REQUIRE((id == n1->id() || id == n8->id()));
                        count ++;
                    });
                    REQUIRE(count == 1);
                    REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                            == distance_index.canonical(top_snarl_handle));
                    return true;
                });
                
            }
           
            SECTION( "SnarlManager can correctly extract the contents of a snarl containing cycles and tips") {
                //TODO: Add tests for identifying tips
                
                VG graph;
                
                Node* n1 = graph.create_node("GCA");
                Node* n2 = graph.create_node("C");
                Node* n3 = graph.create_node("A");
                Node* n4 = graph.create_node("CTGA");
                Node* n5 = graph.create_node("GCA");
                Node* n6 = graph.create_node("T");
                Node* n7 = graph.create_node("G");
                Node* n8 = graph.create_node("AGTA");
                
                Edge* e1 = graph.create_edge(n1, n2);
                Edge* e2 = graph.create_edge(n2, n7);
                Edge* e3 = graph.create_edge(n2, n4);
                Edge* e4 = graph.create_edge(n3, n4);
                Edge* e5 = graph.create_edge(n4, n5);
                Edge* e6 = graph.create_edge(n4, n6);
                Edge* e7 = graph.create_edge(n5, n6);
                Edge* e8 = graph.create_edge(n6, n2, false, true);
                Edge* e9 = graph.create_edge(n6, n7);
                Edge* e10 = graph.create_edge(n7, n8);

                IntegratedSnarlFinder snarl_finder(graph);
                SnarlDistanceIndex distance_index(&graph, &snarl_finder);
                
                //The root contains one connected component
                net_handle_t root_handle = distance_index.get_root();
                net_handle_t top_chain_handle;
                size_t component_count = 0;
                distance_index.for_each_child(root_handle, [&](const net_handle_t& child) {
                    component_count += 1;
                    top_chain_handle = child;
                    REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                            == distance_index.canonical(root_handle));
                });
                REQUIRE(component_count == 1);


                //The top connected component is a chain with one snarl and four boundaries
                net_handle_t top_snarl_handle;
                size_t child_i = 0;
                distance_index.for_each_child(top_chain_handle, [&](const net_handle_t& child) {
                    ;
                    if (child_i == 0 || child_i == 4) {
                        //FIrst or last child
                        REQUIRE(distance_index.is_node(child));
                        REQUIRE((graph.get_id(distance_index.get_handle(child, &graph)) == n1->id() ||
                                graph.get_id(distance_index.get_handle(child, &graph)) == n8->id()));
                    } else if (child_i == 1 || child_i == 3) {
                        REQUIRE(distance_index.is_node(child));
                        REQUIRE((graph.get_id(distance_index.get_handle(child, &graph)) == n2->id() ||
                                graph.get_id(distance_index.get_handle(child, &graph)) == n7->id()));
                    }  else {
                        //Middle child = snarl
                        REQUIRE(distance_index.is_snarl(child));
                        top_snarl_handle = child;
                    }
                    child_i++;
                    REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                            == distance_index.canonical(top_chain_handle));
                });
                REQUIRE(child_i == 5);

                SECTION( "Index finds only possible traversals") {
                    size_t traversal_count = 0;
                    distance_index.for_each_traversal(top_snarl_handle, [&](const net_handle_t net) {

                        bool end_end = distance_index.starts_at(net) == SnarlDecomposition::END &&
                            distance_index.ends_at(net) == SnarlDecomposition::END;
                        if (graph.get_id(distance_index.get_handle(distance_index.get_bound(top_snarl_handle, false, false), &graph)) == 7) {
                            end_end = distance_index.starts_at(net) == SnarlDecomposition::START &&
                                 distance_index.ends_at(net) == SnarlDecomposition::START;
                        }
                        bool tip_tip = distance_index.starts_at(net) == SnarlDecomposition::TIP &&
                            distance_index.ends_at(net) == SnarlDecomposition::TIP;
                        REQUIRE((! tip_tip && !end_end));
                        traversal_count++;
                    });
                    REQUIRE(traversal_count == 7);
                }

                //Get graph handle for bounds of the top snarl facing in
                handle_t top_snarl_start = distance_index.get_handle(
                    distance_index.get_bound(top_snarl_handle, false, true), &graph);
                handle_t top_snarl_end  = distance_index.get_handle(
                    distance_index.get_bound(top_snarl_handle, true, true), &graph);
                
                bool top_level_correct = ((graph.get_id(top_snarl_start) == n2->id() &&
                                           graph.get_id(top_snarl_end) == n7->id() &&
                                           !graph.get_is_reverse(top_snarl_start) &&
                                           graph.get_is_reverse(top_snarl_end)) ||
                                          (graph.get_id(top_snarl_start) == n7->id() &&
                                           graph.get_id(top_snarl_end) == n2->id() &&
                                           graph.get_is_reverse(top_snarl_start) &&
                                           !graph.get_is_reverse(top_snarl_end)));
                REQUIRE(top_level_correct);
                

                //Go through children of top snarl (1->7);
                size_t snarl_child_count = 0;
                net_handle_t node_3;
                net_handle_t chain_4_6;
                distance_index.for_each_child(top_snarl_handle, [&](const net_handle_t& child) {
                    REQUIRE(distance_index.is_chain(child));
                    size_t chain_child_count = 0;
                    //Go through children of chains (trivial chain 3 and chain 4->6);
                    distance_index.for_each_child(child, [&](const net_handle_t& grandchild){
                        if (distance_index.is_node(grandchild)) {
                            id_t id = graph.get_id(distance_index.get_handle(grandchild, &graph));
                            REQUIRE((id == n3->id() || id == n4->id() || id == n6->id()));
                        } else {
                            REQUIRE(distance_index.is_snarl(grandchild));
                            id_t start_id = graph.get_id(distance_index.get_handle(
                                    distance_index.get_bound(grandchild, false, true), &graph));
                            id_t end_id = graph.get_id(distance_index.get_handle(
                                    distance_index.get_bound(grandchild, true, true), &graph));
                            REQUIRE( ((start_id == n4->id() && end_id == n6->id()) ||
                                      (start_id == n6->id() && end_id == n4->id())));
                        }
                        chain_child_count ++;
                        REQUIRE( distance_index.canonical(distance_index.get_parent(grandchild))
                            == distance_index.canonical(child));
                        return true;

                    });
                    REQUIRE(((chain_child_count == 1) || (chain_child_count == 3)));
                    if (chain_child_count == 1) {
                        node_3 = child;
                    } else {
                        chain_4_6 = child;
                    }
                    snarl_child_count++;
                    REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                            == distance_index.canonical(top_snarl_handle));
                    return true;
                });
                REQUIRE(snarl_child_count == 2);
                size_t edge_count = 0;
                distance_index.follow_net_edges(node_3, &graph, false, [&](const net_handle_t& next) {
                    REQUIRE(distance_index.is_chain(next));
                    id_t start = graph.get_id(distance_index.get_handle(
                            distance_index.get_bound(next, false, true), &graph));
                    id_t end = graph.get_id(distance_index.get_handle(
                            distance_index.get_bound(next, true, true), &graph));
                    REQUIRE(( (start == n4->id() && end == n6->id()) ||
                              (start == n6->id() && end == n4->id())));
                    edge_count++;
                });
                REQUIRE(edge_count == 1);
                edge_count = 0;
                distance_index.follow_net_edges(node_3,&graph,  true, [&](const net_handle_t& next) {
                    edge_count++;
                });
                REQUIRE(edge_count == 0);

                
                //Walking out from chain 4,6 should reach 3, 2, or 7
                bool snarl_rev = graph.get_id(distance_index.get_handle(
                    distance_index.get_bound(chain_4_6, false, true), &graph)) == n6->id();


                //Traverse 4->6
                edge_count = 0;
                bool found_2 = false;
                bool found_7 = false;
                distance_index.follow_net_edges(chain_4_6, &graph, snarl_rev, [&](const net_handle_t& next) {
                    REQUIRE(distance_index.is_sentinel(next));
                    //This is 2 or 7
                    id_t id = graph.get_id(distance_index.get_handle(next, &graph));
                    if (id == n2->id()) {
                        found_2 = true; 
                    } else {
                        found_7 = true;
                    }
                    REQUIRE((id == n2->id() || n7->id())); 
                    edge_count++;
                });


                //Traverse 6->4
                REQUIRE(found_2);
                REQUIRE(found_7);
                edge_count = 0;
                distance_index.follow_net_edges(chain_4_6, &graph, !snarl_rev, [&](const net_handle_t& next) {
                    if (distance_index.is_chain(next)){
                        //This is the node 3
                        REQUIRE(distance_index.is_trivial_chain(next));
                        distance_index.for_each_child(next, [&](const net_handle_t& child){
                            REQUIRE(distance_index.is_node(child));
                            REQUIRE(graph.get_id(distance_index.get_handle(child, &graph)) == n3->id());
                            REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                                == distance_index.canonical(next));
                        });

                    } else {
                        REQUIRE(distance_index.is_sentinel(next));
                        //This is 2 or 7
                        id_t id = graph.get_id(distance_index.get_handle(next, &graph));
                        REQUIRE(id == n2->id());
                    }
                    edge_count++;
                });
                REQUIRE(edge_count == 2);

                
            }
            
            
            
            
            //SECTION( "SnarlManager can correctly extract the full contents of a reversing-edge snarl") {
            //    
            //    string graph_json = R"(
            //    {
            //        "node": [
            //                 {
            //                 "sequence": "TTTTTG",
            //                 "id": 6462830
            //                 },
            //                 {
            //                 "sequence": "AAAAAAAAAAAAAA",
            //                 "id": 8480141
            //                 },
            //                 {
            //                 "sequence": "A",
            //                 "id": 6462831
            //                 },
            //                 {
            //                 "sequence": "T",
            //                 "id": 6462832
            //                 },
            //                 {
            //                 "sequence": "G",
            //                 "id": 8480142
            //                 },
            //                 {
            //                 "sequence": "A",
            //                 "id": 8480143
            //                 }
            //                 ],
            //        "edge": [
            //                 {
            //                 "to": 8480141,
            //                 "from": 6462830,
            //                 "from_start": true
            //                 },
            //                 {
            //                 "to": 6462831,
            //                 "from": 6462830
            //                 },
            //                 {
            //                 "to": 6462832,
            //                 "from": 6462830
            //                 },
            //                 {
            //                 "to": 8480142,
            //                 "from": 8480141
            //                 },
            //                 {
            //                 "to": 8480143,
            //                 "from": 8480141
            //                 }
            //                 ]
            //    }
            //    )";
            //    
            //    VG graph;
            //    
            //    // Load up the graph
            //    Graph g;
            //    json2pb(g, graph_json.c_str(), graph_json.size());
            //    graph.extend(g);
            //    
            //    // Define the one snarl
            //    Snarl snarl1;
            //    snarl1.mutable_start()->set_node_id(6462830);
            //    snarl1.mutable_start()->set_backward(true);
            //    snarl1.mutable_end()->set_node_id(8480141);
            //    snarl1.set_type(ULTRABUBBLE);
            //    
            //    list<Snarl> snarls;
            //    snarls.push_back(snarl1);
            //    
            //    SnarlManager snarl_manager(snarls.begin(), snarls.end());
            //    
            //    // Find the snarl again
            //    const Snarl* snarl = snarl_manager.top_level_snarls()[0];
            //    
            //    // Get its contents
            //    pair<unordered_set<Node*>, unordered_set<Edge*> > contents = pb_contents(graph, snarl_manager.deep_contents(snarl, graph, true));
            //    
            //    // We need the right snarl
            //    REQUIRE(snarl->start().node_id() == 6462830);
            //    REQUIRE(snarl->start().backward());
            //    REQUIRE(snarl->end().node_id() == 8480141);
            //    REQUIRE(!snarl->end().backward());

            //    // And it needs to contain just those two nodes and the edges connecting them.
            //    REQUIRE(contents.first.size() == 2);
            //    REQUIRE(contents.second.size() == 1);
            //    
            //}
            //
            //SECTION( "SnarlManager does not include child snarls' edges in parent snarls") {
            //    
            //    // This graph is 3 nodes in a row, with two anchoring nodes on
            //    // the end, and an edge deleting the three in the middle and
            //    // just linking the anchoring nodes.
            //    string graph_json = R"(
            //    {
            //      "node": [
            //        {
            //          "sequence": "A",
            //          "id": 178895
            //        },
            //        {
            //          "sequence": "G",
            //          "id": 178896
            //        },
            //        {
            //          "sequence": "A",
            //          "id": 187209
            //        },
            //        {
            //          "sequence": "TCTCAAAAAAAAAAAAAAAAAAAAAAAAAA",
            //          "id": 178894
            //        },
            //        {
            //          "sequence": "AATGTGTCTTCCTGGGT",
            //          "id": 187208
            //        }
            //      ],
            //      "edge": [
            //        {
            //          "from": 187209,
            //          "to": 178895
            //        },
            //        {
            //          "from": 178895,
            //          "to": 178896
            //        },
            //        {
            //          "from": 178896,
            //          "to": 187208
            //        },
            //        {
            //          "from": 178894,
            //          "to": 187209
            //        },
            //        {
            //          "from": 178894,
            //          "to": 187208
            //        }
            //      ],
            //      "path": [
            //        {
            //          "name": "5",
            //          "mapping": [
            //            {
            //              "position": {
            //                "node_id": 178894
            //              },
            //              "rank": 98372
            //            },
            //            {
            //              "position": {
            //                "node_id": 187209
            //              },
            //              "rank": 98373
            //            },
            //            {
            //              "position": {
            //                "node_id": 178895
            //              },
            //              "rank": 98374
            //            },
            //            {
            //              "position": {
            //                "node_id": 178896
            //              },
            //              "rank": 98375
            //            },
            //            {
            //              "position": {
            //                "node_id": 187208
            //              },
            //              "rank": 98376
            //            }
            //          ]
            //        }
            //      ]
            //    }
            //    )";
            //    
            //    // We have one parent snarl for the deletion, with two back-to-back trivial child snarls.
            //    string snarl1_json = R"({"type": 1, "end": {"node_id": 187208}, "start": {"node_id": 178894}})";
            //    string snarl2_json = R"({"type": 1, "end": {"node_id": 187209, "backward": true}, "start": {"node_id": 178895, "backward": true}, "parent": {"end": {"node_id": 187208}, "start": {"node_id": 178894}}})";
            //    string snarl3_json = R"({"type": 1, "end": {"node_id": 178896}, "start": {"node_id": 178895}, "parent": {"end": {"node_id": 187208}, "start": {"node_id": 178894}}})";
            //    
            //    VG graph;
            //    
            //    // Load up the graph
            //    Graph g;
            //    json2pb(g, graph_json.c_str(), graph_json.size());
            //    graph.extend(g);
            //    
            //    // Load the snarls
            //    Snarl snarl1, snarl2, snarl3;
            //    json2pb(snarl1, snarl1_json.c_str(), snarl1_json.size());
            //    json2pb(snarl2, snarl2_json.c_str(), snarl2_json.size());
            //    json2pb(snarl3, snarl3_json.c_str(), snarl3_json.size());
            //    
            //    // Put them in a list
            //    list<Snarl> snarls;
            //    snarls.push_back(snarl1);
            //    snarls.push_back(snarl2);
            //    snarls.push_back(snarl3);
            //    
            //    SnarlManager snarl_manager(snarls.begin(), snarls.end());
            //    
            //    // Find the root snarl again
            //    const Snarl* snarl = snarl_manager.manage(snarl1);
            //    
            //    // Get its contents0
            //    pair<unordered_set<Node*>, unordered_set<Edge*> > contents = pb_contents(graph, snarl_manager.shallow_contents(snarl, graph, true));
            //    
            //    // We need the right snarl
            //    REQUIRE(snarl->start().node_id() == 178894);
            //    REQUIRE(!snarl->start().backward());
            //    REQUIRE(snarl->end().node_id() == 187208);
            //    REQUIRE(!snarl->end().backward());
            //    
            //    SECTION("The top-level snarl contains all 5 nodes") {
            //        REQUIRE(contents.first.size() == 5);
            //    }
            //    
            //    SECTION("The top-level snarl only contains the three edges not in any child snarl") {
            //        REQUIRE(contents.second.size() == 3);
            //    }
            //    
            //}  
        }
        
        TEST_CASE("Snarls can be found", "[snarl_distance]") {
    
            // Build a toy graph
            // Top-level chain with snarls (1,6) and (6,9)
            // Snarl (1,6) has a child snarl (2,5)
            //
            const string graph_json = R"(
            
            {
                "node": [
                    {"id": 1, "sequence": "G"},
                    {"id": 2, "sequence": "A"},
                    {"id": 3, "sequence": "T"},
                    {"id": 4, "sequence": "GGG"},
                    {"id": 5, "sequence": "T"},
                    {"id": 6, "sequence": "A"},
                    {"id": 7, "sequence": "C"},
                    {"id": 8, "sequence": "A"},
                    {"id": 9, "sequence": "A"}
                ],
                "edge": [
                    {"from": 1, "to": 2},
                    {"from": 1, "to": 6},
                    {"from": 2, "to": 3},
                    {"from": 2, "to": 4},
                    {"from": 3, "to": 5},
                    {"from": 4, "to": 5},
                    {"from": 5, "to": 6},
                    {"from": 6, "to": 7},
                    {"from": 6, "to": 8},
                    {"from": 7, "to": 9},
                    {"from": 8, "to": 9}
                    
                ],
                "path": [
                    {"name": "hint", "mapping": [
                        {"position": {"node_id": 1}, "rank" : 1 },
                        {"position": {"node_id": 6}, "rank" : 2 },
                        {"position": {"node_id": 8}, "rank" : 3 },
                        {"position": {"node_id": 9}, "rank" : 4 }
                    ]}
                ]
            }
            
            )";
            
            // Make an actual graph
            VG graph;
            Graph chunk;
            json2pb(chunk, graph_json.c_str(), graph_json.size());
            graph.extend(chunk);
            
            // We need to see the path.
            REQUIRE(graph.paths.size() == 1);
            

/*
 * TODO: This tests finding the snarls and going through them with a snarl manager
 */
            SECTION("The integrated snarl finder finds the right snarls") {
                IntegratedSnarlFinder snarl_finder(graph);
                auto snarl_manager = snarl_finder.find_snarls_parallel();
                SECTION("There are 2 top level snarls") {
                    REQUIRE(snarl_manager.top_level_snarls().size() == 2);
    
                    const Snarl* child1 = snarl_manager.top_level_snarls()[0];
                    const Snarl* child2 = snarl_manager.top_level_snarls()[1];
    
                    if (child1->start().node_id() > child1->end().node_id()) {
                        snarl_manager.flip(child1);
                    }
    
                    if (child2->start().node_id() > child2->end().node_id()) {
                        snarl_manager.flip(child2);
                    }
    
                    SECTION("First child is from 1 end to 6 start") {
    
                        {
                            bool found_in_forward_orientation = (child1->start().node_id() == 1 &&
                                                                 child1->start().backward() == false &&
                                                                 child1->end().node_id() == 6 &&
                                                                 child1->end().backward() == false);
                            bool found_in_reverse_orientation = (child1->start().node_id() == 6 &&
                                                                 child1->start().backward() == true &&
                                                                 child1->end().node_id() == 1 &&
                                                                 child1->end().backward() == true);
                            bool found_snarl = found_in_forward_orientation || found_in_reverse_orientation;
                            REQUIRE(found_snarl);
                        }
                        SECTION("First child has a child from 2 end to 5 start") {
   
                            REQUIRE(snarl_manager.children_of(child1).size() == 1);
   
                            const Snarl* subchild = snarl_manager.children_of(child1)[0];
   
                            {
                                bool found_in_forward_orientation = (subchild->start().node_id() == 2 &&
                                                                     subchild->start().backward() == false &&
                                                                     subchild->end().node_id() == 5 &&
                                                                     subchild->end().backward() == false);
                                bool found_in_reverse_orientation = (subchild->start().node_id() == 5 &&
                                                                     subchild->start().backward() == true &&
                                                                     subchild->end().node_id() == 2 &&
                                                                     subchild->end().backward() == true);
                                bool found_snarl = found_in_forward_orientation || found_in_reverse_orientation;
                                REQUIRE(found_snarl);
                            }
   
                            SECTION("Subchild has no children") {
                                REQUIRE(snarl_manager.children_of(subchild).size() == 0);
                            }
   
                        }
   
                    }

                    SECTION("Second child is from 6 end to 9 start") {
                        {
                            bool found_in_forward_orientation = (child2->start().node_id() == 6 &&
                                                                 child2->start().backward() == false &&
                                                                 child2->end().node_id() == 9 &&
                                                                 child2->end().backward() == false);
                            bool found_in_reverse_orientation = (child2->start().node_id() == 9 &&
                                                                 child2->start().backward() == true &&
                                                                 child2->end().node_id() == 6 &&
                                                                 child2->end().backward() == true);
                            bool found_snarl = found_in_forward_orientation || found_in_reverse_orientation;
                            REQUIRE(found_snarl);
                        }
     
                        SECTION("Second child has no children") {
                            REQUIRE(snarl_manager.children_of(child2).size() == 0);
                        }
                    }
     
                 }
            }
            

            SECTION("The snarl distance index finds the right snarls") {
                IntegratedSnarlFinder snarl_finder(graph); 
                SnarlDistanceIndex distance_index(&graph, &snarl_finder);
                
                    
                SECTION("There are 2 top level snarls") {

                    net_handle_t root_handle = distance_index.get_root();
                    net_handle_t top_chain_handle;
                    size_t component_count = 0;
                    distance_index.for_each_child(root_handle, [&](const net_handle_t& child) {
                        component_count += 1;
                        top_chain_handle = child;
                        REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                                == distance_index.canonical(root_handle));
                    });
                    REQUIRE(component_count == 1);

                    //The top-level chain contains two snarls, 1-6 and 6-9
                    REQUIRE(distance_index.is_chain(top_chain_handle));
                    size_t chain_child_i = 0;
                    net_handle_t child1; //snarl 1-6
                    net_handle_t child2; //snarl 6-9

                    //should go 1, (1-6), 6, (6-9), 9 in either order
                    distance_index.for_each_child(top_chain_handle, [&](const net_handle_t& child) {
                        if (chain_child_i == 0 || chain_child_i == 4) {
                            REQUIRE(distance_index.is_node(child));
                            REQUIRE((graph.get_id(distance_index.get_handle(child, &graph)) == 1 ||
                                    graph.get_id(distance_index.get_handle(child, &graph)) == 9));
                        } else if (chain_child_i == 2) {
                            REQUIRE(distance_index.is_node(child));
                            REQUIRE(graph.get_id(distance_index.get_handle(child, &graph)) == 6);
                        } else {
                            //This is the snarl 1-6 or 6-9
                            REQUIRE(distance_index.is_snarl(child));
                            id_t start = graph.get_id(distance_index.get_handle(
                                    distance_index.get_bound(child, false, false), &graph));
                            id_t end = graph.get_id(distance_index.get_handle(
                                    distance_index.get_bound(child, true, false), &graph));
                            if (start == 1 || end == 1) {
                                child1 = child;
                                REQUIRE(((start == 1 && end == 6) ||
                                         (start == 6 && end == 1) ));
                            } else {
                                REQUIRE((start == 9 || end == 9));
                                child2 = child;
                                REQUIRE(((start == 9 && end == 6) ||
                                         (start == 6 && end == 9) ));
                            }

                        }
                        chain_child_i ++;
                        REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                                == distance_index.canonical(top_chain_handle));

                        return true;
                    });
                    REQUIRE(chain_child_i == 5);

                    
                    SECTION("First child is from 1 end to 6 start") {
                        
                        {
                            handle_t start = distance_index.get_handle(distance_index.get_bound(child1, false, true), &graph);
                            handle_t end = distance_index.get_handle(distance_index.get_bound(child1, true, false), &graph);
                            
                            bool found_in_forward_orientation = (graph.get_id(start) == 1 &&
                                                                 graph.get_is_reverse(start) == false &&
                                                                 graph.get_id(end) == 6 &&
                                                                 graph.get_is_reverse(end) == false);
                            bool found_in_reverse_orientation = (graph.get_id(start) == 6 &&
                                                                 graph.get_is_reverse(start) == true &&
                                                                 graph.get_id(end) == 1 &&
                                                                 graph.get_is_reverse(end) == true);
                            bool found_snarl = found_in_forward_orientation || found_in_reverse_orientation;
                            REQUIRE(found_snarl);
                        }
                        SECTION( "Child1 can only traverse start to end") {
                            size_t traversal_count = 0;
                            distance_index.for_each_traversal(child1, [&](const net_handle_t net) {

                                bool start_end = distance_index.starts_at(net) == SnarlDecomposition::START &&
                                    distance_index.ends_at(net) == SnarlDecomposition::END;
                                bool end_start = distance_index.starts_at(net) == SnarlDecomposition::END &&
                                    distance_index.ends_at(net) == SnarlDecomposition::START;
                                REQUIRE((start_end || end_start));
                                traversal_count++;
                            });
                            REQUIRE(traversal_count == 2);
                        }
                        
                        SECTION("First child has a child from 2 end to 5 start") {
                            
                            size_t child_count = 0;
                            net_handle_t subchild;//Chain 2-5
                            distance_index.for_each_child(child1, [&](const net_handle_t& child) {
                                REQUIRE(distance_index.is_chain(child));
                                subchild = child;
                                child_count ++;
                                REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                                == distance_index.canonical(child1));
                                return true;
                            });
                            REQUIRE(child_count == 1);

                            {
                                handle_t start = distance_index.get_handle(distance_index.get_bound(subchild, false, true), &graph);
                                handle_t end = distance_index.get_handle(distance_index.get_bound(subchild, true, false), &graph);

                                bool found_in_forward_orientation = (graph.get_id(start) == 2 &&
                                                                     graph.get_is_reverse(start) == false &&
                                                                     graph.get_id(end) == 5 &&
                                                                     graph.get_is_reverse(end) == false);
                                bool found_in_reverse_orientation = (graph.get_id(start) == 5 &&
                                                                     graph.get_is_reverse(start) == true &&
                                                                     graph.get_id(end)== 2 &&
                                                                     graph.get_is_reverse(end) == true);
                                bool found_snarl = found_in_forward_orientation || found_in_reverse_orientation;
                                REQUIRE(found_snarl);
                            }

                            
                            SECTION("Subchild has no children") {
                                //Subchild is chain 2-5, which only has trivial children
                                size_t subchild_count = 0;
                                REQUIRE(distance_index.is_chain(subchild));
                                distance_index.for_each_child(subchild, [&](const net_handle_t& child) {
                                    REQUIRE((distance_index.is_node(child) || distance_index.is_snarl(child)));
                                    if (distance_index.is_snarl(child)){
                                        distance_index.for_each_child(child, [&](const net_handle_t& grandchild) {
                                            REQUIRE(distance_index.is_trivial_chain(grandchild));
                                        });
                                    }
                                    subchild_count ++;
                                    return true;
                                });
                                REQUIRE(subchild_count == 3);
                            }
                                
                        }
                        
                    }
                    
                    SECTION("Second child is from 6 end to 9 start") {
                        {
                            handle_t start = distance_index.get_handle(distance_index.get_bound(child2, false, true), &graph);
                            handle_t end = distance_index.get_handle(distance_index.get_bound(child2, true, false), &graph);
                            bool found_in_forward_orientation = (graph.get_id(start)== 6 &&
                                                                 graph.get_is_reverse(start) == false &&
                                                                 graph.get_id(end)== 9 &&
                                                                 graph.get_is_reverse(end) == false);
                            bool found_in_reverse_orientation = (graph.get_id(start)== 9 &&
                                                                 graph.get_is_reverse(start) == true &&
                                                                 graph.get_id(end)== 6 &&
                                                                 graph.get_is_reverse(end) == true);
                            bool found_snarl = found_in_forward_orientation || found_in_reverse_orientation;
                            REQUIRE(found_snarl);
                        }
                        
                        SECTION("Second child has no children") {
                            //Subchild is snarl 2-5, which only has trivial children
                            size_t child_count = 0;
                            distance_index.for_each_child(child2, [&](const net_handle_t& subchild) {
                                REQUIRE(distance_index.is_trivial_chain(subchild));
                                child_count ++;
                                return true;
                            });
                            REQUIRE(child_count == 2);
                        }
                    }
                    
                }
                
            }
        }

        TEST_CASE("Bubbles can be found in graphs with only heads", "[snarl_distance]") {
            
            // Build a toy graph
            const string graph_json = R"(
            
            {
                "node": [
                    {"id": 1, "sequence": "G"},
                    {"id": 2, "sequence": "A"}
                ],
                "edge": [
                    {"from": 1, "to": 2, "to_end": true}
                    
                ],
                "path": [
                    {"name": "hint", "mapping": [
                        {"position": {"node_id": 1}, "rank" : 1 },
                        {"position": {"node_id": 2, "is_reverse": true}, "rank" : 2 }
                    ]}
                ]
            }
            
            )";
            
            // Make an actual graph
            VG graph;
            Graph chunk;
            json2pb(chunk, graph_json.c_str(), graph_json.size());
            graph.extend(chunk);
            
            IntegratedSnarlFinder snarl_finder(graph); 
            SnarlDistanceIndex distance_index(&graph, &snarl_finder);

        
            SECTION("Root node has 1 child bubble") {
                //The root contains one connected component
                net_handle_t root_handle = distance_index.get_root();
                net_handle_t top_chain_handle;
                size_t component_count = 0;
                distance_index.for_each_child(root_handle, [&](const net_handle_t& child) {
                    component_count += 1;
                    top_chain_handle = child;
                });
                REQUIRE(component_count == 1);
                REQUIRE(distance_index.is_chain(top_chain_handle));

                net_handle_t child1;
                size_t chain_child_count = 0;
                distance_index.for_each_child(top_chain_handle, [&](const net_handle_t& child) {
                    chain_child_count++;
                    if (distance_index.is_snarl(child)) {
                        child1 = child;
                        handle_t start = distance_index.get_handle(distance_index.get_bound(child, false, true), &graph);
                        handle_t end = distance_index.get_handle(distance_index.get_bound(child, true, true), &graph);
                        if (graph.get_id(start) == 1) {
                            REQUIRE(graph.get_id(end) == 2);
                            REQUIRE(!graph.get_is_reverse(start));
                            REQUIRE(graph.get_is_reverse(end));
                        } else {
                            REQUIRE(graph.get_id(start) == 2);
                            REQUIRE(graph.get_id(end) == 1);
                            REQUIRE(graph.get_is_reverse(start));
                            REQUIRE(!graph.get_is_reverse(end));
                        }
                    } else {
                        REQUIRE(distance_index.is_node(child));
                    }
                });
                REQUIRE(chain_child_count == 2);
            }
        }


        TEST_CASE("Bubbles can be found in bigger graphs with only heads", "[snarl_distance]") {
            
            // Build a toy graph
            const string graph_json = R"(
            
            {
                "node": [
                    {"id": 1, "sequence": "G"},
                    {"id": 2, "sequence": "A"},
                    {"id": 3, "sequence": "T"},
                    {"id": 4, "sequence": "GGG"},
                    {"id": 5, "sequence": "T"},
                    {"id": 6, "sequence": "A"},
                    {"id": 7, "sequence": "C"},
                    {"id": 8, "sequence": "A"},
                    {"id": 9, "sequence": "T"}
                ],
                "edge": [
                    {"from": 1, "to": 2},
                    {"from": 1, "to": 6},
                    {"from": 2, "to": 3},
                    {"from": 2, "to": 4},
                    {"from": 3, "to": 5},
                    {"from": 4, "to": 5},
                    {"from": 5, "to": 6},
                    {"from": 6, "to": 7},
                    {"from": 6, "to": 8},
                    {"from": 7, "to": 9, "to_end": true},
                    {"from": 8, "to": 9, "to_end": true}
                    
                ],
                "path": [
                    {"name": "hint", "mapping": [
                        {"position": {"node_id": 1}, "rank" : 1 },
                        {"position": {"node_id": 6}, "rank" : 2 },
                        {"position": {"node_id": 8}, "rank" : 3 },
                        {"position": {"node_id": 9, "is_reverse": true}, "rank" : 4 }
                    ]}
                ]
            }
            
            )";
            
            // Make an actual graph
            VG graph;
            Graph chunk;
            json2pb(chunk, graph_json.c_str(), graph_json.size());
            graph.extend(chunk);
            
            IntegratedSnarlFinder snarl_finder(graph); 
            SnarlDistanceIndex distance_index(&graph, &snarl_finder);
            
            SECTION("Root node has 2 child bubbles") {

                net_handle_t root_handle = distance_index.get_root();
                net_handle_t top_chain_handle;
                size_t component_count = 0;
                distance_index.for_each_child(root_handle, [&](const net_handle_t& child) {
                    component_count += 1;
                    top_chain_handle = child;
                });
                REQUIRE(component_count == 1);
                REQUIRE(distance_index.is_chain(top_chain_handle));


                //The top connected component is a chain with one snarl
                size_t child_i = 0;
                distance_index.for_each_child(top_chain_handle, [&](const net_handle_t& child) {
                    if (child_i == 0 || child_i == 4) {
                        //start or end of chain
                        REQUIRE(distance_index.is_node(child));
                    } else if (child_i == 1 || child_i == 3) {
                        REQUIRE(distance_index.is_snarl(child));
                        handle_t start = distance_index.get_handle(distance_index.get_bound(child, false, true), &graph);
                        handle_t end = distance_index.get_handle(distance_index.get_bound(child, true, true), &graph);
                        if (graph.get_id(start) == 1){
                            REQUIRE(graph.get_id(end) == 6);
                            REQUIRE(!graph.get_is_reverse(start));
                            REQUIRE(graph.get_is_reverse(end));
                        } else if (graph.get_id(end) == 1) {
                            REQUIRE(graph.get_id(start) == 6);
                            REQUIRE(graph.get_is_reverse(start));
                            REQUIRE(!graph.get_is_reverse(end));
                        } else if (graph.get_id(start) == 9) {
                            REQUIRE(graph.get_id(end) == 6);
                            REQUIRE(!graph.get_is_reverse(start));
                            REQUIRE(!graph.get_is_reverse(end));
                        } else if (graph.get_id(end) == 9){
                            REQUIRE(graph.get_id(start) == 6);
                            REQUIRE(!graph.get_is_reverse(start));
                            REQUIRE(!graph.get_is_reverse(end));
                        }

                    } else {
                        REQUIRE(distance_index.is_node(child));
                        REQUIRE(graph.get_id(distance_index.get_handle(child, &graph)) == 6);
                    }
                    child_i++;
                });
                REQUIRE(child_i == 5);
            }
        }

        TEST_CASE("Bubbles can be found in graphs with only tails", "[snarl_distance]") {
            
            // Build a toy graph
            const string graph_json = R"(
            
            {
                "node": [
                    {"id": 1, "sequence": "C"},
                    {"id": 2, "sequence": "A"},
                    {"id": 3, "sequence": "T"},
                    {"id": 4, "sequence": "GGG"},
                    {"id": 5, "sequence": "T"},
                    {"id": 6, "sequence": "A"},
                    {"id": 7, "sequence": "C"},
                    {"id": 8, "sequence": "A"},
                    {"id": 9, "sequence": "A"}
                ],
                "edge": [
                    {"from": 1, "to": 2, "from_start": true},
                    {"from": 1, "to": 6, "from_start": true},
                    {"from": 2, "to": 3},
                    {"from": 2, "to": 4},
                    {"from": 3, "to": 5},
                    {"from": 4, "to": 5},
                    {"from": 5, "to": 6},
                    {"from": 6, "to": 7},
                    {"from": 6, "to": 8},
                    {"from": 7, "to": 9},
                    {"from": 8, "to": 9}
                    
                ],
                "path": [
                    {"name": "hint", "mapping": [
                        {"position": {"node_id": 1, "is_reverse": true}, "rank" : 1 },
                        {"position": {"node_id": 6}, "rank" : 2 },
                        {"position": {"node_id": 8}, "rank" : 3 },
                        {"position": {"node_id": 9}, "rank" : 4 }
                    ]}
                ]
            }
            
            )";
            
            // Make an actual graph
            VG graph;
            Graph chunk;
            json2pb(chunk, graph_json.c_str(), graph_json.size());
            graph.extend(chunk);
         
            IntegratedSnarlFinder snarl_finder(graph); 
            SnarlDistanceIndex distance_index(&graph, &snarl_finder);
            
            SECTION("Root node has 2 child bubbles") {

                net_handle_t root_handle = distance_index.get_root();
                net_handle_t top_chain_handle;
                size_t component_count = 0;
                distance_index.for_each_child(root_handle, [&](const net_handle_t& child) {
                    component_count += 1;
                    top_chain_handle = child;
                });
                REQUIRE(component_count == 1);
                REQUIRE(distance_index.is_chain(top_chain_handle));


                //The top connected component is a chain with one snarl
                size_t child_i = 0;
                distance_index.for_each_child(top_chain_handle, [&](const net_handle_t& child) {
                    if (child_i == 0 || child_i == 4) {
                        //start or end of chain
                        REQUIRE(distance_index.is_node(child));
                    } else if (child_i == 1 || child_i == 3) {
                        REQUIRE(distance_index.is_snarl(child));
                        handle_t start = distance_index.get_handle(distance_index.get_bound(child, false, true), &graph);
                        handle_t end = distance_index.get_handle(distance_index.get_bound(child, true, true), &graph);
                        if (graph.get_id(start) == 1){
                            REQUIRE(graph.get_id(end) == 6);
                            REQUIRE(graph.get_is_reverse(start));
                            REQUIRE(graph.get_is_reverse(end));
                        } else if (graph.get_id(end) == 1) {
                            REQUIRE(graph.get_id(start) == 6);
                            REQUIRE(graph.get_is_reverse(start));
                            REQUIRE(graph.get_is_reverse(end));
                        } else if (graph.get_id(start) == 9) {
                            REQUIRE(graph.get_id(end) == 6);
                            REQUIRE(graph.get_is_reverse(start));
                            REQUIRE(!graph.get_is_reverse(end));
                        } else if (graph.get_id(end) == 9){
                            REQUIRE(graph.get_id(start) == 6);
                            REQUIRE(!graph.get_is_reverse(start));
                            REQUIRE(graph.get_is_reverse(end));
                        }

                    } else {
                        REQUIRE(distance_index.is_node(child));
                        REQUIRE(graph.get_id(distance_index.get_handle(child, &graph)) == 6);
                    }
                    child_i++;
                });
                REQUIRE(child_i == 5);
            }
        }

        TEST_CASE("Bubbles can be found when heads cannot reach tails", "[snarl_distance]") {
            
            // Build a toy graph
            // Looks like:
            //
            //    1\
            //  2---3
            //  v\4 v
            //
            // The head is 1, the tail is 4, 2 and 3 have self loops, and the graph is connected.
            // But the head cannot reach the tail.
            const string graph_json = R"(
            
            {
                "node": [
                    {"id": 1, "sequence": "A"},
                    {"id": 2, "sequence": "A"},
                    {"id": 3, "sequence": "A"},
                    {"id": 4, "sequence": "A"}
                ],
                "edge": [
                    {"from": 1, "to": 3},
                    {"from": 2, "to": 4},
                    {"from": 2, "to": 3},
                    {"from": 2, "to": 2},
                    {"from": 3, "to": 3}            
                ]
            }
            
            )";
            
            // Make an actual graph
            VG graph;
            Graph chunk;
            json2pb(chunk, graph_json.c_str(), graph_json.size());
            graph.extend(chunk);
            
            IntegratedSnarlFinder snarl_finder(graph); 
            SnarlDistanceIndex distance_index(&graph, &snarl_finder);
           
            SECTION("Root should have one child actual bubble") {

                net_handle_t root_handle = distance_index.get_root();
                net_handle_t top_chain_handle;
                size_t component_count = 0;
                distance_index.for_each_child(root_handle, [&](const net_handle_t& child) {
                    component_count += 1;
                    top_chain_handle = child;
                });
                REQUIRE(component_count == 1);
                REQUIRE(distance_index.is_chain(top_chain_handle));


                //The top connected component is a chain with one snarl
                size_t child_i = 0;
                distance_index.for_each_child(top_chain_handle, [&](const net_handle_t& child) {
                    if (distance_index.is_snarl(child)) {
                        REQUIRE(distance_index.is_snarl(child));
                        size_t grandchild_count = 0;
                        distance_index.for_each_child(child, [&](const net_handle_t& grandchild) {
                            REQUIRE(distance_index.is_trivial_chain(grandchild));
                            grandchild_count++;
                        });
                        REQUIRE(grandchild_count == 2);
                    }
                    child_i++;
                });
                REQUIRE(child_i == 3);
            }
        }
        /*
         * TODO: I think this would be a looping chain and I dont know how to deal with it yet

        TEST_CASE("bubbles can be found in a graph with no heads or tails", "[bubbles][snarls]") {
            
            // Build a toy graph
            const string graph_json = R"(
            
            {
                "node": [
                    {"id": 1, "sequence": "G"},
                    {"id": 2, "sequence": "A"}
                ],
                "edge": [
                    {"from": 1, "to": 2},
                    {"from": 2, "to": 1}
                    
                ],
                "path": [
                    {"name": "hint", "mapping": [
                        {"position": {"node_id": 1}, "rank" : 1 },
                        {"position": {"node_id": 2}, "rank" : 2 }
                    ]}
                ]
            }
            
            )";
            
            // Make an actual graph
            VG graph;
            Graph chunk;
            json2pb(chunk, graph_json.c_str(), graph_json.size());
            graph.extend(chunk);
            
            SnarlManager snarl_manager = CactusSnarlFinder(graph).find_snarls();
            
#ifdef debug
            snarl_manager.for_each_snarl_preorder([&](const Snarl* snarl) {
                cerr << "Found snarl " << snarl->start().node_id() << " " << snarl->start().backward()
                    << " to " << snarl->end().node_id() << " " << snarl->end().backward() << " containing ";
                for (auto& node : snarl_manager.shallow_contents(snarl, graph, false).first) {
                    cerr << node->id() << " ";
                }
                cerr << endl;
            });
#endif
            
            SECTION("Root node has 2 child bubbles") {
                REQUIRE(snarl_manager.top_level_snarls().size() == 2);
                
                const Snarl* child1 = snarl_manager.top_level_snarls()[0];
                const Snarl* child2 = snarl_manager.top_level_snarls()[1];
                
                if (child1->start().node_id() > child1->end().node_id()) {
                    snarl_manager.flip(child1);
                }
                
                if (child2->start().node_id() > child2->end().node_id()) {
                    snarl_manager.flip(child2);
                }
                
                SECTION("First child is from 1 start/end to 2 end/start") {
                    REQUIRE(child1->start().node_id() == 1);
                    REQUIRE(child1->end().node_id() == 2);
                    REQUIRE(!child1->start().backward() != child1->end().backward());
                    
                    SECTION("First child has no children") {
                        REQUIRE(snarl_manager.children_of(child1).size() == 0);
                    }
                    
                }
                
                SECTION("Second child is from 1 start/end to 2 end/start") {
                    REQUIRE(child2->start().node_id() == 1);
                    REQUIRE(child2->end().node_id() == 2);
                    REQUIRE(!child1->start().backward() != child1->end().backward());
                    
                    SECTION("Second child has no children") {
                        REQUIRE(snarl_manager.children_of(child2).size() == 0);
                    }
                    
                }
                
            }
            
        }

        TEST_CASE("bubbles can be found in a graph with no heads or tails or paths", "[bubbles]") {
            
            // Build a toy graph
            const string graph_json = R"(
            
            {
                "node": [
                    {"id": 1, "sequence": "G"},
                    {"id": 2, "sequence": "A"}
                ],
                "edge": [
                    {"from": 1, "to": 2},
                    {"from": 2, "to": 1}
                    
                ]
            }
            
            )";
            
            // Make an actual graph
            VG graph;
            Graph chunk;
            json2pb(chunk, graph_json.c_str(), graph_json.size());
            graph.extend(chunk);
            
            SnarlManager snarl_manager = CactusSnarlFinder(graph).find_snarls();
            
#ifdef debug
            snarl_manager.for_each_snarl_preorder([&](const Snarl* snarl) {
                cerr << "Found snarl " << snarl->start().node_id() << " " << snarl->start().backward()
                    << " to " << snarl->end().node_id() << " " << snarl->end().backward() << " containing ";
                for (auto& node : snarl_manager.shallow_contents(snarl, graph, false).first) {
                    cerr << node->id() << " ";
                }
                cerr << endl;
            });
#endif
            
            SECTION("Root node has 2 child bubbles") {
                REQUIRE(snarl_manager.top_level_snarls().size() == 2);
                
                // We should have 2 bubbles, one looking around the cycle in each direction.
                // TODO: can't really say much about its contents.
                
            }
            
        }
        */

        TEST_CASE("Bubbles are created based on most distant connected tips", "[snarl_distance]") {
            
            // Build a toy graph
            // Looks like:
            //
            //       1\
            //  5--2---3--6
            //      \4  
            //
            // The head is 1, the tail is 4, 2 and 3 have self loops, and the graph is connected.
            // But the head cannot reach the tail.
            const string graph_json = R"(
            
            {
                "node": [
                    {"id": 1, "sequence": "A"},
                    {"id": 2, "sequence": "A"},
                    {"id": 3, "sequence": "A"},
                    {"id": 4, "sequence": "A"},
                    {"id": 5, "sequence": "A"},
                    {"id": 6, "sequence": "A"}
                ],
                "edge": [
                    {"from": 1, "to": 3},
                    {"from": 2, "to": 4},
                    {"from": 2, "to": 3},
                    {"from": 5, "to": 2},
                    {"from": 3, "to": 6}
                ]
            }
            
            )";
            
            // Make an actual graph
            VG graph;
            Graph chunk;
            json2pb(chunk, graph_json.c_str(), graph_json.size());
            graph.extend(chunk);
            
            IntegratedSnarlFinder snarl_finder(graph); 
            SnarlDistanceIndex distance_index(&graph, &snarl_finder);
           
            
                
            SECTION("Root should have three child actual bubbles") {


                net_handle_t root_handle = distance_index.get_root();
                net_handle_t top_chain_handle;
                size_t component_count = 0;
                distance_index.for_each_child(root_handle, [&](const net_handle_t& child) {
                    component_count += 1;
                    top_chain_handle = child;
                });
                REQUIRE(component_count == 1);
                REQUIRE(distance_index.is_chain(top_chain_handle));


                //The top connected component is a chain with one snarl
                size_t child_i = 0;
                distance_index.for_each_child(top_chain_handle, [&](const net_handle_t& child) {
                    if (distance_index.is_snarl(child)) {
                        REQUIRE(distance_index.is_snarl(child));
                        size_t grandchild_count = 0;
                        distance_index.for_each_child(child, [&](const net_handle_t& grandchild) {
                            REQUIRE(distance_index.is_trivial_chain(grandchild));
                            grandchild_count++;
                        });
                        REQUIRE(grandchild_count == 2);
                    } else {
                        REQUIRE(distance_index.is_node(child));
                        id_t id = graph.get_id(distance_index.get_handle(child, &graph));
                        REQUIRE((id == 5 || id == 2 || id == 3 || id == 6));
                    }
                    child_i++;
                });
                REQUIRE(child_i == 5);
            }
        }
        
        
        TEST_CASE("DistanceIndex exposes chains correctly", "[snarl_distance]") {
            
            // We need a graph a chain in it.
            // Looks like:
            //
            //    2   5   8
            //  1   4   7   10 
            //    3   6   9
            //
            const string graph_json = R"(
            
            {
                "node": [
                    {"id": 1, "sequence": "A"},
                    {"id": 2, "sequence": "A"},
                    {"id": 3, "sequence": "A"},
                    {"id": 4, "sequence": "A"},
                    {"id": 5, "sequence": "A"},
                    {"id": 6, "sequence": "A"},
                    {"id": 7, "sequence": "A"},
                    {"id": 8, "sequence": "A"},
                    {"id": 9, "sequence": "A"},
                    {"id": 10, "sequence": "A"}
                ],
                "edge": [
                    {"from": 1, "to": 2},
                    {"from": 1, "to": 3},
                    {"from": 2, "to": 4},
                    {"from": 3, "to": 4},
                    {"from": 4, "to": 5},
                    {"from": 4, "to": 6},
                    {"from": 5, "to": 7},
                    {"from": 6, "to": 7},
                    {"from": 7, "to": 8},
                    {"from": 7, "to": 9},
                    {"from": 8, "to": 10},
                    {"from": 9, "to": 10}
                ]
            }
            
            )";
            
            // Make an actual graph
            VG graph;
            Graph chunk;
            json2pb(chunk, graph_json.c_str(), graph_json.size());
            graph.extend(chunk);
            
            IntegratedSnarlFinder snarl_finder(graph); 
            SnarlDistanceIndex distance_index(&graph, &snarl_finder);
            
                
            SECTION("There should be three top-level snarls") {


                net_handle_t root_handle = distance_index.get_root();
                net_handle_t top_chain_handle;
                size_t component_count = 0;
                distance_index.for_each_child(root_handle, [&](const net_handle_t& child) {
                    component_count += 1;
                    top_chain_handle = child;
                });
                REQUIRE(component_count == 1);
                REQUIRE(distance_index.is_chain(top_chain_handle));


                //The top connected component is a chain with one snarl
                size_t child_i = 0;
                vector<net_handle_t> top_snarls;
                distance_index.for_each_child(top_chain_handle, [&](const net_handle_t& child) {
                    REQUIRE( distance_index.canonical(distance_index.get_parent(child))
                                == distance_index.canonical(top_chain_handle));
                    if (child_i % 2 == 1) {
                        REQUIRE(distance_index.is_snarl(child));
                        top_snarls.emplace_back(child);
                    } else {
                        REQUIRE(distance_index.is_node(child));
                    } 
                    child_i++;
                });
                REQUIRE(child_i == 7);

                SECTION("They should be in a chain") {
                    for (net_handle_t& snarl : top_snarls) {
                        REQUIRE( distance_index.is_chain(distance_index.get_parent(snarl)));
                    }
                }
                
                SECTION("They should be in the same chain") {
                    for (net_handle_t& snarl : top_snarls) {
                        REQUIRE( distance_index.canonical(distance_index.get_parent(snarl))
                                == distance_index.canonical(top_chain_handle));
                    }
                }
                
            }
            
        }
        
        TEST_CASE("chain start and end functions work on difficult chains", "[snarl_distance]") {
            // This graph will have a snarl from 1 to 8, a snarl from 2 to 4, and a
            // snarl from 4 to 7, with a chain in the top snarl. The snarl from 4 to 7
            // will have a child snarl from 5 to 6 (an insertion of node 9)
            VG graph;
                
            Node* n1 = graph.create_node("GCA");
            Node* n2 = graph.create_node("T");
            Node* n3 = graph.create_node("G");
            Node* n4 = graph.create_node("CTGA");
            Node* n5 = graph.create_node("GCA");
            Node* n6 = graph.create_node("T");
            Node* n7 = graph.create_node("G");
            Node* n8 = graph.create_node("CTGA");
            Node* n9 = graph.create_node("GCA");
            
            Edge* e1 = graph.create_edge(n1, n2);
            Edge* e2 = graph.create_edge(n1, n8);
            Edge* e3 = graph.create_edge(n2, n3);
            Edge* e4 = graph.create_edge(n2, n4);
            Edge* e5 = graph.create_edge(n3, n4);
            Edge* e6 = graph.create_edge(n4, n5);
            Edge* e7 = graph.create_edge(n4, n7);
            Edge* e8 = graph.create_edge(n5, n6);
            Edge* e9 = graph.create_edge(n5, n9);
            Edge* e10 = graph.create_edge(n9, n6);
            Edge* e11 = graph.create_edge(n6, n7);
            Edge* e12 = graph.create_edge(n7, n8);
            
            // Work out its snarls
            IntegratedSnarlFinder snarl_finder(graph); 
            SnarlDistanceIndex distance_index(&graph, &snarl_finder);
                
            //The root contains one connected component
            net_handle_t root_handle = distance_index.get_root();
            net_handle_t top_chain_handle;
            size_t component_count = 0;
            distance_index.for_each_child(root_handle, [&](const net_handle_t& child) {
                component_count += 1;
                REQUIRE(distance_index.is_chain(child));
                top_chain_handle = child;
            });
            REQUIRE(component_count == 1);
            
            size_t top_child_count = 0;
            net_handle_t top_snarl;
            distance_index.for_each_child(top_chain_handle, [&](const net_handle_t& child) {
                top_child_count += 1;
                if (distance_index.is_snarl(child)){
                    top_snarl = child;
                } else {
                    REQUIRE(distance_index.is_node(child));
                    id_t id = graph.get_id(distance_index.get_handle(child, &graph));
                    REQUIRE((id == 1 || id == 8));
                }
            });
            REQUIRE(top_child_count == 3);
            id_t start_id = graph.get_id(distance_index.get_handle(distance_index.get_bound(top_snarl, false, false), &graph));
            id_t end_id = graph.get_id(distance_index.get_handle(distance_index.get_bound(top_snarl, true, false), &graph));
            REQUIRE((start_id == 1 || start_id == 8));
            REQUIRE((end_id == 1 || end_id == 8));
            REQUIRE(start_id != end_id);

            size_t snarl_child_count = 0;
            net_handle_t child_chain;
            distance_index.for_each_child(top_snarl, [&](const net_handle_t& child) {
                snarl_child_count++;
                REQUIRE(distance_index.is_chain(child));
                REQUIRE(!distance_index.is_trivial_chain(child));
                child_chain = child;
            });
            REQUIRE(snarl_child_count == 1);
            // Make sure we have one chain    
            
            net_handle_t snarl1;
            net_handle_t snarl2;
            size_t chain_child_count = 0;
            distance_index.for_each_child(child_chain, [&](const net_handle_t& child) {
                if (distance_index.is_snarl(child)) {
                    id_t start = graph.get_id(distance_index.get_handle(distance_index.get_bound(child, false, false), &graph));
                    id_t end = graph.get_id(distance_index.get_handle(distance_index.get_bound(child, true, false), &graph));
                    if ((start == 2 && end == 4) || (start == 4 && end == 2)){
                        snarl1 = child;
                    } else {
                        REQUIRE(((start == 7 && end == 4) || (start == 4 && end == 7)));
                        snarl2 = child;
                    }
                } else {
                    REQUIRE(distance_index.is_node(child));
                }
                chain_child_count++;
            });
            REQUIRE(chain_child_count == 5);

            
            SECTION("A chain can be found from a member snarl") {
                REQUIRE(distance_index.canonical(distance_index.get_parent(snarl1)) == 
                        distance_index.canonical(child_chain));
                REQUIRE(distance_index.canonical(distance_index.get_parent(snarl2)) == 
                        distance_index.canonical(child_chain));
                
            }
            
        }
        /*
         * TODO: I think this one will also be a looping chain


        TEST_CASE( "Snarls can be found for a graph with no ordinary cycles", "[snarls]" ) {
            VG graph;
                
            // We have this dumbell-shaped graph, where you have to break open
            // a cycle but just saying you go from a node to itself isn't a
            // valid snarl.
            
            Node* n1 = graph.create_node("A");
            Node* n2 = graph.create_node("G");
            
            Edge* e1 = graph.create_edge(n1, n2);
            Edge* e2 = graph.create_edge(n1, n1, true, false);
            Edge* e3 = graph.create_edge(n2, n2, false, true);
            
            CactusSnarlFinder bubble_finder(graph);
            SnarlManager snarl_manager = bubble_finder.find_snarls();
            
            // There must be something in the top level snarls
            REQUIRE(!snarl_manager.top_level_snarls().empty());
            
            // The decomposition must cover all the nodes
            unordered_set<id_t> seen_nodes;
            
            snarl_manager.for_each_snarl_preorder([&](const Snarl* snarl) {
                // Get the contents of each snarl
                pair<unordered_set<Node*>, unordered_set<Edge*> > contents = pb_contents(graph, snarl_manager.shallow_contents(snarl, graph, true));
            
                for (auto& node_ptr : contents.first) {
                    // And record all the nodes
                    seen_nodes.insert(node_ptr->id());
                }
            });
            
            // Make sure both nodes appear.
            REQUIRE(seen_nodes.size() == 2);
            
            
        }
        
        TEST_CASE( "Snarls can be found for a bigger graph with no ordinary cycles", "[snarls]" ) {
            VG graph;
                
            Node* n1 = graph.create_node("GCA");
            Node* n2 = graph.create_node("T");
            Node* n3 = graph.create_node("G");
            Node* n4 = graph.create_node("CTGA");
            Node* n5 = graph.create_node("GCA");
            
            Edge* e1 = graph.create_edge(n1, n2);
            Edge* e2 = graph.create_edge(n1, n2, true, false);
            Edge* e3 = graph.create_edge(n2, n3);
            Edge* e4 = graph.create_edge(n3, n4);
            Edge* e5 = graph.create_edge(n3, n5);
            Edge* e6 = graph.create_edge(n4, n5);
            Edge* e7 = graph.create_edge(n5, n3, false, true);
            
            CactusSnarlFinder bubble_finder(graph);
            SnarlManager snarl_manager = bubble_finder.find_snarls();
            
            // There must be something in the top level snarls
            REQUIRE(!snarl_manager.top_level_snarls().empty());
            
            // The decomposition must cover all the nodes
            unordered_set<id_t> seen_nodes;
            
            snarl_manager.for_each_snarl_preorder([&](const Snarl* snarl) {
                // Get the contents of each snarl
                pair<unordered_set<Node*>, unordered_set<Edge*> > contents = pb_contents(graph, snarl_manager.shallow_contents(snarl, graph, true));
            
                for (auto& node_ptr : contents.first) {
                    // And record all the nodes
                    seen_nodes.insert(node_ptr->id());
                }
            });
            
            // Make sure all nodes appear.
            REQUIRE(seen_nodes.size() == 5);
            
            
        }

        TEST_CASE( "NetGraph can traverse looping snarls",
                  "[snarl_distance]" ) {
        
            VG graph;
                
            Node* n1 = graph.create_node("GCA");
            Node* n2 = graph.create_node("T");
            Node* n3 = graph.create_node("G");
            Node* n4 = graph.create_node("CTGA");
            Node* n5 = graph.create_node("GCA");
            
            Edge* e1 = graph.create_edge(n1, n2);
            Edge* e2 = graph.create_edge(n1, n2, true, false);
            Edge* e3 = graph.create_edge(n2, n3);
            Edge* e4 = graph.create_edge(n3, n4);
            Edge* e5 = graph.create_edge(n3, n5);
            Edge* e6 = graph.create_edge(n4, n5);
            Edge* e7 = graph.create_edge(n5, n3, false, true);
            
            // Define the snarls for the top level
            
            // This test depends on the snarl decomposition being rooted at
            // node 1. So we force that to happen by specifying the snarls
            // manually.
            
            vector<Snarl> to_manage;
            
            // We only need the top snarl and its direct child for this test.
            to_manage.emplace_back();
            to_manage.back().mutable_start()->set_node_id(1);
            to_manage.back().mutable_start()->set_backward(true);
            to_manage.back().mutable_end()->set_node_id(1);
            to_manage.back().mutable_end()->set_backward(true);
            to_manage.back().set_type(SnarlType::UNCLASSIFIED);
            to_manage.back().set_start_self_reachable(true);
            to_manage.back().set_end_self_reachable(true);
            to_manage.back().set_start_end_reachable(true);
            
            
            to_manage.emplace_back();
            to_manage.back().mutable_start()->set_node_id(2);
            to_manage.back().mutable_start()->set_backward(false);
            to_manage.back().mutable_end()->set_node_id(2);
            to_manage.back().mutable_end()->set_backward(true);
            to_manage.back().set_type(SnarlType::UNARY);
            to_manage.back().set_start_self_reachable(true);
            to_manage.back().set_end_self_reachable(true);
            to_manage.back().set_start_end_reachable(true);
            to_manage.back().mutable_parent()->mutable_start()->set_node_id(1);
            to_manage.back().mutable_parent()->mutable_start()->set_backward(true);
            to_manage.back().mutable_parent()->mutable_end()->set_node_id(1);
            to_manage.back().mutable_parent()->mutable_end()->set_backward(true);
            
           
            SnarlManager snarl_manager(to_manage.begin(), to_manage.end());

            const vector<const Snarl*>& snarls = snarl_manager.top_level_snarls();
            
            const Snarl* topSnarl = snarls[0];
            //top level snarl starting and ending at node 1 in reverse - 
            //Not a unary snarl but starts and ends at opposite sides of the same node  
            
            
            SECTION( "Traverse unary snarl in netgraph with internal connectivity" ) {

                // Make a net graph - contains two nodes - node1 and the unary 
                // snarl at node2. node1 has two edges to the beginning of node2
                // Top snarl starts at node1 pointing left and ends at node1 
                // pointing left
                // 

                NetGraph ng(topSnarl->start(), topSnarl->end(), snarl_manager.chains_of(topSnarl), &graph, true);
  
                handle_t startHandle = ng.get_handle(topSnarl->start().node_id()
                          , topSnarl->start().backward());
                handle_t startHandleR = ng.get_handle(
                    topSnarl->start().node_id(), !topSnarl->start().backward());


                const vector<const Snarl*>& children= snarl_manager.children_of(
                                                                   topSnarl);
                //One child snarl starting at node 2 forward
                const Snarl* childSnarl = children.at(0);
                pair<id_t, bool> childNode (childSnarl->start().node_id(),
                                            childSnarl->start().backward());

 
                pair<id_t, bool> nextFd;
                auto getNextFd = [&](const handle_t& h) -> bool {
                    nextFd.first =  ng.get_id(h); 
                    nextFd.second = ng.get_is_reverse(h);
                    return true;
                };          

                ng.follow_edges(startHandle, false, getNextFd);
              
                //Following edges going forward from start will find child node
                REQUIRE(nextFd == childNode);

                pair<id_t, bool> nextRev;
                auto getNextRev = [&](const handle_t& h) -> bool {
                    nextRev.first =  ng.get_id(h);
                    nextRev.second = ng.get_is_reverse(h);
                    return true;
                };          

                //Following edges going backward from start will find child node
                ng.follow_edges(startHandleR, false, getNextRev);
                REQUIRE(nextRev == childNode);

 
                unordered_set<id_t> allHandles;
                auto addHandle = [&](const handle_t& h) -> bool {
                    allHandles.insert(ng.get_id(h));
                    return true;
                };          
                ng.for_each_handle(addHandle);                   


                //for_each_handle finds both nodes in net graph
                REQUIRE(allHandles.size() == 2);
                REQUIRE(allHandles.count(childSnarl->start().node_id()) == 1);


                //Check following edges from child snarl
                handle_t childFd = ng.get_handle(childNode.first, childNode.second);
                handle_t childRev = ng.get_handle(childNode.first, !childNode.second);

                //Following edges going fd from child node will find start
                unordered_set<pair<id_t, bool>> seenFd;

                auto childNextFd = [&](const handle_t& h) -> bool {
                    seenFd.insert(make_pair( ng.get_id(h), 
                                             ng.get_is_reverse(h)));
                    return true;
                };          
                ng.follow_edges(childFd, false, childNextFd);
                REQUIRE(seenFd.size() == 2);
                REQUIRE(seenFd.count(make_pair(topSnarl->start().node_id(),
                                           topSnarl->start().backward())) == 1);
                REQUIRE(seenFd.count(make_pair(topSnarl->start().node_id(),
                                          !topSnarl->start().backward())) == 1);
                
                //Following edges going back from child node will find nothing
                unordered_set<pair<id_t, bool>> seenRev;

                auto childNextRev = [&](const handle_t& h) -> bool {
                    seenRev.insert(make_pair( ng.get_id(h), 
                                             ng.get_is_reverse(h)));
                    return true;
                };          
                ng.follow_edges(childRev, false, childNextRev);
                REQUIRE(seenRev.size() == 0);
                
    
                //Following edges fd from child going left (predecessors) finds nothing 
                unordered_set<pair<id_t, bool>> seenFdPred;
                ng.follow_edges(childFd, true, [&](const handle_t& other) {
                    seenFdPred.insert(make_pair(ng.get_id(other), 
                                             ng.get_is_reverse(other)));
                });
                REQUIRE(seenFdPred.size() == 0);

                //Following edges rev from child going left (predecessors) finds nothing 
                unordered_set<pair<id_t, bool>> seenRevPred;
                ng.follow_edges(childRev, true, [&](const handle_t& other) {
                    seenRevPred.insert(make_pair(ng.get_id(other), 
                                             ng.get_is_reverse(other)));
                });
                REQUIRE(seenRevPred.size() == 2);
                REQUIRE(seenRevPred.count(make_pair(topSnarl->start().node_id(),
                                           topSnarl->start().backward())) == 1);
                REQUIRE(seenRevPred.count(make_pair(topSnarl->start().node_id(),
                                          !topSnarl->start().backward())) == 1);

            }
            SECTION( "Traverse unary snarl in netgraph without internal connectivity" ) {

                // Make a net graph - contains two nodes - node1 and the unary 
                // snarl at node2. node1 has two edges to the beginning of node2
                // Top snarl starts at node1 pointing left and ends at node1 
                // pointing left
                // 

                NetGraph ng(topSnarl->start(), topSnarl->end(), snarl_manager.chains_of(topSnarl), &graph);
  
                handle_t startHandle = ng.get_handle(topSnarl->start().node_id()
                          , topSnarl->start().backward());
                handle_t startHandleR = ng.get_handle(
                    topSnarl->start().node_id(), !topSnarl->start().backward());


                const vector<const Snarl*>& children= snarl_manager.children_of(
                                                                   topSnarl);
                //One child snarl starting at node 2 forward
                const Snarl* childSnarl = children.at(0);
                pair<id_t, bool> childNode (childSnarl->start().node_id(),
                                            childSnarl->start().backward());

 
                pair<id_t, bool> nextFd;
                auto getNextFd = [&](const handle_t& h) -> bool {
                    nextFd.first =  ng.get_id(h); 
                    nextFd.second = ng.get_is_reverse(h);
                    return true;
                };          

                ng.follow_edges(startHandle, false, getNextFd);
              
                //Following edges going forward from start will find child node
                REQUIRE(nextFd == childNode);

                pair<id_t, bool> nextRev;
                auto getNextRev = [&](const handle_t& h) -> bool {
                    nextRev.first =  ng.get_id(h);
                    nextRev.second = ng.get_is_reverse(h);
                    return true;
                };          

                //Following edges going backward from start will find child node
                ng.follow_edges(startHandleR, false, getNextRev);
                REQUIRE(nextRev == childNode);

 
                unordered_set<id_t> allHandles;
                auto addHandle = [&](const handle_t& h) -> bool {
                    allHandles.insert(ng.get_id(h));
                    return true;
                };          
                ng.for_each_handle(addHandle);                   


                //for_each_handle finds both nodes in net graph
                REQUIRE(allHandles.size() == 2);
                REQUIRE(allHandles.count(childSnarl->start().node_id()) == 1);


                //Check following edges from child snarl
                handle_t childFd = ng.get_handle(childNode.first, childNode.second);
                handle_t childRev = ng.get_handle(childNode.first, !childNode.second);


                //Following edges going fd from child node will find start
                unordered_set<pair<id_t, bool>> seenFd;

                auto childNextFd = [&](const handle_t& h) -> bool {
                    seenFd.insert(make_pair( ng.get_id(h), 
                                             ng.get_is_reverse(h)));
                    return true;
                };          
                ng.follow_edges(childFd, false, childNextFd);
                REQUIRE(seenFd.size() == 0);
                
                //Following edges going back from child node will find start
                unordered_set<pair<id_t, bool>> seenRev;

                auto childNextRev = [&](const handle_t& h) -> bool {
                    seenRev.insert(make_pair( ng.get_id(h), 
                                             ng.get_is_reverse(h)));
                    return true;
                };          
                ng.follow_edges(childRev, false, childNextRev);
                REQUIRE(seenRev.size() == 2);
                REQUIRE(seenRev.count(make_pair(topSnarl->start().node_id(),
                                           topSnarl->start().backward())) == 1);
                REQUIRE(seenRev.count(make_pair(topSnarl->start().node_id(),
                                          !topSnarl->start().backward())) == 1);
    
                //Following edges fd from child going left (predecessors) finds start 
                unordered_set<pair<id_t, bool>> seenFdPred;
                ng.follow_edges(childFd, true, [&](const handle_t& other) {
                    seenFdPred.insert(make_pair(ng.get_id(other), 
                                             ng.get_is_reverse(other)));
                });
                REQUIRE(seenFdPred.size() == 2);
                REQUIRE(seenFdPred.count(make_pair(topSnarl->start().node_id(),
                                           topSnarl->start().backward())) == 1);
                REQUIRE(seenFdPred.count(make_pair(topSnarl->start().node_id(),
                                          !topSnarl->start().backward())) == 1);



                //Following edges rev from child going left (predecessors) finds nothing 
                unordered_set<pair<id_t, bool>> seenRevPred;
                ng.follow_edges(childRev, true, [&](const handle_t& other) {
                    seenRevPred.insert(make_pair(ng.get_id(other), 
                                             ng.get_is_reverse(other)));
                });
                REQUIRE(seenRevPred.size() == 0);
            }

            
            
             
        }
        
        TEST_CASE( "NetGraph can traverse all the snarls in random graphs",
                  "[snarl_distance][integrated-snarl-finder]" ) {
        
            // Each actual graph takes a fairly long time to do so we randomize sizes...
            
            default_random_engine generator(test_seed_source());
            
            for (size_t repeat = 0; repeat < 100; repeat++) {
            
                uniform_int_distribution<size_t> bases_dist(100, 10000);
                size_t bases = bases_dist(generator);
                uniform_int_distribution<size_t> variant_bases_dist(1, bases/2);
                size_t variant_bases = variant_bases_dist(generator);
                uniform_int_distribution<size_t> variant_count_dist(1, bases/2);
                size_t variant_count = variant_count_dist(generator);
                        
#ifdef debug
                cerr << repeat << ": Do graph of " << bases << " bp with ~" << variant_bases << " bp large variant length and " << variant_count << " events" << endl;
#endif
            
                VG graph;
                random_graph(bases, variant_bases, variant_count, &graph);
                IntegratedSnarlFinder finder(graph); 
                auto manager = finder.find_snarls_parallel();
                
                size_t snarls_seen = 0;
                
                manager.for_each_snarl_preorder([&](const Snarl* snarl) {
                    snarls_seen++;
                    // Make sure we don't follow internal-snarl edges
                    NetGraph net_graph = manager.net_graph_of(snarl, &graph, false);
                    
                    // Make sure we like the nodes in this snarl
                    unordered_set<handle_t> nodes;
                    size_t node_count = 0;
                    net_graph.for_each_handle([&](const handle_t& handle) {
                        nodes.insert(handle);
                        node_count++;
                    });
                    if (nodes.size() != node_count) {
                        cerr << "Problem with graph: " << pb2json(graph.graph) << endl;
                    }
                    REQUIRE(nodes.size() == node_count);
                    
                    // Track edges as followed from, followed to
                    unordered_set<pair<handle_t, handle_t>> edges;
                    // And in canonical order
                    unordered_set<pair<handle_t, handle_t>> canonical_edges;
                    for (auto& handle : nodes) {
                    
                        // Save all the edges off of each node
                        net_graph.follow_edges(handle, false, [&](const handle_t& other) {
                            edges.emplace(handle, other);
                            canonical_edges.insert(net_graph.edge_handle(handle, other));
                        });
                        net_graph.follow_edges(handle, true, [&](const handle_t& other) {
                            edges.emplace(net_graph.flip(handle), net_graph.flip(other));
                            canonical_edges.insert(net_graph.edge_handle(other, handle));
                        });
                    }
                    if (canonical_edges.size() != net_graph.get_edge_count()) {
                        cerr << "Problem with graph: " << pb2json(graph.graph) << endl;
                        cerr << "In snarl: " << snarl->start().node_id() << " -> " << snarl->end().node_id() << endl;
                        cerr << "Observed edges: " << endl;
                        for (auto& edge : canonical_edges) {
                            cerr << "\t" << net_graph.get_id(edge.first) << (net_graph.get_is_reverse(edge.first) ? "-" : "+")
                                << " -> " << net_graph.get_id(edge.second) << (net_graph.get_is_reverse(edge.second) ? "-" : "+") << endl;
                        }
                    }
                    REQUIRE(canonical_edges.size() == net_graph.get_edge_count());
                    for (auto& edge : edges) {
                        // Each edge should be seen from both directions
                        REQUIRE(edges.count(make_pair(net_graph.flip(edge.second), net_graph.flip(edge.first))));
                    }
                });
                    
            }
        
            
        }
       

    */

   }
}
