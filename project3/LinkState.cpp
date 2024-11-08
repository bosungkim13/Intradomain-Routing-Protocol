#include "LinkState.h"
#include <cassert>
#include <unordered_set>

LinkState::LinkState(Node* n, router_id id, adjacencyList_ref adjList, portStatus_ref portStatus, forwardingTable_ref forwardingTable, port_num numPorts) 
    : sys(n), myRouterID(id), adjacencyList(adjList), portStatus(portStatus), forwardingTable(forwardingTable), numPorts(numPorts), seqNum(0) {
}

Packet LinkState::CreatePacket(unsigned short size) {
    // TODO: do i need to malloc here? 
    Packet packet;
    packet.header.packetType = LS;
    packet.header.size = size;
    packet.header.sourceID = this->myRouterID;
    // Destination ID is not used in LS packet because it will flood to all neighbors
    packet.header.destID = 0;
    
    memcpy(packet.payload, &this->seqNum, sizeof(this->seqNum));
    int offset = sizeof(this->seqNum); // Update index to start after seqNum
    for (auto nbr: this->adjacencyList) {
        auto neighborID = nbr.first;
        auto cost = nbr.second.timeCost;

        // Add neighbor ID and cost to the payload
        memcpy(packet.payload + offset, &neighborID, sizeof(neighborID));
        offset += sizeof(neighborID);
        memcpy(packet.payload + offset, &cost, sizeof(cost));
        offset += sizeof(cost);
    }
    assert(offset <= MAX_PAYLOAD_SIZE);
    return packet;
}

void LinkState::SendUpdates() {
    // Send updates to all neighbors
    // Each neighbor will have its port number and cost (2 + 2 = 4 bytes)
    // Header size is 12 bytes and seqNum is 4 bytes which is start of payload
    unsigned int size = adjacencyList.size() * 4 + HEADER_SIZE + sizeof(seqNum);
    assert(12 == HEADER_SIZE);

    for (port_num portId = 0; portId < this->numPorts; portId++) {
        auto pit = portStatus.find(portId);

        // continue if port is detached
        if (pit == portStatus.end() || !(pit->second.isUp)) {
            continue;
        }

        Packet packet = this->CreatePacket(size);
        void* msg = serializePacket(packet);
        sys->send(portId, msg, size);
    }
}

void LinkState::FloodUpdates(port_num myPort, void* floodMe, unsigned short size) {
    for (port_num portId = 0; portId < this->numPorts; portId++) {
        if (portId == myPort) {
            continue;
        }
        char *copy = strdup(static_cast<char *>(floodMe));
        sys->send(portId, copy, size);
    }
    free(floodMe);
}

void LinkState::UpdateTable() {
    // destination router : <current router ID, cost>
    unordered_map<router_id, std::pair<router_id, cost>> activeDistances;

    // adjacent routers to current router
    for (auto entry = portStatus.begin(); entry != portStatus.end(); entry++) {
        PortStatusEntry pEntry = entry->second;
        if (pEntry.isUp) {
            activeDistances[pEntry.destRouterID] = std::make_pair(this->myRouterID, pEntry.timeCost);
        }
        
    }

    unordered_set<router_id> toVisit;
    // all other routers
    for (auto entry = costTable.begin(); entry != costTable.end(); entry++) {
        router_id destId = entry->first;
        if (destId != this->myRouterID) {
            toVisit.insert(destId);
            if (activeDistances.find(destId) == activeDistances.end()) {
                // this destination is not connected to the current router
                activeDistances[destId] = make_pair(this->myRouterID, INFINITY_COST);
            }
        }
    }

    // DIJKSTRA
    router_id currRouterId;
    cost minCost;

    while (!toVisit.empty()) {
        minCost = INFINITY_COST;
        for (auto id: toVisit) {
            if (activeDistances[id].second < minCost) {
                currRouterId = id;
                minCost = activeDistances[id].second;
            }
        }

        if (minCost == INFINITY_COST) {
            // There are no paths to current root router
            break;
        }
        toVisit.erase(currRouterId);

        auto currRouterCost = costTable[currRouterId];
        for (auto id: currRouterCost) {
            router_id neighborId = id.first;
            cost neighborCost = id.second;
            if (toVisit.find(neighborId) != toVisit.end() && neighborCost + minCost < activeDistances[neighborId].second) {
                activeDistances[neighborId] = make_pair(currRouterId, neighborCost + minCost);
            }
        }


    }

    // Update the forwarding table and all the info in nodeTable
    // Update distances for neighbors
    ls_path_info pathInfo = nodeTable[currRouterId];
    for (auto distance : activeDistances) {
        router_id destId = distance.first;
        pair<router_id, cost> oldDistance = distance.second;
        if (distance.first != currRouterId) {
            ls_path_info newPathInfo(destId, sys->time(), oldDistance.second);
            nodeTable[destId] = newPathInfo;
            // update forwarding table if cheaper path is found
            forwardingTable[destId] = this->FindNextHop(activeDistances, destId);
        }
    }
}

router_id LinkState::FindNextHop(unordered_map<router_id, std::pair<router_id, cost>> activeDistances, router_id destId) {
    router_id currDest = destId;
    // start at destination and find where do I need to hop next?
    while (activeDistances[currDest].first != this->myRouterID) {
        currDest = activeDistances[currDest].first;
    }
    return currDest;
}

