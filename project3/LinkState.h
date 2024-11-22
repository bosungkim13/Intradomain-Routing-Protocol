#ifndef LINKSTATE_H
#define LINKSTATE_H

#include "global.h"
#include "sharedUtils.h"
#include "lsUtils.h"
#include "Node.h"

class LinkState {
    public:
        void UpdateTable();
        // Function that sends this router's updates to all neighbors
        void SendUpdates();
        // Function that forwards updates to all neighbors. This function is called when an update is received from a neighbor.
        void FloodUpdates(port_num myPort, void* floodMe, unsigned short size);
        // Function that creates a LS packet
        Packet CreatePacket(unsigned short size);
        // Function to handle LS packets
        void HandlePacket(port_num portId, void* packet, unsigned short size);
        // Function to find the next hop for a given destination
        router_id FindNextHop(unordered_map<router_id, std::pair<router_id, cost> > activeDistances, router_id destId);

        ls_packet_info DeserializeLSPacket(void* deserializeMe);

        bool NeedCostUpdated(router_id nbrId, unordered_map<router_id, cost> neighborCostTable);

        LinkState();
        LinkState(Node* n, router_id id, adjacencyList_ptr adjList, portStatus_ptr portStatus, forwardingTable_ptr forwardingTable, port_num numPorts);

        bool PortExpiredCheck();

        // check if entry in node table has expired and removes it
        bool NodeTableExpiredCheck();

        void printNodeTable() {
            std::cout << "------------------" << std::endl;
            std::cout << this->myRouterID << std::endl;
            for (auto it: this->nodeTable) {
                std::cout << "Node " << it.first << " has cost " << it.second.timeCost << " and last update " << it.second.lastUpdate << " and port " << it.second.myID << std::endl;
            }
            std::cout << "------------------" << std::endl;
        }

        void printAdjList() {
            std::cout << "Adjacency List:" << std::endl;
            for (auto it = adjacencyList->begin(); it != adjacencyList->end(); ++it) {
                Neighbor n = it->second;
                std::cout << it->first << " -> " << n.timeCost << " via " << n.port << std::endl;
            }
        }

        void printForwardingTable() {
            std::cout << "Forwarding Table:" << std::endl;
            for (auto it = forwardingTable->begin(); it != forwardingTable->end(); ++it) {
                std::cout << it->first << " -> " << it->second << std::endl;
            }
        }

        void printCostTable() {
            std::cout << "Cost Table:" << std::endl;
            for (const auto& router : costTable) {
                std::cout << "Router " << router.first << " neighbors:" << std::endl;
                for (const auto& neighbor : router.second) {
                    std::cout << "  -> " << neighbor.first << " (cost: " << neighbor.second << ")" << std::endl;
                }
            }
        }

        void printTables() {
            std::cout << "printTables()" << std::endl;
            this->printNodeTable();
            this->printCostTable();
            this->printForwardingTable();
            this->printAdjList();
        }

        void RefreshNodeEntry(router_id destId) {
            this->nodeTable[destId].lastUpdate = this->sys->time();
        }

        void removeNodeFromCostTable(router_id downNode) {
            if (this->costTable.find(downNode) != this->costTable.end()) {
                this->costTable.erase(downNode); // Remove the entire mapping for downNode
            }

            // Remove all incoming edges to downNode
            auto it = this->costTable.begin();
            bool foundOtherDownNode = false;
            while (it != this->costTable.end()) {
                auto& neighbors = it->second;
                auto neighborIt = neighbors.find(downNode);

                if (neighborIt != neighbors.end()) {
                    neighbors.erase(neighborIt); // Remove reference to downNode
                    foundOtherDownNode = true;
                }
                // advance
                ++it;
            }
        }
    private:
        router_id myRouterID;
        // neighbor router ID -> neighbor router cost
        adjacencyList_ptr adjacencyList;
        // port number -> PortStatusEntry which contains nbrid
        portStatus_ptr portStatus;
        forwardingTable_ptr forwardingTable;

        // map from router to previous router and cost of best current path
        unordered_map<router_id, ls_path_info> nodeTable;
        // map from router to next router and cost. to be updated by the pong handling
        unordered_map<router_id, unordered_map<router_id, cost> > costTable;
        // map from router to sequence number
        unordered_map<router_id, seq_num> seqTable;


        Node* sys;
        port_num numPorts;
        unsigned int seqNum = 0;
};

#endif