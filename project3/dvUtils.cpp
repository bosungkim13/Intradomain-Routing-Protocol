#include "dvUtils.h"

DVPacketPayload deserializeDVPayload(char* deserializeMe) {
    DVPacketPayload payload(0);
    int offset = 0;
    memcpy(&payload.sequenceNumber, deserializeMe, sizeof(seq_num));
    offset += sizeof(seq_num);
    while (offset < MAX_PAYLOAD_SIZE) {
        router_id neighborID;
        time_stamp cost;
        memcpy(&neighborID, (char*)deserializeMe + offset, sizeof(router_id));
        offset += sizeof(router_id);
        memcpy(&cost, (char*)deserializeMe + offset, sizeof(time_stamp));
        offset += sizeof(time_stamp);
        payload.addNeighbor(neighborID, cost);
    }
    return payload;
}

char* serializeDVPayload(DVPacketPayload serializeMe) {
    char* serializedPacket = new char[MAX_PAYLOAD_SIZE];
    int offset = 0;
    memcpy(serializedPacket, &serializeMe.sequenceNumber, sizeof(seq_num));
    offset += sizeof(seq_num);
    for (auto neighbor: serializeMe.neighbors) {
        memcpy(serializedPacket + offset, &neighbor.first, sizeof(router_id));
        offset += sizeof(router_id);
        memcpy(serializedPacket + offset, &neighbor.second, sizeof(time_stamp));
        offset += sizeof(time_stamp);
    }
    return serializedPacket;
}