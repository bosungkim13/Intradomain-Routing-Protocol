#include "RoutingProtocolImpl.h"
#include <global.h>
#include <sys/time.h>
#include <sharedUtils.h>

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

}

void RoutingProtocolImpl::handle_alarm(void *data) {
  // add your own code
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
      break;
    default:
      // Handle unknown packet type
      break;
  }
}

// add more of your own code

// HELPER FUNCTIONS
// Function to send Ping-Pong packets
void RoutingProtocolImpl::sendPings() {
  int size = sizeof(Packet); // Size of the packet struct

  for (unsigned int i = 0; i < this->numPorts; i++) {
    Packet packet;
    time_stamp timestamp = htonl(sys->time()); // Set timestamp
    memcpy(packet.payload, &timestamp, sizeof(timestamp)); // Copy timestamp to payload

    PacketHeader *packetHeader = &packet.header;
    packetHeader->packetType = htons(PING); // Set packet type
    packetHeader->size = htons(size); // Set packet size
    packetHeader->sourceID = htons(routerID); // Set source ID
    // defaults to 0 if not set
    packetHeader->destID = htons(portStatus[i].destRouterID); // Set destination ID

    // Serialize
    void *serializedPacket = serializePacket(packet);

    // Send the packet
    sys->send(i, serializedPacket, size);
  }
}

void RoutingProtocolImpl::handlePings(unsigned short port, Packet pingPacket) {
  pingPacket.header.packetType = PONG;
  time_stamp prevTimestamp = ntohl(*(time_stamp*)pingPacket.payload);
  time_stamp currTimestamp = sys->time();

  assert(prevTimestamp <= currTimestamp);

  memcpy(pingPacket.payload, &currTimestamp, sizeof(currTimestamp));
  void *serializedPongPacket = serializePacket(pingPacket);
  sys->send(port, serializedPongPacket, sizeof(Packet));
}