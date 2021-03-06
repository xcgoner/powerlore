/**  
 * Copyright (c) 2009 Carnegie Mellon University. 
 *     All rights reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing,
 *  software distributed under the License is distributed on an "AS
 *  IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 *  express or implied.  See the License for the specific language
 *  governing permissions and limitations under the License.
 *
 * For more about this software visit:
 *
 *      http://www.graphlab.ml.cmu.edu
 *
 */

#ifndef GRAPHLAB_DISTRIBUTED_CONSTELL_INGRESS_HPP
#define GRAPHLAB_DISTRIBUTED_CONSTELL_INGRESS_HPP

#include <boost/functional/hash.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/unordered_set.hpp>

#include <graphlab/graph/graph_basic_types.hpp>
#include <graphlab/graph/ingress/distributed_ingress_base.hpp>
#include <graphlab/graph/distributed_graph.hpp>
#include <graphlab/graph/graph_hash.hpp>
#include <graphlab/rpc/buffered_exchange.hpp>
#include <graphlab/rpc/distributed_event_log.hpp>
#include <graphlab/parallel/pthread_tools.hpp>
#include <graphlab/logger/logger.hpp>
#include <graphlab/util/dense_bitset.hpp>
#include <vector>

#include <graphlab/macros_def.hpp>
namespace graphlab {
  template<typename VertexData, typename EdgeData>
  class distributed_graph;

  /**
   * \brief Ingress object assigning edges using randoming hash function.
   */
  template<typename VertexData, typename EdgeData>
  class distributed_constell_ingress :
    public distributed_ingress_base<VertexData, EdgeData> {
  public:
    typedef distributed_graph<VertexData, EdgeData> graph_type;
    /// The type of the vertex data stored in the graph 
    typedef VertexData vertex_data_type;
    /// The type of the edge data stored in the graph 
    typedef EdgeData   edge_data_type;

    typedef distributed_ingress_base<VertexData, EdgeData> base_type;

    typedef typename graph_type::vertex_record vertex_record;
    typedef typename graph_type::mirror_type mirror_type;

    typedef typename buffered_exchange<vertex_id_type>::buffer_type
        vertex_id_buffer_type;

    /// The rpc interface for this object
    dc_dist_object<distributed_constell_ingress> rpc;
    /// The underlying distributed graph object that is being loaded
    graph_type& graph;

    bool standalone;

    /// threshold to divide high-degree and low-degree vertices
    size_t threshold;
    /// interval to synchronize the mht
    size_t interval;

//    typedef typename base_type::edge_buffer_record edge_buffer_record;
    struct edge_buffer_record {
      vertex_id_type source, target;
      edge_data_type edata;
      // additional information to mark the hashed vertex
      unsigned short hash_flag;
      edge_buffer_record(const vertex_id_type& source = vertex_id_type(-1),
                         const vertex_id_type& target = vertex_id_type(-1),
                         const edge_data_type& edata = edge_data_type(),
                         const unsigned short& hash_flag = 0) :
        source(source), target(target), edata(edata), hash_flag(hash_flag) { }
      void load(iarchive& arc) { arc >> source >> target >> edata >> hash_flag; }
      void save(oarchive& arc) const { arc << source << target << edata << hash_flag; }
    };
    typedef typename buffered_exchange<edge_buffer_record>::buffer_type
        edge_buffer_type;

    typedef typename base_type::vertex_buffer_record vertex_buffer_record;
    typedef typename buffered_exchange<vertex_buffer_record>::buffer_type
        vertex_buffer_type;

    typedef fixed_dense_bitset<RPC_MAX_N_PROCS> bin_counts_type;

    /** Type of the mirror hash table:
     * a map from vertex id to a bitset of length num_procs. */
    typedef typename boost::unordered_map<vertex_id_type, bin_counts_type>
    mht_type;

    /** distributed hash table stored on local machine */
    boost::unordered_map<vertex_id_type, bin_counts_type > mht;

    typedef typename boost::unordered_map<vertex_id_type,
      std::vector<edge_buffer_record> > raw_map_type;

    struct mirror_buffer_record {
      vertex_id_type vid;
      procid_t pid;
      mirror_buffer_record(const vertex_id_type& vid = vertex_id_type(-1), const procid_t& pid = procid_t(-1))
        :vid(vid), pid(pid) { }
      void load(iarchive& arc) { arc >> vid >> pid; }
      void save(oarchive& arc) const { arc << vid << pid; }
    };
    buffered_exchange<mirror_buffer_record> mirror_exchange;
    typedef typename buffered_exchange<mirror_buffer_record>::buffer_type
      mirror_buffer_type;

    struct proc_edges_incre_record {
      procid_t pid;
      size_t increment;
      proc_edges_incre_record(const procid_t& pid = procid_t(-1), const size_t& increment = 0)
        :pid(pid), increment(increment) { }
      void load(iarchive& arc) { arc >> pid >> increment; }
      void save(oarchive& arc) const { arc << pid << increment; }
    };
    buffered_exchange<proc_edges_incre_record> proc_edges_incre_exchange;
    typedef typename buffered_exchange<proc_edges_incre_record>::buffer_type
      proc_edges_incre_buffer_type;

    /* ingress exchange */
    buffered_exchange<edge_buffer_record> edge_exchange;
    buffered_exchange<vertex_buffer_record> vertex_exchange;

    struct vertex_degree_buffer_record {
      vertex_id_type vid;
      size_t degree;
      vertex_degree_buffer_record(const vertex_id_type& vid = vertex_id_type(-1), const size_t& degree = 0)
        :vid(vid), degree(degree) { }
      void load(iarchive& arc) { arc >> vid >> degree; }
      void save(oarchive& arc) const { arc << vid << degree; }
    };
    typedef typename buffered_exchange<vertex_degree_buffer_record>::buffer_type
        vertex_degree_buffer_type;


    typedef typename base_type::vertex_negotiator_record vertex_negotiator_record;
   
  public:
    distributed_constell_ingress(distributed_control& dc, graph_type& graph, size_t interval = 10) :
    base_type(dc, graph), rpc(dc, this),
    graph(graph), interval(interval),
    vertex_exchange(dc), edge_exchange(dc),
    mirror_exchange(dc), proc_edges_incre_exchange(dc)
    {
      /* fast pass for standalone case. */
      standalone = rpc.numprocs() == 1;
      rpc.barrier();
    } // end of constructor

    ~distributed_constell_ingress() { }

    /** Add an edge to the ingress object using random assignment. */
    void add_edge(vertex_id_type source, vertex_id_type target,
                  const EdgeData& edata) {
//      const edge_buffer_record record(source, target, edata);
//      const procid_t owning_proc = base_type::edge_decision.edge_to_proc_random(source, target, base_type::rpc.numprocs());
//      edge_exchange.send(owning_proc, record);
      // assign to source
      const procid_t source_owning_proc = standalone ? 0 :
                  graph_hash::hash_vertex(source) % rpc.numprocs();
//          0;
      // assign to source
      const procid_t target_owning_proc = standalone ? 0 :
                  graph_hash::hash_vertex(target) % rpc.numprocs();
//          0;
      if(source_owning_proc == target_owning_proc) {
          const edge_buffer_record record(source, target, edata, 0);
          edge_exchange.send(source_owning_proc, record);
      }
      else {
          const edge_buffer_record source_record(source, target, edata, 1);
          edge_exchange.send(source_owning_proc, source_record);

          const edge_buffer_record target_record(source, target, edata, 2);
          edge_exchange.send(target_owning_proc, target_record);
      }
    } // end of add edge

    /* add vdata */
    void add_vertex(vertex_id_type vid, const VertexData& vdata) {
      const vertex_buffer_record record(vid, vdata);
      const procid_t owning_proc = standalone ? 0 :
        graph_hash::hash_vertex(vid) % rpc.numprocs();
      // for debug
//      std::cout << vid << ":\t" << owning_proc << std::endl;
      vertex_exchange.send(owning_proc, record);
    } // end of add vertex

    /** Greedy assign (source, target) to a machine using:
     *  bitset<MAX_MACHINE> src_degree : the degree presence of source over machines
     *  bitset<MAX_MACHINE> dst_degree : the degree presence of target over machines
     *  vector<size_t>      proc_num_edges : the edge counts over machines
     * */
    procid_t edge_to_proc_degree (const vertex_id_type source,
        const vertex_id_type target,
        const bin_counts_type& src_mirror,
        const bin_counts_type& dst_mirror,
        const std::vector<size_t>& proc_num_edges,
        hopscotch_map<vertex_id_type, size_t>& degree_set) {
      size_t numprocs = proc_num_edges.size();

      // Compute the score of each proc.
      procid_t best_proc = -1;
      double maxscore = 0.0;
      double epsilon = 1.0;
      std::vector<double> proc_score(numprocs);
      size_t minedges = *std::min_element(proc_num_edges.begin(), proc_num_edges.end());
      size_t maxedges = *std::max_element(proc_num_edges.begin(), proc_num_edges.end());

      size_t source_degree, target_degree;
      source_degree = degree_set[source];
      target_degree = degree_set[target];

      bool is_source_small = (target_degree >= source_degree);
      bool is_target_small = (target_degree <= source_degree);
      for (size_t i = 0; i < numprocs; ++i) {
        size_t sd = src_mirror.get(i);
        size_t td = dst_mirror.get(i);
        double bal = (maxedges - proc_num_edges[i])/(epsilon + maxedges - minedges);
        bool sd1 = (sd > 0);
        bool td1 = (td > 0);
        bool sd2 = (sd1 && is_source_small);
        bool td2 = (td1 && is_target_small);
        bool d0 = (sd2 && td2);
        proc_score[i] = bal + sd1 + sd2 + td1 + td2 - d0;
      }
      maxscore = *std::max_element(proc_score.begin(), proc_score.end());

      std::vector<procid_t> top_procs;
      for (size_t i = 0; i < numprocs; ++i)
        if (std::fabs(proc_score[i] - maxscore) < 1e-5)
          top_procs.push_back(i);

      // Hash the edge to one of the best procs.
      typedef std::pair<vertex_id_type, vertex_id_type> edge_pair_type;
      const edge_pair_type edge_pair(std::min(source, target),
          std::max(source, target));
      best_proc = top_procs[graph_hash::hash_edge(edge_pair) % top_procs.size()];

      return best_proc;
    }

    void sync_assign(hopscotch_map<vertex_id_type, size_t>& degree_set,
        const std::vector<edge_buffer_record>& edge_buffer,
        std::vector<size_t>& proc_num_edges) {
      procid_t l_procid = rpc.procid();
      procid_t proc(-1);
//      std::vector<size_t> proc_incre_edges(rpc.numprocs());
//      foreach(size_t& num_edges, proc_incre_edges) {
//        num_edges = 0;
//      }
      size_t edge_count = 0;
      foreach(const edge_buffer_record& rec, edge_buffer) {
        if(mht.count(rec.source) == 0)
          mht[rec.source].clear();
        if(mht.count(rec.target) == 0)
          mht[rec.target].clear();
        const procid_t best_pid = edge_to_proc_degree(rec.source, rec.target,
                                    mht[rec.source], mht[rec.target],
                                    proc_num_edges, degree_set);

        edge_exchange.send(best_pid, rec);

        if(mht[rec.source].get(best_pid) == false) {
            const mirror_buffer_record mirror_record(rec.source, best_pid);
            for(procid_t pid = 0; pid < rpc.numprocs(); pid++) {
              if(pid == l_procid)
                mht[rec.source].set_bit(best_pid);
              else {
                mirror_exchange.send(pid, mirror_record);
//                      mirror_exchange.partial_flush(0);
              }
            }
        }

        if(mht[rec.target].get(best_pid) == false) {
            const mirror_buffer_record mirror_record(rec.target, best_pid);
            for(procid_t pid = 0; pid < rpc.numprocs(); pid++) {
              if(pid == l_procid)
                mht[rec.target].set_bit(best_pid);
              else {
                mirror_exchange.send(pid, mirror_record);
//                      mirror_exchange.partial_flush(0);
              }
            }
        }

        proc_num_edges[best_pid]++;
//        proc_incre_edges[best_pid]++;

        if(edge_count % interval == 0) {
//          // send the increment of proc_num_edges
//          for (procid_t p = 0; p < rpc.numprocs(); p++) {
//            const proc_edges_incre_record edges_incre_record(p, proc_incre_edges[p]);
//            for (procid_t i = 0; i < rpc.numprocs(); i++)
//              if (i != l_procid)
//                proc_edges_incre_exchange.send(i, edges_incre_record);
//            proc_incre_edges[p] = 0;
//          }

          // flush increment and mirrors
//          proc_edges_incre_exchange.partial_flush(0);
          mirror_exchange.partial_flush(0);

//          // update number of edges
//          if(proc_edges_incre_exchange.size() > 0) {
//              proc_edges_incre_buffer_type proc_edges_incre_buffer;
//              proc = -1;
//              while(proc_edges_incre_exchange.recv(proc, proc_edges_incre_buffer)) {
//                  foreach(const proc_edges_incre_record& rec, proc_edges_incre_buffer) {
//                    proc_num_edges[rec.pid] += rec.increment;
//                  }
//              } // end of while
//              proc_edges_incre_exchange.clear();
//          } // end of mirror_exchange.size() > 0

          if(mirror_exchange.size() > 0) {
              mirror_buffer_type mirror_buffer;
              proc = -1;
              while(mirror_exchange.recv(proc, mirror_buffer)) {
                  foreach(const mirror_buffer_record& rec, mirror_buffer) {
                    if(mht.count(rec.vid) == 0)
                      mht[rec.vid].clear();
                    mht[rec.vid].set_bit(rec.pid);
                  }
              } // end of while
              mirror_exchange.clear();
          } // end of mirror_exchange.size() > 0
      } // end of internal

        edge_count++;
      } // end of foreach

      // synchronize the residual ...
      mirror_exchange.flush();
      if(mirror_exchange.size() > 0) {
          mirror_buffer_type mirror_buffer;
          proc = -1;
          while(mirror_exchange.recv(proc, mirror_buffer)) {
              foreach(const mirror_buffer_record& rec, mirror_buffer) {
                if(mht.count(rec.vid) == 0)
                  mht[rec.vid].clear();
                mht[rec.vid].set_bit(rec.pid);
              }
          } // end of while
          mirror_exchange.clear();
      } // end of mirror_exchange.size() > 0
//      proc_edges_incre_exchange.flush();
//      if(proc_edges_incre_exchange.size() > 0) {
//          proc_edges_incre_buffer_type proc_edges_incre_buffer;
//          proc = -1;
//          while(proc_edges_incre_exchange.recv(proc, proc_edges_incre_buffer)) {
//              foreach(const proc_edges_incre_record& rec, proc_edges_incre_buffer) {
//                proc_num_edges[rec.pid] += rec.increment;
//              }
//          } // end of while
//          proc_edges_incre_exchange.clear();
//      } // end of mirror_exchange.size() > 0
    }

    void finalize() {

      const procid_t l_procid = rpc.procid();
      const size_t nprocs = rpc.numprocs();

      rpc.full_barrier();

      bool first_time_finalize = false;
      /**
       * Fast pass for first time finalization.
       */
      if (graph.is_dynamic()) {
        size_t nverts = graph.num_local_vertices();
        rpc.all_reduce(nverts);
        first_time_finalize = (nverts == 0);
      } else {
        first_time_finalize = false;
      }


      if (l_procid == 0) {
        logstream(LOG_EMPH) << "Finalizing Graph..." << std::endl;
      }

      typedef typename hopscotch_map<vertex_id_type, lvid_type>::value_type
        vid2lvid_pair_type;

      typedef typename buffered_exchange<edge_buffer_record>::buffer_type
        edge_buffer_type;

      typedef typename buffered_exchange<vertex_buffer_record>::buffer_type
        vertex_buffer_type;

      /**
       * \internal
       * Buffer storage for new vertices to the local graph.
       */
      typedef typename graph_type::hopscotch_map_type vid2lvid_map_type;
      vid2lvid_map_type vid2lvid_buffer;

      /**
       * \internal
       * The beginning id assigned to the first new vertex.
       */
      const lvid_type lvid_start  = graph.vid2lvid.size();

      /**
       * \internal
       * Bit field incidate the vertex that is updated during the ingress.
       */
      dense_bitset updated_lvids(graph.vid2lvid.size());

      /**************************************************************************/
      /*                                                                        */
      /*                       Flush any additional data                        */
      /*                                                                        */
      /**************************************************************************/
      edge_exchange.flush(); vertex_exchange.flush();

      /**
       * Fast pass for redundant finalization with no graph changes.
       */
      {
        size_t changed_size = edge_exchange.size() + vertex_exchange.size();
        rpc.all_reduce(changed_size);
        if (changed_size == 0) {
          logstream(LOG_INFO) << "Skipping Graph Finalization because no changes happened..." << std::endl;
          return;
        }
      }

      if(l_procid == 0)
        memory_info::log_usage("Post Flush");

      /**************************************************************************/
      /*                                                                        */
      /*                       Prepare constell ingress                           */
      /*                                                                        */
      /**************************************************************************/

      boost::unordered_map<vertex_id_type, procid_t> master_map;
      {
        std::vector<edge_buffer_record> edge_recv_buffer;
        hopscotch_map<vertex_id_type, size_t> degree_set;
        buffered_exchange<vertex_degree_buffer_record> vertex_degree_exchange(rpc.dc());
        if (!standalone) {
            procid_t proc = -1;
            edge_recv_buffer.reserve(edge_exchange.size());
            edge_buffer_type edge_buffer;
            // count degree
            std::vector< boost::dynamic_bitset<> > degree_exchange_set(nprocs);
            while(edge_exchange.recv(proc, edge_buffer)) {
              foreach(const edge_buffer_record& rec, edge_buffer) {
//                if(rec.hash_flag == 0 || ((rec.source >= rec.target) + 1) == rec.hash_flag)
                edge_recv_buffer.push_back(rec);
                // count the degree of each vertex
                degree_set[rec.target]++;
                degree_set[rec.source]++;

                if(rec.hash_flag == 1) {
                    // this proc is the source vertex hashed to and target_id < source_id
                    const procid_t owning_proc = graph_hash::hash_vertex(rec.target) % nprocs;
                    if(degree_exchange_set[owning_proc].size() < (rec.source + 1))
                      degree_exchange_set[owning_proc].resize(rec.source + 1);
                    degree_exchange_set[owning_proc][rec.source] = true;
                }
                else if(rec.hash_flag == 2) {
                    // this proc is the target vertex hashed to
                    const procid_t owning_proc = graph_hash::hash_vertex(rec.source) % nprocs;
                    if(degree_exchange_set[owning_proc].size() < (rec.target + 1))
                      degree_exchange_set[owning_proc].resize(rec.target + 1);
                    degree_exchange_set[owning_proc][rec.target] = true;
                }

              }
            }
            edge_exchange.clear();

            // send degree
            for(size_t idx = 0; idx < degree_exchange_set.size(); idx++) {
                for(size_t vid = degree_exchange_set[idx].find_first();
                    vid != degree_exchange_set[idx].npos;
                    vid = degree_exchange_set[idx].find_next(vid)) {
                    const vertex_degree_buffer_record record(vid, degree_set[vid]);
                    vertex_degree_exchange.send(idx, record);
                }
            }
            vertex_degree_exchange.flush();

            vertex_degree_buffer_type vertex_degree_buffer;
            proc = -1;
            while(vertex_degree_exchange.recv(proc, vertex_degree_buffer)) {
                foreach(const vertex_degree_buffer_record& rec, vertex_degree_buffer) {
                  degree_set[rec.vid] = rec.degree;
                }
            }
            vertex_degree_exchange.clear();
            // degree_set is ready

            // filter the edges to be assigned
            // rearrange the edges
            {
              raw_map_type raw_map;
              foreach(const edge_buffer_record& rec, edge_recv_buffer) {
                if(rec.hash_flag == 0
                    || ((degree_set[rec.source] >= degree_set[rec.target]) + 1) == rec.hash_flag)
                  raw_map[rec.source].push_back(rec);
              }
              const size_t edge_buffer_size = edge_recv_buffer.size();
              std::vector<edge_buffer_record>().swap(edge_recv_buffer);
              edge_recv_buffer.clear();
              edge_recv_buffer.reserve(edge_buffer_size);
              for (typename raw_map_type::iterator it = raw_map.begin();
                        it != raw_map.end(); ++it) {
                  size_t degree = it->second.size();
                  for(size_t i = 0; i < degree; ++i)
                    edge_recv_buffer.push_back(it->second[i]);
              }
            }

//            rpc.full_barrier();

            // initialize
            std::vector<size_t> proc_num_edges(nprocs);
            foreach(size_t& num_edges, proc_num_edges) {
              num_edges = 0;
            }

            // assign all
            rpc.full_barrier();
            sync_assign(degree_set, edge_recv_buffer, proc_num_edges);

            edge_exchange.flush();

            // set up the map from vid to its master proc after mht is synchronized
            for(mht_type::iterator it = mht.begin(); it != mht.end(); it++) {
//                if(graph_hash::hash_vertex(it->first) % nprocs == l_procid) {
                    std::vector<procid_t> master_candidates;
                    for(size_t idx = 0; idx < nprocs; idx++) {
                        if(it->second.get(idx) > 0)
                          master_candidates.push_back(idx);
                    }
                    master_map[it->first] = master_candidates[graph_hash::hash_vertex(it->first) % master_candidates.size()];
                    // for debug
//                    std::cout << l_procid << ":\t" << it->first << ":\t" << master_map[it->first] << std::endl;
//                }
            }

        } // end of if (!standalone)
      } // end of Prepare constell ingress

      /**************************************************************************/
      /*                                                                        */
      /*                         Construct local graph                          */
      /*                                                                        */
      /**************************************************************************/
      { // Add all the edges to the local graph
        logstream(LOG_INFO) << "Graph Finalize: constructing local graph" << std::endl;
        const size_t nedges = edge_exchange.size() + 1;
        graph.local_graph.reserve_edge_space(nedges + 1);
        edge_buffer_type edge_buffer;
        procid_t proc(-1);
        while(edge_exchange.recv(proc, edge_buffer)) {
          foreach(const edge_buffer_record& rec, edge_buffer) {
            // Get the source_vlid;
            lvid_type source_lvid(-1);
            if(graph.vid2lvid.find(rec.source) == graph.vid2lvid.end()) {
              if (vid2lvid_buffer.find(rec.source) == vid2lvid_buffer.end()) {
                source_lvid = lvid_start + vid2lvid_buffer.size();
                vid2lvid_buffer[rec.source] = source_lvid;
              } else {
                source_lvid = vid2lvid_buffer[rec.source];
              }
            } else {
              source_lvid = graph.vid2lvid[rec.source];
              updated_lvids.set_bit(source_lvid);
            }
            // Get the target_lvid;
            lvid_type target_lvid(-1);
            if(graph.vid2lvid.find(rec.target) == graph.vid2lvid.end()) {
              if (vid2lvid_buffer.find(rec.target) == vid2lvid_buffer.end()) {
                target_lvid = lvid_start + vid2lvid_buffer.size();
                vid2lvid_buffer[rec.target] = target_lvid;
              } else {
                target_lvid = vid2lvid_buffer[rec.target];
              }
            } else {
              target_lvid = graph.vid2lvid[rec.target];
              updated_lvids.set_bit(target_lvid);
            }
            graph.local_graph.add_edge(source_lvid, target_lvid, rec.edata);
//            std::cout << l_procid << " add edge " << rec.source << "\t" << rec.target << std::endl;
          } // end of loop over add edges
        } // end for loop over buffers
        edge_exchange.clear();

        ASSERT_EQ(graph.vid2lvid.size()  + vid2lvid_buffer.size(), graph.local_graph.num_vertices());
        if(l_procid == 0)  {
          memory_info::log_usage("Finished populating local graph.");
        }

        // Finalize local graph
        logstream(LOG_INFO) << "Graph Finalize: finalizing local graph."
                            << std::endl;
        graph.local_graph.finalize();
        logstream(LOG_INFO) << "Local graph info: " << std::endl
                            << "\t nverts: " << graph.local_graph.num_vertices()
                            << std::endl
                            << "\t nedges: " << graph.local_graph.num_edges()
                            << std::endl;

        if(l_procid == 0) {
          memory_info::log_usage("Finished finalizing local graph.");
          // debug
          // std::cout << graph.local_graph << std::endl;
        }
      }

      /**************************************************************************/
      /*                                                                        */
      /*             Receive and add vertex data to masters                     */
      /*                                                                        */
      /**************************************************************************/
      // Setup the map containing all the vertices being negotiated by this machine
      { // Receive any vertex data sent by other machines
        vertex_buffer_type vertex_buffer; procid_t sending_proc(-1);
        while(vertex_exchange.recv(sending_proc, vertex_buffer)) {
          foreach(const vertex_buffer_record& rec, vertex_buffer) {
            lvid_type lvid(-1);
            if (graph.vid2lvid.find(rec.vid) == graph.vid2lvid.end()) {
              if (vid2lvid_buffer.find(rec.vid) == vid2lvid_buffer.end()) {
                lvid = lvid_start + vid2lvid_buffer.size();
                vid2lvid_buffer[rec.vid] = lvid;
              } else {
                lvid = vid2lvid_buffer[rec.vid];
              }
            } else {
              lvid = graph.vid2lvid[rec.vid];
              updated_lvids.set_bit(lvid);
            }
            if (vertex_combine_strategy && lvid < graph.num_local_vertices()) {
              vertex_combine_strategy(graph.l_vertex(lvid).data(), rec.vdata);
            } else {
              graph.local_graph.add_vertex(lvid, rec.vdata);
            }
          }
        }
        vertex_exchange.clear();
        if(l_procid == 0)
          memory_info::log_usage("Finished adding vertex data");
      } // end of loop to populate vrecmap



      /**************************************************************************/
      /*                                                                        */
      /*        assign vertex data and allocate vertex (meta)data  space        */
      /*                                                                        */
      /**************************************************************************/
      { // Determine masters for all negotiated vertices
        const size_t local_nverts = graph.vid2lvid.size() + vid2lvid_buffer.size();
        graph.lvid2record.reserve(local_nverts);
        graph.lvid2record.resize(local_nverts);
        graph.local_graph.resize(local_nverts);
        foreach(const vid2lvid_pair_type& pair, vid2lvid_buffer) {
            vertex_record& vrec = graph.lvid2record[pair.second];
            vrec.gvid = pair.first;
//            vrec.owner = graph_hash::hash_vertex(pair.first) % nprocs;
            vrec.owner = master_map[pair.first];
            // for debug
//            std::cout << l_procid << ":\t" << pair.first << ":\t" << vrec.owner << std::endl;
        }
        ASSERT_EQ(local_nverts, graph.local_graph.num_vertices());
        // for debug
//        std::cout << local_nverts << std::endl;
        ASSERT_EQ(graph.lvid2record.size(), graph.local_graph.num_vertices());
        if(l_procid == 0)
          memory_info::log_usage("Finihsed allocating lvid2record");
        master_map.clear();
      }

      /**************************************************************************/
      /*                                                                        */
      /*                          Master handshake                              */
      /*                                                                        */
      /**************************************************************************/
      {
  #ifdef _OPENMP
        buffered_exchange<vertex_id_type> vid_buffer(rpc.dc(), omp_get_max_threads());
  #else
        buffered_exchange<vertex_id_type> vid_buffer(rpc.dc());
  #endif

  #ifdef _OPENMP
  #pragma omp parallel for
  #endif
        // send not owned vids to their master
        for (lvid_type i = lvid_start; i < graph.lvid2record.size(); ++i) {
          procid_t master = graph.lvid2record[i].owner;
          if (master != l_procid)
  #ifdef _OPENMP
            vid_buffer.send(master, graph.lvid2record[i].gvid, omp_get_thread_num());
  #else
            vid_buffer.send(master, graph.lvid2record[i].gvid);
  #endif
        }
        vid_buffer.flush();
        rpc.barrier();

        // receive all vids owned by me
        mutex flying_vids_lock;
        boost::unordered_map<vertex_id_type, mirror_type> flying_vids;
  #ifdef _OPENMP
  #pragma omp parallel
  #endif
        {
          typename buffered_exchange<vertex_id_type>::buffer_type buffer;
          procid_t recvid;
          while(vid_buffer.recv(recvid, buffer)) {
            foreach(const vertex_id_type vid, buffer) {
              if (graph.vid2lvid.find(vid) == graph.vid2lvid.end()) {
                if (vid2lvid_buffer.find(vid) == vid2lvid_buffer.end()) {
                  flying_vids_lock.lock();
                  mirror_type& mirrors = flying_vids[vid];
                  flying_vids_lock.unlock();
                  mirrors.set_bit(recvid);
                } else {
                  lvid_type lvid = vid2lvid_buffer[vid];
                  graph.lvid2record[lvid]._mirrors.set_bit(recvid);
                }
              } else {
                lvid_type lvid = graph.vid2lvid[vid];
                graph.lvid2record[lvid]._mirrors.set_bit(recvid);
                updated_lvids.set_bit(lvid);
              }
            }
          }
        }

        vid_buffer.clear();
        // reallocate spaces for the flying vertices.
        size_t vsize_old = graph.lvid2record.size();
        size_t vsize_new = vsize_old + flying_vids.size();
        graph.lvid2record.resize(vsize_new);
        graph.local_graph.resize(vsize_new);
        for (typename boost::unordered_map<vertex_id_type, mirror_type>::iterator it = flying_vids.begin();
             it != flying_vids.end(); ++it) {
          lvid_type lvid = lvid_start + vid2lvid_buffer.size();
          vertex_id_type gvid = it->first;
          graph.lvid2record[lvid].owner = l_procid;
          graph.lvid2record[lvid].gvid = gvid;
          graph.lvid2record[lvid]._mirrors= it->second;
          vid2lvid_buffer[gvid] = lvid;
          // for debug
//         std::cout << "proc " << l_procid << " recevies flying vertex " << gvid << std::endl;
        }
      } // end of master handshake

      /**************************************************************************/
      /*                                                                        */
      /*                        Merge in vid2lvid_buffer                        */
      /*                                                                        */
      /**************************************************************************/
      {
        if (graph.vid2lvid.size() == 0) {
          graph.vid2lvid.swap(vid2lvid_buffer);
        } else {
          graph.vid2lvid.rehash(graph.vid2lvid.size() + vid2lvid_buffer.size());
          foreach (const typename vid2lvid_map_type::value_type& pair, vid2lvid_buffer) {
            graph.vid2lvid.insert(pair);
          }
          vid2lvid_buffer.clear();
          // vid2lvid_buffer.swap(vid2lvid_map_type(-1));
        }
      }


      /**************************************************************************/
      /*                                                                        */
      /*              synchronize vertex data and meta information              */
      /*                                                                        */
      /**************************************************************************/
      {
        // construct the vertex set of changed vertices

        // Fast pass for first time finalize;
        vertex_set changed_vset(true);

        // Compute the vertices that needs synchronization
        if (!first_time_finalize) {
          vertex_set changed_vset = vertex_set(false);
          changed_vset.make_explicit(graph);
          updated_lvids.resize(graph.num_local_vertices());
          for (lvid_type i = lvid_start; i <  graph.num_local_vertices(); ++i) {
            updated_lvids.set_bit(i);
          }
          changed_vset.localvset = updated_lvids;
          buffered_exchange<vertex_id_type> vset_exchange(rpc.dc());
          // sync vset with all mirrors
          changed_vset.synchronize_mirrors_to_master_or(graph, vset_exchange);
          changed_vset.synchronize_master_to_mirrors(graph, vset_exchange);
        }

        graphlab::graph_gather_apply<graph_type, vertex_negotiator_record>
            vrecord_sync_gas(graph,
                             boost::bind(&distributed_constell_ingress::finalize_gather, this, _1, _2),
                             boost::bind(&distributed_constell_ingress::finalize_apply, this, _1, _2, _3));
        vrecord_sync_gas.exec(changed_vset);

        if(l_procid == 0)
          memory_info::log_usage("Finished synchronizing vertex (meta)data");
      }

      base_type::exchange_global_info(false);
    } // end of finalize

  private:
    boost::function<void(vertex_data_type&, const vertex_data_type&)> vertex_combine_strategy;

    /**
     * \brief Gather the vertex distributed meta data.
     */
    vertex_negotiator_record finalize_gather(lvid_type& lvid, graph_type& graph) {
        vertex_negotiator_record accum;
        accum.num_in_edges = graph.local_graph.num_in_edges(lvid);
        accum.num_out_edges = graph.local_graph.num_out_edges(lvid);
        if (graph.l_is_master(lvid)) {
          accum.has_data = true;
          accum.vdata = graph.l_vertex(lvid).data();
          accum.mirrors = graph.lvid2record[lvid]._mirrors;
        }
        return accum;
    }

    /**
     * \brief Update the vertex datastructures with the gathered vertex metadata.
     */
    void finalize_apply(lvid_type lvid, const vertex_negotiator_record& accum, graph_type& graph) {
        typename graph_type::vertex_record& vrec = graph.lvid2record[lvid];
        vrec.num_in_edges = accum.num_in_edges;
        vrec.num_out_edges = accum.num_out_edges;
        graph.l_vertex(lvid).data() = accum.vdata;
        vrec._mirrors = accum.mirrors;
    }

  }; // end of distributed_constell_ingress
}; // end of namespace graphlab
#include <graphlab/macros_undef.hpp>


#endif
