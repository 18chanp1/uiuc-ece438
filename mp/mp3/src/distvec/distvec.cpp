#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <queue>
#include <set>


#include "../graph.hpp"

/**
 * Routing class that implements the Distance Vector Routing Algorithm.
 */
class DistanceVectorRouting : public Graph {
 public:
  DistanceVectorRouting() : Graph() {}
  DistanceVectorRouting(std::vector<Edge> edges) : Graph(edges) {}

  /**
 * Constructs the forwarding table for the graph using the Distance Vector Routing Algorithm.
 */
  std::vector<Edge> construct_paths(int src) {
    std::map<int, std::map<int, int>> distance;
    std::map<int, int> next_hop;
    // std::set<int> neighbors;
    std::set<int> known;
    std::set<int> visited;
    std::queue<int> queue;
    int node;

    for (int vertex : vertices) {
      distance[vertex][vertex] = 0;
      next_hop[vertex] = vertex;
    }

    for (auto neighbor : adj_list[src]) {
      distance[src][neighbor.first] = neighbor.second;
      next_hop[neighbor.first] = neighbor.first;
      // neighbors.insert(neighbor.first);
      queue.push(neighbor.first);
    }

    while (!queue.empty()) {
      node = queue.front();
      queue.pop();
      visited.insert(node);

      for (auto neighbor : adj_list[node]) {
        // can't find neighbor, or;
        // dist A = (source to current node dist) + (current node to neighbor)
        // dist b = source to neighbor distance
        if (distance[src].find(neighbor.first) == distance[src].end() ||
            distance[src][node] + neighbor.second < distance[src][neighbor.first]) {
          distance[src][neighbor.first] = distance[src][node] + neighbor.second;
          next_hop[neighbor.first] = next_hop[node];
        }

        //if we did not visit the neighbor, add it to the queue
        if (visited.find(neighbor.first) == visited.end()) {
          queue.push(neighbor.first);
        }
      }
    }

    std::vector<Edge> result;
    for (auto it = distance[src].begin(); it != distance[src].end(); ++it) {
      result.push_back(Edge(src, it->first, it->second));
    }

    return result;
  }

  std::vector<ForwardtableEntry> construct_fte(int src) {
    //rewrite distance vector algorithm
    /**
     * Key: The node for which the routing table is to be obtained
     * Value: The routing table for the node
     */
    std::map<int, std::map<int, ForwardtableEntry>> distanceTables;

    /**
     *Initialize the distance table for each node
     */
    for(int node : vertices) {
      std::map <int, ForwardtableEntry> currTable;
      // set every other node to infinity
      // for(int node2 : vertices) {
      //     currTable[node2] = -;
      // }

    //set the cost of itself to 0
      currTable[node] = ForwardtableEntry(node, node, 0, node);;


    //set the cost of its neighbors to the appropriate weight
    for(auto adjListEntry : adj_list[node]) {
      int neighborId = adjListEntry.first;
      int neighborWeight = adjListEntry.second;

      currTable[neighborId] = ForwardtableEntry(node, neighborId, neighborWeight, neighborId);
    }

      //initialization complete. Add the table to master table
      distanceTables[node] = currTable;
    }

    /**
     *simulate propagation by updating the tables one by one
     */
    std::queue<int> nodesToUpdate;
    for(int vertex : vertices) nodesToUpdate.push(vertex);

    while(!nodesToUpdate.empty()) {
      int current = nodesToUpdate.front();
      nodesToUpdate.pop();

      //update all values of current distance table
      bool updated = false;
      for(int targetNode : vertices) {
        //short circuit for self
        if(targetNode == current) continue;

        // iterate through neighbors to find the shortest path
        //check if the target node is in the current distance table
        bool currentHasPathToTarget = distanceTables[current].count(targetNode) > 0;
        int shortestPathToTarget = currentHasPathToTarget ? distanceTables[current][targetNode].cost : INT_MAX;
        int viaNeighbor = -1;

        //edge cases where target is your neighbor!
        if(currentHasPathToTarget) viaNeighbor = distanceTables[current][targetNode].next_hop;

        //copy and sort adjacency list


        for(auto adjListEntry : adj_list[current]) {
          int neighborId = adjListEntry.first;
          int neighborWeight = adjListEntry.second;

          //shorter path via neighbor discovered, condition:
          // 1. neighbor has a path to target
          // 2. the path is shorter than the current shortest path
          bool neighborHasPath = distanceTables[neighborId].count(targetNode);
          int neighborDistToTarget = neighborHasPath ? distanceTables[neighborId][targetNode].cost : INT_MAX;
          bool shorterPath = neighborHasPath &&
            (neighborWeight + distanceTables[neighborId][targetNode].cost) < shortestPathToTarget;
          bool eqPath = neighborHasPath &&
            (neighborWeight + distanceTables[neighborId][targetNode].cost) == shortestPathToTarget;
          bool lowerHopId = neighborId < viaNeighbor;
          if(neighborHasPath && ((shorterPath || (eqPath && lowerHopId)) || !currentHasPathToTarget)) {
            shortestPathToTarget = neighborWeight + neighborDistToTarget;
            viaNeighbor = neighborId;
            updated = true;
            currentHasPathToTarget = true;
          }
        }

        //we have checked all our neighbors
        //if the shortest path has changed, update the table, and;
        if(viaNeighbor != -1) {
          distanceTables[current][targetNode] = ForwardtableEntry(current, targetNode, shortestPathToTarget, viaNeighbor);
        }
      }

      //if the table was updated, add all neighbors to the queue (notify)
      if(updated) {
        for(auto adjListEntry : adj_list[current]) {
          int neighborId = adjListEntry.first;
          nodesToUpdate.push(neighborId);
        }
      }
    }

    //construct the forwarding table
    std::vector<ForwardtableEntry> result;
    for(auto distanceTable : distanceTables[src]) {
      int target = distanceTable.first;
      int cost = distanceTable.second.cost;
      int next_hop = distanceTable.second.next_hop;

      result.push_back(ForwardtableEntry(src, target, cost, next_hop));
    }

    return result;
  }

  ~DistanceVectorRouting() {}
};

int main(int argc, char** argv) {
  Graph* graph;
  std::vector<Edge> edges;
  std::vector<Message> messages;
  std::vector<Edge> changes;
  std::ofstream out;

  //printf("Number of arguments: %d", argc);
  if (argc != 4) {
    printf("Usage: ./distvec topofile messagefile changesfile\n");
    return -1;
  }

  edges = parse_link_file(argv[1]);
  messages = parse_message_file(argv[2]);
  changes = parse_link_file(argv[3]);
  graph = new DistanceVectorRouting(edges);
  out.open("output.txt");

  print_messages(graph, messages, out);
  for (Edge edge : changes) {
    graph->update_edge(edge);
    print_messages(graph, messages, out);
  }

  delete graph;
  out.close();
  return 0;
}
