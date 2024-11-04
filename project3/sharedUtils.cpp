#include "sharedUtils.h"

void* serializePacket(Packet serializeMe) {
    // Allocate memory for the serialized packet
    void* buffer = malloc(sizeof(Packet));
    if (buffer == nullptr) {
        return nullptr; // Check for allocation failure
    }
    // Copy the packet header with endian conversion
    PacketHeader header = serializeMe.header;
    header.size = htons(header.size);
    header.sourceID = htons(header.sourceID);
    header.destID = htons(header.destID);
    memcpy(buffer, &header, sizeof(PacketHeader));

    // Copy the payload
    memcpy((char*)buffer + sizeof(PacketHeader), serializeMe.payload, sizeof(serializeMe.payload));
    
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
    
    // Copy the payload back
    memcpy(packet.payload, (char*)deserializeMe + sizeof(PacketHeader), sizeof(packet.payload));
    
    return packet; // Return the deserialized packet
}