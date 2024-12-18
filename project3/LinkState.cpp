#include "LinkState.h"
#include <cassert>
#include <unordered_set>


LinkState::LinkState() : sys(nullptr), myRouterID(0), adjacencyList(nullptr), portStatus(nullptr), forwardingTable(nullptr), numPorts(0), seqNum(0) {}

LinkState::LinkState(Node* n, router_id id, adjacencyList_ptr adjList, portStatus_ptr portStatus, forwardingTable_ptr forwardingTable, port_num numPorts) : sys(n), myRouterID(id), adjacencyList(adjList), portStatus(portStatus), forwardingTable(forwardingTable), numPorts(numPorts), seqNum(0) {}

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
    for (auto nbr: (*this->adjacencyList)) {
        auto neighborID = nbr.first;
        auto nbrCost = nbr.second.timeCost;
        assert(nbrCost != INFINITY_COST);
        // Add neighbor ID and cost to the payload
        memcpy(packet.payload + offset, &neighborID, sizeof(neighborID));
        offset += sizeof(neighborID);
        memcpy(packet.payload + offset, &nbrCost, sizeof(cost));
        offset += sizeof(cost);
    }
    assert(offset <= MAX_PAYLOAD_SIZE);
    return packet;
}

void LinkState::SendUpdates() {
    // Send updates to all neighbors
    // Each neighbor will have its port number and cost (2 + 2 = 4 bytes)
    // Header size is 12 bytes and seqNum is 4 bytes which is start of payload
    unsigned int size = this->adjacencyList->size() * 4 + HEADER_SIZE + sizeof(seqNum);
    assert(8 == HEADER_SIZE);

    for (port_num portId = 0; portId < this->numPorts; portId++) {
        auto pit = this->portStatus->find(portId);

        // continue if port is detached
        if (pit == this->portStatus->end() || !(pit->second.isUp)) {
            continue;
        }

        Packet packet = this->CreatePacket(size);
        void* msg = serializePacket(packet);
        this->sys->send(portId, msg, size);
    }
    this->seqNum++;
}

void LinkState::FloodUpdates(port_num myPort, void* floodMe, unsigned short size) {
    for (port_num portId = 0; portId < this->numPorts; portId++) {
        if (portId == myPort) {
            continue;
        }
        char* copy = static_cast<char*>(malloc(size));    // Allocate exact size
        memcpy(copy, floodMe, size);  
        sys->send(portId, copy, size);
    }
    free(floodMe);
}

// must call checkTableExpired() before calling this function
void LinkState::UpdateTable() {
    // destination router : <current router ID, cost>
    unordered_map<router_id, std::pair<router_id, cost> > activeDistances;

    // adjacent routers to current router
    for (auto entry = this->portStatus->begin(); entry != this->portStatus->end(); entry++) {
        PortStatusEntry pEntry = entry->second;

        // TODO: This is likely where we are going wrong with routing. Adjacency list could not be updated
        if (pEntry.isUp && (*this->adjacencyList).find(pEntry.destRouterID) != (*this->adjacencyList).end()) {
            activeDistances[pEntry.destRouterID] = std::make_pair(this->myRouterID, pEntry.timeCost);
        }
        
    }

    unordered_set<router_id> toVisit;
    // all other routers

    // NOTE: Changed to use nodeTable instead of costTable because costTable is harder to find active routers
    for (auto entry = nodeTable.begin(); entry != nodeTable.end(); entry++ ) {
        router_id destId = entry->first;
        if (destId != this->myRouterID) {
            toVisit.insert(destId);
            if (activeDistances.find(destId) == activeDistances.end()) {
                activeDistances[destId] = make_pair(this->myRouterID, INFINITY_COST);
            }
        }
    }

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

        // Here we can iterate through portStatus to get neighbor ids then query nodeTable to get cost if we dont use costTable

        auto currRouterCost = costTable[currRouterId];
        for (auto id: currRouterCost) {
            router_id neighborId = id.first;
            if (neighborId == this->myRouterID) {
                continue;
            }
            cost neighborCost = id.second;
            if (toVisit.find(neighborId) != toVisit.end() && activeDistances.find(neighborId) != activeDistances.end() && neighborCost + minCost < activeDistances[neighborId].second) {
                activeDistances[neighborId] = make_pair(currRouterId, neighborCost + minCost);
            } 
        }


    }

    // Update the forwarding table and all the info in nodeTable
    // Update distances for neighbors
    for (auto distance : activeDistances) {
        router_id destId = distance.first;
        pair<router_id, cost> oldDistance = distance.second;
        if (destId != this->myRouterID) {
            ls_path_info newPathInfo(destId, sys->time(), oldDistance.second);
            nodeTable[destId] = newPathInfo;
            // update forwarding table if cheaper path is found
            (*forwardingTable)[destId] = this->FindNextHop(activeDistances, destId);
        }
    }
}

router_id LinkState::FindNextHop(unordered_map<router_id, std::pair<router_id, cost> > activeDistances, router_id destId) {
    router_id currDest = destId;
    // start at destination and find where do I need to hop next?
    while (activeDistances[currDest].first != this->myRouterID) {
        currDest = activeDistances[currDest].first;
    }

    return currDest;
}

ls_packet_info LinkState::DeserializeLSPacket(void* deserializeMe) {
    Packet packet;
    ls_packet_info info;
    
    // Copy the header back
    memcpy(&packet.header, deserializeMe, sizeof(PacketHeader));
    
    // Convert header fields from network to host byte order
    packet.header.size = ntohs(packet.header.size);
    packet.header.sourceID = ntohs(packet.header.sourceID);
    info.sourceId = packet.header.sourceID;

    // Read the sequence number
    memcpy(&info.seqNum, (char*)deserializeMe + sizeof(PacketHeader), sizeof(seq_num));
    info.seqNum = ntohl(info.seqNum);

    // Reader neioghbor id and cost
    int offset = sizeof(PacketHeader) + sizeof(seq_num);
    while (offset < packet.header.size) {
        router_id neighborID;
        cost neighborCost;

        memcpy(&neighborID, (char*)deserializeMe + offset, sizeof(router_id));
        neighborID = ntohs(neighborID); // Convert from network to host byte order
        offset += sizeof(router_id);

        memcpy(&neighborCost, (char*)deserializeMe + offset, sizeof(cost));
        neighborCost = ntohs(neighborCost); // Convert from network to host byte order
        offset += sizeof(cost);

        // Store the neighbor ID and cost in the costTable
        info.costTable[neighborID] = neighborCost;
    }
    return info;
}

void  LinkState::HandlePacket(port_num portId, void* handleMe, unsigned short size) {
    ls_packet_info info = DeserializeLSPacket(handleMe);
    unordered_map<router_id, cost> incomingCostTable = info.costTable;
    seq_num incomingSeqNum = info.seqNum;
    router_id incomingSourceId = info.sourceId;
    if (this->seqTable.find(incomingSourceId) == this->seqTable.end() || (this->seqTable.find(incomingSourceId) != this->seqTable.end() && incomingSeqNum > this->seqTable[incomingSourceId])) {
        // if source id is not in table or incoming seq num is greater than current seq num
        this->seqTable[incomingSourceId] = incomingSeqNum;
        this->RefreshNodeEntry(incomingSourceId);
        if (this->NeedCostUpdated(incomingSourceId, incomingCostTable)) {
            this->costTable[incomingSourceId] = incomingCostTable;
            this->UpdateTable();
        }
        // Send updates to all neighbors
        this->FloodUpdates(portId, handleMe, size);
    } else {
        free(handleMe);
    }
}

bool LinkState::NeedCostUpdated(router_id nbrId, unordered_map<router_id, cost> neighborCostTable) {
    // cost table does not contain this neighbor yet
    if (this->costTable.find(nbrId) == this->costTable.end()) {
        return !neighborCostTable.empty();
    }
    else
    {
        // cost table entry exists
        unordered_map<router_id, cost> currEntry = this->costTable[nbrId];
        if (currEntry.size() != neighborCostTable.size()) {
            return true;
        } else {
            for (auto it : neighborCostTable) {
                router_id neighbor_id = it.first;
                cost neighbor_cost = it.second;
                if (currEntry.find(neighbor_id) == currEntry.end()) {
                    // don' have this neighbor yet
                    return true;
                }
                else {
                    // have neighbor cost but it is different
                    if (currEntry[neighbor_id] != neighbor_cost) {
                        return true;
                    }
                }
            }
        }
        return false;
    }
}

bool LinkState::PortExpiredCheck() {
    bool expired = false;
    for (auto it = this->portStatus->begin(); it != this->portStatus->end(); it++) {
        if (this->sys->time() - it->second.lastUpdate > 15 * 1000) {
            it->second.timeCost = INFINITY_COST;
            it->second.isUp = false;
            expired = true;
            if (this->adjacencyList->find(it->second.destRouterID) != this->adjacencyList->end()) {
                this->adjacencyList->erase(it->second.destRouterID);
                this->removeNodeFromCostTable(it->second.destRouterID);
                if (this->nodeTable.find(it->second.destRouterID) != this->nodeTable.end()) {
                    this->nodeTable[it->second.destRouterID].timeCost = INFINITY_COST;
                }
            }
        }
    }
    return expired;
}

bool LinkState::NodeTableExpiredCheck() {
    unsigned int currTime = this->sys->time();
    bool expired = false;
    router_id destId;
    router_id sourceId;
    auto it = this->nodeTable.begin();
    
    while (it != this->nodeTable.end()){
        currTime = this->sys->time();
        ls_path_info& pathInfo = it->second;
        if (currTime - pathInfo.lastUpdate >= 45 * 1000 && pathInfo.timeCost != INFINITY_COST) {
            expired = true;
            destId = it->first;
            sourceId = pathInfo.myID;
            pathInfo.timeCost = INFINITY_COST;
            this->removeNodeFromCostTable(destId);
        } 
        it++;
    }
    return expired;
}