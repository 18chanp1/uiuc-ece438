#ifndef MP3_SHARED_HPP
#define MP3_SHARED_HPP

#include <algorithm>
#include <climits>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

class Edge {
 public:
  int src;
  int dst;
  int cost;

  Edge() : src(-1), dst(-1), cost(-1) {}
  Edge(int src, int dst, int cost) : src(src), dst(dst), cost(cost) {}

  bool valid() { return cost >= 0; }

  ~Edge() {}
};

class ForwardtableEntry : public Edge {
public:
  int next_hop = -1;

  ForwardtableEntry() = default;

  ForwardtableEntry(int src, int dist, int cost, int next_hop):
    Edge(src, dist, cost), next_hop(next_hop) {}



};

struct EdgeCompare {
  /**
   * Compares two edges based on their cost.
   * 
   * If the costs are equal, the smaller destination is considered smaller.
   */
  bool operator()(const Edge& lhs, const Edge& rhs) const {
    return lhs.cost > rhs.cost || (lhs.cost == rhs.cost && lhs.dst > rhs.dst);
  }
};

struct EdgeSrcCompare {
  /**
   * Compares two edges based on their source.
   * 
   * If the sources are equal, the smaller destination is considered smaller.
   */
  bool operator()(const Edge& lhs, const Edge& rhs) const {
    return lhs.src < rhs.src || (lhs.src == rhs.src && lhs.dst < rhs.dst);
  }
};

/**
 * A generic graph class
 * 
 * The graph is represented with an adjacency list and a list of vertices.
 * 
 * The adjacency list is a map of maps, where the key is the vertex ID and the value is a map of neighbors and costs.
 * 
 * The list of vertices is a vector of vertex IDs.
 */
class Graph {
 public:
  std::set<int> vertices;
  std::map<int, std::map<int, int>> adj_list;
  int min_node;
  int max_node;

  Graph() {
    min_node = INT_MAX;
    max_node = INT_MIN;
  }

  Graph(std::vector<Edge> edges) {
    min_node = INT_MAX;
    max_node = INT_MIN;
    for (Edge edge : edges) {
      vertices.insert(edge.src);
      vertices.insert(edge.dst);
      adj_list[edge.dst][edge.src] = edge.cost;
      adj_list[edge.src][edge.dst] = edge.cost;
      min_node = std::min(min_node, std::min(edge.src, edge.dst));
      max_node = std::max(max_node, std::max(edge.src, edge.dst));
    }
  }

  virtual ~Graph() {
    for (auto it = adj_list.begin(); it != adj_list.end(); ++it) {
      it->second.clear();
    }
    adj_list.clear();
    vertices.clear();
  }

  void update_edge(Edge edge) {
    if (edge.valid()) {
      adj_list[edge.src][edge.dst] = edge.cost;
      adj_list[edge.dst][edge.src] = edge.cost;
      vertices.insert(edge.src);
      vertices.insert(edge.dst);
    } else {
      adj_list[edge.src].erase(edge.dst);
      adj_list[edge.dst].erase(edge.src);
      // if (adj_list[edge.src].empty()) {
      //   vertices.erase(edge.src);
      // }
      // if (adj_list[edge.dst].empty()) {
      //   vertices.erase(edge.dst);
      // }
    }
  }

  /**
 * Constructs the forwarding table for the graph.
 * 
 * @param src The source node to construct the forwarding table for.
 */
  virtual std::vector<Edge> construct_paths(int src) {
    std::cout << "Not implemented: " << src << std::endl;
    throw std::runtime_error("Not implemented construct_paths");
  };

  /**
  * Constructs the forwarding table for the graph.
  * @param The source node to construct the forwarding table for
  */
  virtual std::vector<ForwardtableEntry> construct_fte(int src) {
    std::cout << "Not implemented: " << src << std::endl;
    throw std::runtime_error("Not implemented construct_fte");
  };
};

class Message {
 public:
  int src;
  int dst;
  std::string text;
  Message(int src, int dst, std::string text)
      : src(src), dst(dst), text(text) {}
};

/**
 * Parses a link file and returns a vector of links.
 *
 * The link file format is as follows:
 * <src node ID> <dest node ID> <cost> 
 *
 * @param filename The name of the file to parse.
 */
static std::vector<Edge> parse_link_file(char* filename) {
  std::vector<Edge> edges;
  std::string line;
  int src, dst, cost;
  std::ifstream file(filename);

  if (!file.is_open()) {
    std::cerr << "Error: could not open file " << filename << std::endl;
    exit(1);
  }

  while (std::getline(file, line)) {
    sscanf(line.c_str(), "%d %d %d", &src, &dst, &cost);
    edges.push_back(Edge(src, dst, cost));
  }

  return edges;
}

/**
 * Parses a message file and returns a vector of messages.
 *
 * The message file format is as follows:
 * <src node ID> <dest node ID> <message text> 
 *
 * @param filename The name of the file to parse.
 */
static std::vector<Message> parse_message_file(char* filename) {
  std::vector<Message> messages;
  std::string line;
  std::ifstream file(filename);

  if (!file.is_open()) {
    std::cerr << "Error: could not open file " << filename << std::endl;
    exit(1);
  }

  while (std::getline(file, line)) {
    std::string src, dst, text;

    src = line.substr(0, line.find(' '));
    line = line.substr(line.find(' ') + 1);
    dst = line.substr(0, line.find(' '));
    text = line.substr(line.find(' ') + 1);

    messages.push_back(
        Message(std::stoi(src), std::stoi(dst), text));
  }

  return messages;
}

static void print_edges(std::vector<ForwardtableEntry> edges, std::ostream& out) {
  std::sort(edges.begin(), edges.end(), EdgeSrcCompare());

  for (ForwardtableEntry edge : edges) {
    //TODO the format should be destination, nexthop, cost
    out << edge.dst << " " << edge.next_hop << " " << edge.cost << std::endl;
  }

  // out << std::endl;
}

/**
 * Prints the messages to the output stream.
 *
 * @param g The graph to use for the messages.
 * @param messages The messages to print.
 * @param out The output stream to print to.
 */
static void print_messages(Graph* g, std::vector<Message> messages,
                           std::ostream& out) {
  std::map<int, std::vector<ForwardtableEntry>> forwarding_table;
  std::vector<int> nodes;
  int src, dst;

  for (auto node: g->vertices) {
    forwarding_table[node] = g->construct_fte(node);
    nodes.push_back(node);
  }
  std::sort(nodes.begin(), nodes.end());
  for (int node : nodes) {
    print_edges(forwarding_table[node], out);
  }

  for (Message message : messages) {
    // Find the shortest path from the source to the destination
    src = message.src;
    dst = message.dst;

    //look through forwarding table to find the destination
    std::vector<ForwardtableEntry> srcTable = forwarding_table[src];
    bool found = false;
    for(auto edge : srcTable) {
      if(edge.dst == dst) {
        out << "from " << src << " to " << dst << " cost " << edge.cost;
        found = true;
        break;
      }
    }

    if(!found) {
      out << "from " << src << " to " << dst << " cost infinite";
      out << " hops unreachable " << "message " << message.text << std::endl;
      continue;
    }


    out << " hops ";
    std::set<int> visited;
    
    while (src != dst && src != -1) {
      out << src << " ";
      visited.insert(src);
      int lowest_cost = INT_MAX;

      //go through current table to find the next hop
      std::vector<ForwardtableEntry> currentTable = forwarding_table[src];
      bool found = false;
      for(const auto& fte: currentTable) {
        if(fte.dst == dst) {
          src = fte.next_hop;
          found = true;
        }
      }
      //could not find it
      if(!found) {
        out << "unreachable " << "message " << message.text << std::endl;
        break;
      }
      // out << src << " ";
    }
    out << "message " << message.text << std::endl;
  }

  // out << std::endl;
}

#endif
