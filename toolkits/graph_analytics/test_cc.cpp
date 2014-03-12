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

#include <string>
#include <iostream>
#include <algorithm>
#include <vector>
#include <map>
#include <boost/unordered_map.hpp>
#include <time.h>

#include <graphlab.hpp>
#include <graphlab/graph/distributed_graph.hpp>

struct vdata {
  uint64_t labelid;
  vdata() :
      labelid(0) {
  }

  void save(graphlab::oarchive& oarc) const {
    oarc << labelid;
  }
  void load(graphlab::iarchive& iarc) {
    iarc >> labelid;
  }
};

typedef graphlab::distributed_graph<vdata, graphlab::empty> graph_type;

//set label id at vertex id
void initialize_vertex(graph_type::vertex_type& v) {
  v.data().labelid = v.id();
}

//message where summation means minimum
struct min_message {
  uint64_t value;
  explicit min_message(uint64_t v) :
      value(v) {
  }
  min_message() :
      value(std::numeric_limits<uint64_t>::max()) {
  }
  min_message& operator+=(const min_message& other) {
    value = std::min<uint64_t>(value, other.value);
    return *this;
  }

  void save(graphlab::oarchive& oarc) const {
    oarc << value;
  }
  void load(graphlab::iarchive& iarc) {
    iarc >> value;
  }
};

class label_propagation: public graphlab::ivertex_program<graph_type, size_t,
    min_message>, public graphlab::IS_POD_TYPE {
private:
  size_t recieved_labelid;
  bool perform_scatter;
public:
  label_propagation() {
    recieved_labelid = std::numeric_limits<size_t>::max();
    perform_scatter = false;
  }

  //receive messages
  void init(icontext_type& context, const vertex_type& vertex,
      const message_type& msg) {
    recieved_labelid = msg.value;
  }

  //do not gather
  edge_dir_type gather_edges(icontext_type& context,
      const vertex_type& vertex) const {
    return graphlab::NO_EDGES;
  }
  size_t gather(icontext_type& context, const vertex_type& vertex,
      edge_type& edge) const {
    return 0;
  }

  //update label id. If updated, scatter messages
  void apply(icontext_type& context, vertex_type& vertex,
      const gather_type& total) {
    if (recieved_labelid == std::numeric_limits<size_t>::max()) {
      perform_scatter = true;
    } else if (vertex.data().labelid > recieved_labelid) {
      perform_scatter = true;
      vertex.data().labelid = recieved_labelid;
    }
  }

  edge_dir_type scatter_edges(icontext_type& context,
      const vertex_type& vertex) const {
    if (perform_scatter)
      return graphlab::ALL_EDGES;
    else
      return graphlab::NO_EDGES;
  }

  //If a neighbor vertex has a bigger label id, send a massage
  void scatter(icontext_type& context, const vertex_type& vertex,
      edge_type& edge) const {
    if (edge.source().id() != vertex.id()
        && edge.source().data().labelid > vertex.data().labelid) {
      context.signal(edge.source(), min_message(vertex.data().labelid));
    }
    if (edge.target().id() != vertex.id()
        && edge.target().data().labelid > vertex.data().labelid) {
      context.signal(edge.target(), min_message(vertex.data().labelid));
    }
  }
};

class graph_writer {
public:
  std::string save_vertex(graph_type::vertex_type v) {
    std::stringstream strm;
    strm << v.id() << "," << v.data().labelid << "\n";
    return strm.str();
  }
  std::string save_edge(graph_type::edge_type e) {
    return "";
  }
};

int main(int argc, char** argv) {
//  std::cout << "Connected Component\n\n";

  graphlab::mpi_tools::init(argc, argv);
  graphlab::distributed_control dc;
  global_logger().set_log_level(LOG_DEBUG);
  //parse options
  graphlab::command_line_options clopts("Connected Component.");
  std::string graph_dir;
  std::string saveprefix;
  std::string format = "snap";
  std::string exec_type = "synchronous";
  clopts.attach_option("graph", graph_dir,
                       "The graph file. This is not optional");
  clopts.add_positional("graph");
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
  size_t ITERATIONS = 0;
  clopts.attach_option("iterations", ITERATIONS,
                       "If set, will force the use of the synchronous engine"
                       "overriding any engine option set by the --engine parameter. "
                       "Runs complete (non-dynamic) PageRank for a fixed "
                       "number of iterations. Also overrides the iterations "
                       "option in the engine");
  clopts.attach_option("saveprefix", saveprefix,
                       "If set, will save the pairs of a vertex id and "
                       "a component id to a sequence of files with prefix "
                       "saveprefix");
  std::string result_file;
  clopts.attach_option("result_file", result_file,
                       "If set, will save the test result to the"
                       "specific file");
  if (!clopts.parse(argc, argv)) {
    dc.cout() << "Error in parsing command line arguments." << std::endl;
    return EXIT_FAILURE;
  }

  if (ITERATIONS) {
    // make sure this is the synchronous engine
    dc.cout() << "--iterations set. Forcing Synchronous engine, and running "
              << "for " << ITERATIONS << " iterations." << std::endl;
    //clopts.get_engine_args().set_option("type", "synchronous");
    clopts.get_engine_args().set_option("max_iterations", ITERATIONS);
    clopts.get_engine_args().set_option("sched_allv", true);
  }

  graph_type graph(dc, clopts);

  //load graph
  dc.cout() << "Loading graph in format: "<< format << std::endl;
  if(powerlaw > 0) { // make a synthetic graph
    dc.cout() << "Loading synthetic Powerlaw graph." << std::endl;
    graph.load_synthetic_powerlaw(powerlaw, alpha, beta, 100000000);
  }
  else if (graph_dir.length() > 0) { // Load the graph from a file
    dc.cout() << "Loading graph in format: "<< format << std::endl;
    graph.load_format(graph_dir, format);
  }
  else {
    dc.cout() << "graph or powerlaw option must be specified" << std::endl;
    clopts.print_description();
    return 0;
  }
  graphlab::timer ti;
  ti.start();
  graph.finalize();
  const double ingress_time = ti.current_time();
  dc.cout() << "Finalizing graph. Finished in "
    << ingress_time << std::endl;

  dc.cout() << "#vertices: " << graph.num_vertices()
            << " #edges:" << graph.num_edges() << std::endl;
  graph.transform_vertices(initialize_vertex);

  //running the engine
  graphlab::synchronous_engine<label_propagation> engine(dc, graph, clopts);
  engine.signal_all();
  ti.start();
  engine.start();
  const double runtime = ti.current_time();

  //write results
  if (saveprefix.size() > 0) {
    graph.save(saveprefix, graph_writer(),
        false, //set to true if each output file is to be gzipped
        true, //whether vertices are saved
        false); //whether edges are saved
  }

  if(dc.procid() == 0) {
      std::cout << graph.num_replicas() << "\t"
                << (double)graph.num_replicas()/graph.num_vertices() << "\t"
                << graph.get_edge_balance() << "\t"
                << graph.get_vertex_balance() << "\t"
                << ingress_time << "\t"
                << engine.get_exec_time() << "\t"
                << engine.get_one_itr_time() << "\t"
                << engine.get_compute_balance() << "\t"
                << std::endl;
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
              << engine.get_exec_time() << "\t"
              << engine.get_one_itr_time() << "\t"
              << engine.get_compute_balance() << "\t"
              << std::endl;
          fout.close();
      }
  }

  graphlab::mpi_tools::finalize();

  return EXIT_SUCCESS;
}

