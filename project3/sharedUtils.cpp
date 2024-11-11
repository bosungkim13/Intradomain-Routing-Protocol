#include "sharedUtils.h"
#include <cassert>

void* serializePacket(Packet serializeMe) {
    // Copy the packet header with endian conversion
    PacketHeader header = serializeMe.header;
    unsigned short mySize = header.size;
    header.size = htons(header.size);
    header.sourceID = htons(header.sourceID);
    header.destID = htons(header.destID);

    // Allocate memory for the serialized packet
    void* buffer = malloc(mySize);
    if (buffer == nullptr) {
        return nullptr; // Check for allocation failure
    }

    memcpy(buffer, &header, HEADER_SIZE);

    // Copy the payload. Could be less than the maximum payload size.
    int offset = HEADER_SIZE;
    if (header.packetType == LS) {
        // For LS packet, first 4 bytes are seqNum
        unsigned int seqNum = serializeMe.payload[offset - HEADER_SIZE];
        seqNum = htonl(seqNum);
        memcpy((char*)buffer + offset, &seqNum, sizeof(seq_num));
        // For the rest of the payload, it alternates between neighbor ID and cost
        offset += sizeof(seq_num);
        while (offset < header.size) {
            unsigned short neighborID = serializeMe.payload[offset - HEADER_SIZE];
            neighborID = htons(neighborID);
            memcpy((char*)buffer + offset, &neighborID, sizeof(router_id));
            offset += sizeof(router_id);
            unsigned short costValue = serializeMe.payload[offset];
            costValue = htons(costValue);
            memcpy((char*)buffer + offset, &costValue, sizeof(cost));
            offset += sizeof(cost);
        }
    } else if (header.packetType == DV) {
        unsigned int numEntries = (header.size - HEADER_SIZE) / (2 * sizeof(router_id) + sizeof(cost));

        assert((header.size - HEADER_SIZE) % (2 * sizeof(router_id) + sizeof(cost)) == 0); // TEMPORARY CODE, DELETE LATER

        // For the rest of the payload, it alternates between destination ID, nextHop ID, and routeCost
        for (unsigned int i = 0; i < numEntries; i++) {
            unsigned short destID = htons(*reinterpret_cast<unsigned short*>(serializeMe.payload[offset - HEADER_SIZE]));
            memcpy((char*)buffer + offset, &destID, sizeof(router_id));
            offset += sizeof(router_id);

            unsigned short nextHopID = htons(*reinterpret_cast<unsigned short*>(serializeMe.payload[offset - HEADER_SIZE]));
            memcpy((char*)buffer + offset, &nextHopID, sizeof(router_id));
            offset += sizeof(router_id);

            unsigned short routeCost = htons(*reinterpret_cast<unsigned short*>(serializeMe.payload[offset - HEADER_SIZE]));
            memcpy((char*)buffer + offset, &routeCost, sizeof(cost));
            offset += sizeof(cost);
        }
    } else if (header.packetType == PONG || header.packetType == PING) {
        // For PING and PONG packets, the payload is the timestamp
        time_stamp timestamp = serializeMe.payload[offset - HEADER_SIZE];
        timestamp = htonl(timestamp);
        memcpy((char*)buffer + offset, &timestamp, sizeof(timestamp));
    } else {
        std::cout << "serializePacket(): Unknown packet type. Not processing payload." << std::endl;
    }
    
    return buffer; // Return the serialized packet
}

Packet deserializePacket(void* deserializeMe) {
    Packet packet;
    
    // Copy the header back
    memcpy(&packet.header, deserializeMe, sizeof(PacketHeader));
    
    // Convert header fields from network to host byte order
    packet.header.size = ntohs(packet.header.size);
    packet.header.sourceID = ntohs(packet.header.sourceID);
    packet.header.destID = ntohs(packet.header.destID);
    
    // Copy the payload back. Could be less than the maximum payload size.
    memcpy(packet.payload, (char*)deserializeMe + sizeof(PacketHeader), packet.header.size - sizeof(PacketHeader));
    
    free(deserializeMe); // Note to Bosung: I (michael) added this line, make sure it doesn't break anything in your implementation of LS

    return packet; // Return the deserialized packet
}