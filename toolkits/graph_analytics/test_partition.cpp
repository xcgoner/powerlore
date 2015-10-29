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

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <time.h>

#include <graphlab.hpp>
// #include <graphlab/macros_def.hpp>

// Global random reset probability
double RESET_PROB = 0.15;

double TOLERANCE = 1.0E-2;

size_t ITERATIONS = 0;

bool USE_DELTA_CACHE = false;

// The vertex data is just the pagerank value (a double)
typedef double vertex_data_type;

// There is no edge data in the pagerank application
typedef graphlab::empty edge_data_type;

// The graph type is determined by the vertex and edge data types
typedef graphlab::distributed_graph<vertex_data_type, edge_data_type> graph_type;

/*
 * A simple function used by graph.transform_vertices(init_vertex);
 * to initialize the vertes data.
 */
void init_vertex(graph_type::vertex_type& vertex) { vertex.data() = 1; }



/*
 * The factorized page rank update function extends ivertex_program
 * specifying the:
 *
 *   1) graph_type
 *   2) gather_type: double (returned by the gather function). Note
 *      that the gather type is not strictly needed here since it is
 *      assumed to be the same as the vertex_data_type unless
 *      otherwise specified
 *
 * In addition ivertex program also takes a message type which is
 * assumed to be empty. Since we do not need messages no message type
 * is provided.
 *
 * pagerank also extends graphlab::IS_POD_TYPE (is plain old data type)
 * which tells graphlab that the pagerank program can be serialized
 * (converted to a byte stream) by directly reading its in memory
 * representation.  If a vertex program does not exted
 * graphlab::IS_POD_TYPE it must implement load and save functions.
 */
class pagerank :
  public graphlab::ivertex_program<graph_type, double> {

  double last_change;
public:

  /**
   * Gather only in edges.
   */
  edge_dir_type gather_edges(icontext_type& context,
                              const vertex_type& vertex) const {
    return graphlab::ALL_EDGES;
  } // end of Gather edges


  /* Gather the weighted rank of the adjacent page   */
  double gather(icontext_type& context, const vertex_type& vertex,
               edge_type& edge) const {
    return (edge.source().data() / edge.source().num_out_edges());
  }

  /* Use the total rank of adjacent pages to update this page */
  void apply(icontext_type& context, vertex_type& vertex,
             const gather_type& total) {

    const double newval = (1.0 - RESET_PROB) * total + RESET_PROB;
    last_change = (newval - vertex.data());
    vertex.data() = newval;
    if (ITERATIONS) context.signal(vertex);
  }

  /* The scatter edges depend on whether the pagerank has converged */
  edge_dir_type scatter_edges(icontext_type& context,
                              const vertex_type& vertex) const {
//    // If an iteration counter is set then
//    if (ITERATIONS) return graphlab::NO_EDGES;
//    // In the dynamic case we run scatter on out edges if the we need
//    // to maintain the delta cache or the tolerance is above bound.
//    if(USE_DELTA_CACHE || std::fabs(last_change) > TOLERANCE ) {
//      return graphlab::OUT_EDGES;
//    } else {
//      return graphlab::NO_EDGES;
//    }
    return graphlab::ALL_EDGES;
  }

  /* The scatter function just signal adjacent pages */
  void scatter(icontext_type& context, const vertex_type& vertex,
               edge_type& edge) const {
//    if(USE_DELTA_CACHE) {
//      context.post_delta(edge.target(), last_change);
//    }
//
//    if(last_change > TOLERANCE || last_change < -TOLERANCE) {
//        context.signal(edge.target());
//    } else {
//      context.signal(edge.target()); //, std::fabs(last_change));
//    }
    context.signal(edge.target());
    context.signal(edge.source());
  }

  void save(graphlab::oarchive& oarc) const {
    // If we are using iterations as a counter then we do not need to
    // move the last change in the vertex program along with the
    // vertex data.
    if (ITERATIONS == 0) oarc << last_change;
  }

  void load(graphlab::iarchive& iarc) {
    if (ITERATIONS == 0) iarc >> last_change;
  }

}; // end of factorized_pagerank update functor


/*
 * We want to save the final graph so we define a write which will be
 * used in graph.save("path/prefix", pagerank_writer()) to save the graph.
 */
struct pagerank_writer {
  std::string save_vertex(graph_type::vertex_type v) {
    std::stringstream strm;
    strm << v.id() << "\t" << v.data() << "\n";
    return strm.str();
  }
  std::string save_edge(graph_type::edge_type e) { return ""; }
}; // end of pagerank writer


double map_rank(const graph_type::vertex_type& v) { return v.data(); }


double pagerank_sum(graph_type::vertex_type v) {
  return v.data();
}

int main(int argc, char** argv) {
  // Initialize control plain using mpi
  graphlab::mpi_tools::init(argc, argv);
  graphlab::distributed_control dc;
  global_logger().set_log_level(LOG_INFO);

  // Parse command line options -----------------------------------------------
  graphlab::command_line_options clopts("PageRank algorithm.");
  std::string graph_dir;
  std::string format = "adj";
  std::string exec_type = "synchronous";
  clopts.attach_option("graph", graph_dir,
                       "The graph file.  If none is provided "
                       "then a toy graph will be created");
  clopts.add_positional("graph");
  clopts.attach_option("engine", exec_type,
                       "The engine type synchronous or asynchronous");
  clopts.attach_option("tol", TOLERANCE,
                       "The permissible change at convergence.");
  clopts.attach_option("format", format,
                       "The graph file format");
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
  std::string saveprefix;
  clopts.attach_option("saveprefix", saveprefix,
                       "If set, will save the resultant pagerank to a "
                       "sequence of files with prefix saveprefix");

  std::string result_file;
  clopts.attach_option("result_file", result_file,
                       "If set, will save the test result to the"
                       "specific file");

  if(!clopts.parse(argc, argv)) {
    dc.cout() << "Error in parsing command line arguments." << std::endl;
    return EXIT_FAILURE;
  }


  // Enable gather caching in the engine
  clopts.get_engine_args().set_option("use_cache", USE_DELTA_CACHE);

  if (ITERATIONS) {
    // make sure this is the synchronous engine
    dc.cout() << "--iterations set. Forcing Synchronous engine, and running "
              << "for " << ITERATIONS << " iterations." << std::endl;
    //clopts.get_engine_args().set_option("type", "synchronous");
    clopts.get_engine_args().set_option("max_iterations", ITERATIONS);
    clopts.get_engine_args().set_option("sched_allv", true);
  }

  int ntrials = 20;
  int trial_results1[20];
  double trial_results2[20][8];
  uint32_t seed_set[20] = {1492133106, 680965948, 2040586311, 73972395, 942196338, 819390547, 1643934785, 1707678784, 401305863, 1051761031, 956889080, 1387946621, 1523349375, 1620677309, 592759340, 1459650384, 1406812251, 349206043, 255545576, 1070228652};
  for(int i = 0; i < ntrials; i++)
  {
    // Build the graph ----------------------------------------------------------
    dc.cout() << "Loading graph." << std::endl;
	
	// random seed
	// dc.cout() << seed_set[i] << std::endl;
	clopts.get_graph_args().set_option("seed", seed_set[i]);
	
	// clopts.print();
	
    graph_type graph(dc, clopts);
	
    if(powerlaw > 0) { // make a synthetic graph
      dc.cout() << "Loading synthetic Powerlaw graph." << std::endl;
      graph.load_synthetic_powerlaw(powerlaw, alpha, beta, 100000000);
    }
    else if (graph_dir.length() > 0) { // Load the graph from a file
        if(dc.procid() == 24)
      std::cout << "Loading graph in format: "<< format << std::endl;
      graph.load_format(graph_dir, format);
    }
    else {
      dc.cout() << "graph or powerlaw option must be specified" << std::endl;
      clopts.print_description();
      return 0;
    }


//    dc.cout() << "Loading graph. Finished in "
//      << timer.current_time() << std::endl;
//    // must call finalize before querying the graph
//    dc.cout() << "Finalizing graph." << std::endl;
    graphlab::timer timer;
    timer.start();
    graph.finalize();
    const double ingress_time = timer.current_time();
    dc.cout() << "Finalizing graph. Finished in "
      << ingress_time << std::endl;

    dc.cout() << "#vertices: " << graph.num_vertices()
              << " #edges:" << graph.num_edges() << std::endl;

  //  graphlab::mpi_tools::finalize();
  //  return EXIT_SUCCESS;

	timer.start();
    // Initialize the vertex data
    graph.transform_vertices(init_vertex);

    // Running The Engine -------------------------------------------------------
    graphlab::synchronous_engine<pagerank> engine(dc, graph, clopts);
    engine.signal_all();
    engine.start();
  //  dc.cout() << "----------------------------------------------------------"
  //            << std::endl
  //            << "Final Runtime (seconds):   " << runtime
  //            << std::endl
  //            << "Updates executed: " << engine.num_updates() << std::endl
  //            << "Update Rate (updates/second): "
  //            << engine.num_updates() / runtime << std::endl;

    const double total_rank = graph.map_reduce_vertices<double>(map_rank);
	const double runtime = timer.current_time();
  //  if(dc.procid() == 0)
  //    std::cout << "Total rank: " << total_rank << std::endl;

    // Save the final graph -----------------------------------------------------
    if (saveprefix != "") {
      graph.save(saveprefix, pagerank_writer(),
                 false,    // do not gzip
                 true,     // save vertices
                 false);   // do not save edges
    }

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
          trial_results1[i] = graph.num_replicas();
          trial_results2[i][0] = (double)graph.num_replicas()/graph.num_vertices();
          trial_results2[i][1] = graph.get_edge_balance();
          trial_results2[i][2] = graph.get_vertex_balance() ;
		  trial_results2[i][3] = ingress_time;
          trial_results2[i][4] = runtime;
          trial_results2[i][5] = engine.get_exec_time();
          trial_results2[i][6] = engine.get_one_itr_time();
          trial_results2[i][7] = engine.get_compute_balance();

          if(result_file != "") {
              std::cout << "saving the result to " << result_file << std::endl;
              std::ofstream fout(result_file.c_str(), std::ios::app);
              if(powerlaw > 0) {
                  fout << "powerlaw synthetic: " << powerlaw << "\t"
                      << alpha << "\t" << beta << "\t" << dc.numprocs() << std::endl;
              }
              else {
                  fout << "real-world graph: " << graph_dir << "\t"
                      << graph.num_vertices() << "\t" << graph.num_edges()
                      << "\t" << dc.numprocs() << std::endl;
              }
              std::string ingress_method = "";
              clopts.get_graph_args().get_option("ingress", ingress_method);
              fout << ingress_method << "\t";
              fout << graph.num_replicas() << "\t"
                  << (double)graph.num_replicas()/graph.num_vertices() << "\t"
                  << graph.get_edge_balance() << "\t"
                  << graph.get_vertex_balance() << "\t"
                  << ingress_time << "\t"
				  << runtime << "\t"
                  << engine.get_exec_time() << "\t"
                  << engine.get_one_itr_time() << "\t"
                  << engine.get_compute_balance() << "\t"
                  << std::endl;
              fout.close();
          }
      }
  }

  if(dc.procid() == 0) {
    for (int i = 0; i < ntrials; i++) {
      std::cout << trial_results1[i] << "\t"
          << trial_results2[i][0] << "\t"
          << trial_results2[i][1] << "\t"
          << trial_results2[i][2] << "\t"
          << trial_results2[i][3] << "\t"
          << trial_results2[i][4] << "\t"
          << trial_results2[i][5] << "\t"
          << trial_results2[i][6] << "\t"
		  << trial_results2[i][7] << "\t"
          << std::endl;
    }
  }

  // Tear-down communication layer and quit -----------------------------------
  graphlab::mpi_tools::finalize();
  return EXIT_SUCCESS;
} // End of main


// We render this entire program in the documentation


