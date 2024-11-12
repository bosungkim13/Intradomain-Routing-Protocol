#include "RoutingProtocolImpl.h"
#include "global.h"
#include <sys/time.h>
#include "sharedUtils.h"
#include <cassert>

RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
  sys = n;
  // add your own code
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
  // add your own code (if needed)
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
  this->numPorts = num_ports;
  this->routerID = router_id;
  this->protocolType = protocol_type;

  char *ppAlarm = new char[sizeof(eAlarmType)];
  char *updateAlarm = new char[sizeof(eAlarmType)];
  char *freshnessAlarm = new char[sizeof(eAlarmType)];

  *((eAlarmType *)ppAlarm) = PingPongAlarm;
  *((eAlarmType *)updateAlarm) = UpdateAlarm;
  *((eAlarmType *)freshnessAlarm) = FreshnessAlarm;

  sendPings();

  this->sys->set_alarm(this, 1 * 1000, freshnessAlarm);
  this->sys->set_alarm(this, 10 * 1000, ppAlarm);
  this->sys->set_alarm(this, 30 * 1000, updateAlarm);

  // initialize link or distance vector protocol
  if (this->protocolType == P_LS) {
    this->myLSRP = LinkState(this->sys, this->routerID, &this->adjacencyList, &this->portStatus, &this->forwardingTable, this->numPorts);
  } else if (this->protocolType == P_DV) {
    this->myDV = DistanceVector(this->sys, this->routerID, &this->adjacencyList, &this->portStatus, this->numPorts);
  }

}

void RoutingProtocolImpl::handle_alarm(void *data) {
  // add your own code
  eAlarmType type = (*((eAlarmType *)data));
  switch (type) {
    case PingPongAlarm:
      this->sendPings();
      this->sys->set_alarm(this, 10 * 1000, data);
      break;
    case UpdateAlarm:
      if (protocolType == P_LS) {
        // LS
        this->myLSRP.SendUpdates();
      } else {
        // DV
        this->myDV.sendUpdates();
      }
      this->sys->set_alarm(this, 30 * 1000, data);
      break;
    case FreshnessAlarm:
      // check link every second
      if (protocolType == P_LS) {
        if (this->myLSRP.PortExpiredCheck()) {
          // port expired check updates the table so send updates doesn't send to dead ports
          this->myLSRP.SendUpdates();
        }

        // update freshness
        if (this->myLSRP.NodeTableExpiredCheck()) {
          // updates routing table based on current link state table
          this->myLSRP.UpdateTable();
        }

      } else {
        // DV

        // update port freshness
        if (this->myDV.portExpiredCheck()) {
          // port expired check updates the ports table so send updates doesn't send to dead ports
          this->myDV.sendUpdates();
        }

        // update DV entry freshness
        if (this->myDV.dvEntryExpiredCheck()) {
          this->myDV.sendUpdates();
        }
      }
      this->sys->set_alarm(this, 1 * 1000, data);
      break;
    default:
      std::cout << "handle_alarm(): Unknown alarm type." << std::endl;
      break;
  }
}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
  // add your own code
  Packet deserializedPacket = deserializePacket(packet);
  switch (deserializedPacket.header.packetType) {
    case PING:
      // Handle PING packet
      this->handlePings(port, deserializedPacket);
      break;
    case PONG:
      // Handle PONG packet
      this->handlePongs(port, deserializedPacket);
      break;
    case DATA:
      // Handle DATA packet
      this->handleData(port, packet);
    case LS:
      // Handle LS packet
      this->myLSRP.HandlePacket(port, packet, size);
      break;
    case DV:
      // Handle DV packet
      this->myDV.handleDVPacket(port, deserializedPacket);
    default:
      // Handle unknown packet type
      break;
  }
}

// add more of your own code

// HELPER FUNCTIONS
// Function to send Ping-Pong packets
void RoutingProtocolImpl::sendPings() {
  int size = HEADER_SIZE + sizeof(time_stamp); // Size of the packet struct

  for (unsigned int i = 0; i < this->numPorts; i++) {
    Packet packet;
    time_stamp timestamp = htonl(sys->time()); // Set timestamp
    memcpy(packet.payload, &timestamp, sizeof(timestamp)); // Copy timestamp to payload

    PacketHeader *packetHeader = &packet.header;
    packetHeader->packetType = PING; // Set packet type
    packetHeader->size = htons(size); // Set packet size
    packetHeader->sourceID = htons(routerID); // Set source ID

    // Destination packet is unused in the ping packet, so michael commented this out:
    // // defaults to 0 if not set
    // packetHeader->destID = htons(portStatus[i].destRouterID); // Set destination ID

    // Serialize
    void *serializedPacket = serializePacket(packet);

    // Send the packet
    sys->send(i, serializedPacket, size);
  }
}

/*
When the neighbor router receives the PING message, it must update the 
received message’s type to PONG, copy the source ID to the destination ID,
update the source ID to its own, then send the resulting PONG message (with the
original timestamp still in the payload) immediately back to the neighbor.

'port' is the port from which this packet was received.
*/
void RoutingProtocolImpl::handlePings(unsigned short port, Packet pingPacket) {
  pingPacket.header.packetType = PONG;
  pingPacket.header.destID = pingPacket.header.sourceID;
  pingPacket.header.sourceID = this->routerID;

  void *serializedPongPacket = serializePacket(pingPacket);
  sys->send(port, serializedPongPacket, pingPacket.header.size);
}

/*
When the PONG message is received, the timestamp in the message is compared 
to the current time to compute the RTT. 

'port' is the port from which this packet was received.
*/
void RoutingProtocolImpl::handlePongs(unsigned short port, Packet pongPacket) {
  // Calculate RTT
  time_stamp prevTimestamp = ntohl(*(time_stamp*)pongPacket.payload);
  time_stamp currTimestamp = sys->time();
  // assert(prevTimestamp <= currTimestamp);
  time_stamp rtt = currTimestamp - prevTimestamp;
  std::cout << "handlePongs(): RTT = " << rtt << std::endl;

  // Use the PONG packet's source ID to discover the ID of its current neighbor
  router_id destID = pongPacket.header.sourceID;

  // If we didn't previously know port, create port -> PortStatusEntry mapping 
  if (portStatus.find(port) == portStatus.end()) {
    portStatus[port] = PortStatusEntry();
  }

  // Update that port's information
  portStatus[port].destRouterID = destID;
  bool wasUp = portStatus[port].isUp;
  portStatus[port].isUp = true;
  portStatus[port].lastUpdate = currTimestamp;
  portStatus[port].timeCost = rtt;

  // If we didn't previously have destID as a neighbor, create destID -> neighbor (port, timecost) mapping 
  if (adjacencyList.find(destID) == adjacencyList.end()) {
    adjacencyList[destID] = Neighbor(port, 0);
  }

  adjacencyList[pongPacket.header.sourceID].port = port;
  cost oldTimeCost = adjacencyList[pongPacket.header.sourceID].timeCost;  // Note: if the destID has just been added to adjList, timeCost will be 0
  adjacencyList[pongPacket.header.sourceID].timeCost = rtt;
  std::cout << "handlePongs(): oldTimeCost = " << oldTimeCost << std::endl;
  std::cout << "handlePongs(): newTimeCost = " << adjacencyList[pongPacket.header.sourceID].timeCost << std::endl;

  if (protocolType == P_DV) {
    // for DV, call "handleCostChange()" whether or not timecost changed. This is because it still needs to update "lastUpdated" time stamp for each DV entry associated with this link
    cost changeCost = adjacencyList[pongPacket.header.sourceID].timeCost - oldTimeCost;
    this->myDV.handleCostChange(port, changeCost);
    // send updates to all neighbors
    this->myDV.sendUpdates();
  } else if (protocolType == P_LS) {
    // preserving the behavior 
    if (wasUp) {
      // If this port was previously up, only send updates if time cost changed
      if (adjacencyList[pongPacket.header.sourceID].timeCost != oldTimeCost) {
        // Time cost has changed
        // for link state routing protocol, we need to update since cost changes
        this->myLSRP.UpdateTable();
        // send updates to all neighbors
        this->myLSRP.SendUpdates();
      }
    } else {
      // Update the forwarding table? Should always be the same as
      assert(forwardingTable[port] == pongPacket.header.sourceID);
      forwardingTable[port] = pongPacket.header.sourceID;

      // If port was not previously up, send updates no matter what
      // for link state routing protocol, we need to update since cost changes
      this->myLSRP.UpdateTable();
      // send updates to all neighbors
      this->myLSRP.SendUpdates();
    }
  }
}

void RoutingProtocolImpl::handleData(unsigned short port, void* handleMe) {
  // TODO: Implement this method to handle DATA packets
  //       (this method will be called every time a DATA packet is received)
  Packet dataPacket = deserializePacket(handleMe);
  router_id destId = dataPacket.header.destID;
  if (dataPacket.header.packetType != DATA) {
    std::cout << "handleData(): Packet type is not DATA." << std::endl;
  }

  if (destId == this->routerID) {
    delete[] static_cast<char*>(handleMe);
    return;
  }

  // this is poor design, but DV has its own separate instance of a forwarding table, so we need to use that one.
  if (this->protocolType == P_LS) {
    if (this->forwardingTable.find(destId) != this->forwardingTable.end()) {
      // If the destination is in the forwarding table, send the packet to the next hop
      sys->send(this->forwardingTable[destId], handleMe, dataPacket.header.size);
    }
  } else if (this->protocolType == P_DV) { 
    if (this->myDV.forwardingTable.table.find(destId) != this->myDV.forwardingTable.table.end()) {
      // If the destination is in the forwarding table, send the packet to the next hop
      sys->send(this->myDV.forwardingTable.table[destId].nextHop, handleMe, dataPacket.header.size);
  }
  
  // If it makes it here, the destination is not in forwarding table and the packet is lost
  std::cout << "handleData(): Destination is not in forwarding table." << std::endl;

}
}