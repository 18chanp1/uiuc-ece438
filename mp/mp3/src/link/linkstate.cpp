#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <climits>
#include <queue>
#include <set>

#include "../graph.hpp"

/**
 * Routing class that implements the Link-state Routing Algorithm.
 */
class LinkstateRouting : public Graph {
 public:
  LinkstateRouting() : Graph() {}
  LinkstateRouting(std::vector<Edge> edges) : Graph(edges) {}

  /**
   *
   * @param src the node from which paths are started
   * @return A list of Forwarding table entries with cost.
   */
  std::vector<ForwardtableEntry> construct_fte(int src) override {
    std::set<int> known;
    std::map<int, int> distance;
    std::priority_queue<Edge, std::vector<Edge>, EdgeCompare> queue;
    std::map<int, int> predecessor;

    // known.insert(src);
    // distance[src] = 0;

    /** Initialization step **/
    for (int vertex : vertices) {
      if(vertex == src) {
        distance[vertex] = 0;
        predecessor[vertex] = src;
        known.insert(src);
        // queue.push(Edge(src, src, 0));
      }
      //set distance for adjacent nodes, add them to "items to be discovered"
      else if (adj_list[src].find(vertex) != adj_list[src].end()) {
        distance[vertex] = adj_list[src][vertex];
        predecessor[vertex] = src;
        queue.push(Edge(src, vertex, adj_list[src][vertex]));
      }
      //set distance to infinity.
      else {
        distance[vertex] = INT_MAX;
        predecessor[vertex] = -1;
      }
    }

    Edge edge;
    std::vector<ForwardtableEntry> result;
    while (!queue.empty()) {
      edge = queue.top();
      queue.pop();

      int current = edge.dst;

      // If current node is already known, skip this edge.
      if (known.find(current) != known.end()) {
        continue;
      }

      //mark the current node as known
      known.insert(current);

      for (auto neighbor : adj_list[current]) {
        int neighborId = neighbor.first;
        int neighborCost = neighbor.second;

        // If the new distance is less than the current distance, or the distances are equal but
        // the current path is lexicographically smaller, update the distance and predecessor.
        bool lessDistance = distance[current] + neighborCost < distance[neighborId];
        bool eqDistance = distance[current] + neighborCost == distance[neighborId];

        if (lessDistance || (eqDistance && current < predecessor[neighborId])) {
          distance[neighborId] = distance[edge.dst] + neighborCost;
          predecessor[neighborId] = edge.dst;
        }
        //add neighbor to queue, regardless of whether it was updated
        queue.push(Edge(edge.dst, neighbor.first, distance[neighbor.first]));
      }

    }



    //key is destination, value is next hop. Backtrack to find next hop.
    std::map<int, int> forwardingTable;
    for(auto const& x : predecessor) {
      int back = x.first;
      int oneLessThanBack = x.first;

      //while we haven't arrived at a dead end
      while (back != -1 && back != src) {
        //follow the path
        oneLessThanBack = back;
        back = predecessor[back];
      }

      //we are at the source. OneLessThanBack is the output port that we need
      forwardingTable[x.first] = oneLessThanBack;
    }


    //comment to self: the format is (src, dst, cost)
    //first is key (destination), second is value(cost)
    for (auto it = distance.begin(); it != distance.end(); ++it) {
      int neighborId = it->first;
      // If the neighbor is the source, the cost is 0
      if(neighborId == src) {
        result.push_back(ForwardtableEntry(src, src, 0, forwardingTable[src]));
      }
      //if it is unreachable, do not add it
      else if (it->second == INT_MAX || distance[it->first] == INT_MAX) {
          continue;
      }
      else {
        result.push_back(ForwardtableEntry(src, it->first, it->second, forwardingTable[it->first]));
      }
    }


    return result;
  };

  ~LinkstateRouting() {}
};

int main(int argc, char** argv) {
  Graph* graph;
  std::vector<Edge> edges;
  std::vector<Message> messages;
  std::vector<Edge> changes;
  std::ofstream out;

  //printf("Number of arguments: %d", argc);
  if (argc != 4) {
    printf("Usage: ./linkstate topofile messagefile changesfile\n");
    return -1;
  }

  out.open("output.txt");
  edges = parse_link_file(argv[1]);
  messages = parse_message_file(argv[2]);
  changes = parse_link_file(argv[3]);
  graph = new LinkstateRouting(edges);

  print_messages(graph, messages, out);
  for (Edge edge : changes) {
    graph->update_edge(edge);
    print_messages(graph, messages, out);
  }

  delete graph;
  out.close();
  return 0;
}
