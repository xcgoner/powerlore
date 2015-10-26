/*  
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


#include <boost/unordered_set.hpp>
#include <graphlab.hpp>
#include <graphlab/macros_def.hpp>
#include "../../src/graphlab/util/timer.hpp"
/**
 *
 * In this program we implement the "k-core" decomposition algorithm.
 * We use a parallel variant of
 * 
 * V. Batagelj and M. Zaversnik, An O(m) algorithm for cores
 * decomposition of networks,
 *
 *  - Essentially, recursively remove everything with degree 1
 *  - Then recursively remove everything with degree 2
 *  - etc.
 */

/*
 * Each vertex maintains a "degree" count. If this value
 * is 0, the vertex is "deleted"
 */
typedef int vertex_data_type;

/*
 * Don't need any edges
 */
typedef graphlab::empty edge_data_type;

/*
 * Define the type of the graph
 */
typedef graphlab::distributed_graph<vertex_data_type,
                                    edge_data_type> graph_type;

// The current K to compute
size_t CURRENT_K;


size_t ITERATIONS = 0;

/*
 * The core K-core implementation.
 * The basic concept is simple.
 * Each vertex maintains a count of the number of adjacent edges.
 * If a vertex receives a message, the message contains the number of
 * adjacent edges deleted. The vertex then updates its counter.
 * If the counter falls below K, it deletes itself
 * (set the adjacent count to 0) and signals each of its neighbors
 * with a message of 1.
 */
class k_core :
  public graphlab::ivertex_program<graph_type,
                                   graphlab::empty, // gathers are integral
                                   int>,   // messages are integral
  public graphlab::IS_POD_TYPE  {
public:
  // the last received message
  int msg;
  
  /* Each vertex can only signal once. I set this flag
   * if it is the first time this vertex falls below K, so I can
   * initiate scattering
   */
  bool just_deleted;
  
  k_core():msg(0),just_deleted(false) { }

  /* The message contains the number of adjacent edges deleted.
   * Store the message in the program, and reset the just_deleted flag
   */
  void init(icontext_type& context, const vertex_type& vertex,
            const message_type& message) {
    msg = message;
    just_deleted = false;
  }

  // gather is never invoked
  edge_dir_type gather_edges(icontext_type& context,
                             const vertex_type& vertex) const {
    return graphlab::NO_EDGES;
  }

  /* On apply, if the vertex has not yet been deleted,
   * decrement the counter on the vertex.
   * If the adjacency count of the vertex falls below K,
   * the vertex shall be deleted.
   * We set the vertex data to 0 to designate that it is deleted
   * and Set the just_deleted flag to signal the neighbors in scatter
   */
  void apply(icontext_type& context, vertex_type& vertex,
             const gather_type& unused) {
    if (vertex.data() > 0) {
      vertex.data() -= msg;
      if (vertex.data() < CURRENT_K) {
        just_deleted = true;
        vertex.data() = 0;
      }
    }
  } 

  /*
   * If the vertex is deleted, we signal all neighbors on the scatter
   */
  edge_dir_type scatter_edges(icontext_type& context,
                              const vertex_type& vertex) const {
    return just_deleted ?
      graphlab::ALL_EDGES : graphlab::NO_EDGES;
  }

  /*
   * For each neighboring vertex, if it is not yet deleted,
   * signal it.
   */
  void scatter(icontext_type& context,
               const vertex_type& vertex,
               edge_type& edge) const {
    vertex_type other = edge.source().id() == vertex.id() ?
      edge.target() : edge.source();
    if (other.data() > 0) {
      context.signal(other, 1);
    }
  }
  
};

// type of the synchronous_engine
typedef graphlab::synchronous_engine<k_core> engine_type;

/*
 * Called before any graph operation is performed.
 * Initializes all vertex data to the number of adjacent edges.
 * Can be called from a graph.transform_vertices()
 */
void initialize_vertex_values(graph_type::vertex_type& v) {
  v.data() = v.num_in_edges() + v.num_out_edges();
}

/*
 * Signals all non-deleted vertices with degree less than K.
 * Can be called from an engine.map_reduce_vertices()
 * We return empty since no reduction is performed. Only the map.
 */
graphlab::empty signal_vertices_at_k(engine_type::icontext_type& ctx,
                                     const graph_type::vertex_type& vertex) {
  if (vertex.data() > 0 && vertex.data() < CURRENT_K) {
    ctx.signal(vertex, 0);
  }
  return graphlab::empty();
}

/*
 * Counts the number of un-deleted vertices.
 */
size_t count_active_vertices(const graph_type::vertex_type& vertex) {
  return vertex.data() > 0;
}

/*
 * Counts the degree of each un-deleted vertex. Half of this
 * will be the size of the K-core graph.
 */
size_t double_count_active_edges(const graph_type::vertex_type& vertex) {
  return (size_t) vertex.data();
}



/*
 * Saves the graph in a tsv format with the condition that
 * the adjacent vertices have not yet been deleted.
 * This allows saving of the k-core graph.
 */
struct save_core_at_k {
  std::string save_vertex(graph_type::vertex_type) { return ""; }
  std::string save_edge(graph_type::edge_type e) {
    if (e.source().data() > 0 && e.target().data() > 0) {
      return graphlab::tostr(e.source().id()) + "\t" +
        graphlab::tostr(e.target().id()) + "\n";
    }
    else return "";
  }
};
    
int main(int argc, char** argv) {
  std::cout << "Computes a k-core decomposition of a graph.\n\n";

  graphlab::command_line_options clopts
    ("K-Core decomposition. This program "
     "computes the K-Core decomposition of a graph, for K ranging from [kmin] "
     "to [kmax]. The size of the remaining K-core graph at each K is printed. "
     "The [savecores] allow the saving of each K-Core graph in a TSV format"
     );
  std::string prefix, format;
  size_t kmin = 0;
  size_t kmax = (size_t)(-1);
  std::string savecores;
  clopts.attach_option("graph", prefix,
                       "Graph input. reads all graphs matching prefix*");
  clopts.attach_option("format", format,
                       "The graph format");
  clopts.attach_option("kmin", kmin,
                       "Compute the k-Core for k the range [kmin,kmax]");
  clopts.attach_option("kmax", kmax,
                       "Compute the k-Core for k the range [kmin,kmax]");
  clopts.attach_option("savecores", savecores,
                       "If non-empty, will save tsv of each core with prefix [savecores].K.");
	clopts.attach_option("iterations", ITERATIONS,
                       "If set, will force the use of the synchronous engine"
                       "overriding any engine option set by the --engine parameter. "
                       "Runs complete (non-dynamic) PageRank for a fixed "
                       "number of iterations. Also overrides the iterations "
                       "option in the engine");
	size_t powerlaw = 0;
  clopts.attach_option("powerlaw", powerlaw,
                       "Generate a synthetic powerlaw degree graph. ");
  double alpha = 2.1, beta = 2.2;
  clopts.attach_option("alpha", alpha,
                         "Power-law constant for indegree ");
  clopts.attach_option("beta", beta,
                         "Power-law constant for outdegree ");
  clopts.attach_option("iterations", ITERATIONS,
                       "If set, will force the use of the synchronous engine"
                       "overriding any engine option set by the --engine parameter. "
                       "Runs complete (non-dynamic) PageRank for a fixed "
                       "number of iterations. Also overrides the iterations "
                       "option in the engine");
  clopts.attach_option("use_delta", USE_DELTA_CACHE,
                       "Use the delta cache to reduce time in gather.");
  std::string result_file;
  clopts.attach_option("result_file", result_file,
                       "If set, will save the test result to the"
                       "specific file");

  if(!clopts.parse(argc, argv)) return EXIT_FAILURE;
  if (prefix == "") {
    std::cout << "--graph is not optional\n";
    clopts.print_description();
    return EXIT_FAILURE;
  }
  else if (format == "") {
    std::cout << "--format is not optional\n";
    clopts.print_description();
    return EXIT_FAILURE;
  }
  else if (kmax < kmin) {
    std::cout << "kmax must be at least as large as kmin\n";
    clopts.print_description();
    return EXIT_FAILURE;
  }
  
  // Initialize control plane using mpi
  graphlab::mpi_tools::init(argc, argv);
  graphlab::distributed_control dc;
  
  if (ITERATIONS) {
    // make sure this is the synchronous engine
    dc.cout() << "--iterations set. Forcing Synchronous engine, and running "
              << "for " << ITERATIONS << " iterations." << std::endl;
    //clopts.get_engine_args().set_option("type", "synchronous");
    clopts.get_engine_args().set_option("max_iterations", ITERATIONS);
    clopts.get_engine_args().set_option("sched_allv", true);
  }
  
  int ntrials = 5;
  int trial_results1[20];
  double trial_results2[20][8];
  uint32_t seed_set[20] = {1492133106, 680965948, 2040586311, 73972395, 942196338, 819390547, 1643934785, 1707678784, 401305863, 1051761031, 956889080, 1387946621, 1523349375, 1620677309, 592759340, 1459650384, 1406812251, 349206043, 255545576, 1070228652};
  for(int i = 0; i < ntrials; i++)
  {
  
	  // load graph
	  graph_type graph(dc, clopts);
	  
	  if(powerlaw > 0) { // make a synthetic graph
		  dc.cout() << "Loading synthetic Powerlaw graph." << std::endl;
		  graph.load_synthetic_powerlaw(powerlaw, alpha, beta, 100000000);
		}
		else if (prefix.length() > 0) { // Load the graph from a file
			if(dc.procid() == 24)
		  std::cout << "Loading graph in format: "<< format << std::endl;
		  graph.load_format(prefix, format);
		}
		else {
		  dc.cout() << "graph or powerlaw option must be specified" << std::endl;
		  clopts.print_description();
		  return 0;
		}

	  graphlab::timer timer;
	  timer.start();
	  graph.finalize();
	  const double ingress_time = timer.current_time();
	  dc.cout() << "Finalizing graph. Finished in "
		  << ingress_time << std::endl;
	  dc.cout() << "Number of vertices: " << graph.num_vertices() << std::endl
				<< "Number of edges:    " << graph.num_edges() << std::endl;

	  graphlab::synchronous_engine<k_core> engine(dc, graph, clopts);

	  // initialize the vertex data with the degree
	  graph.transform_vertices(initialize_vertex_values);

	  timer.start();
	  // for each K value
	  for (CURRENT_K = kmin; CURRENT_K <= kmax; CURRENT_K++) {
		// signal all vertices with degree less than K
		engine.map_reduce_vertices<graphlab::empty>(signal_vertices_at_k);
		// recursively delete all vertices with degree less than K
		engine.start();
		// count the number of vertices and edges remaining
		size_t numv = graph.map_reduce_vertices<size_t>(count_active_vertices);
		size_t nume = graph.map_reduce_vertices<size_t>(double_count_active_edges) / 2;
		if (numv == 0) break;
		// Output the size of the graph
		dc.cout() << "K=" << CURRENT_K << ":  #V = "
				  << numv << "   #E = " << nume << std::endl;

		// Saves the result if requested
		if (savecores != "") {
		  graph.save(savecores + "." + graphlab::tostr(CURRENT_K) + ".",
					 save_core_at_k(),
					 false, /* no compression */ 
					 false, /* do not save vertex */
					 true, /* save edge */ 
					 clopts.get_ncpus()); /* one file per machine */
		}
	  }
	  const double runtime = timer.current_time();

	  if(dc.procid() == 0) {
		std::cout << graph.num_replicas() << "\t"
			<< (double)graph.num_replicas()/graph.num_vertices() << "\t"
			<< graph.get_edge_balance() << "\t"
			<< graph.get_vertex_balance() << "\t"
			<< ingress_time << "\t"
			<< runtime << "\t"
		   << engine.get_exec_time() << "\t"
		   << engine.get_one_itr_time() << "\t"
		   << engine.get_compute_balance() << "\t"
			<< std::endl;
	  }
  
  }
  
  graphlab::mpi_tools::finalize();
  return EXIT_SUCCESS;
} // End of main

