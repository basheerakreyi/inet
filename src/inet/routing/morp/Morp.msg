
// Author: Basheer Al-Qassab

import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;
import inet.networklayer.contract.ipv4.Ipv4Address;
import inet.common.geometry.Geometry;

namespace inet;

class MorpBeacon extends FieldsChunk
{
    Ipv4Address srcAddress;        // Node that generate the beacon
    Ipv4Address nextAddress;       // Node that transmits or re-transmits the packet    
    unsigned int sequenceNumber;   // Sequence number created by the source node of the beacon msg
    float cost;                    // Cost to reach the source node
    
    Coord nextPosition;            // Position of the next node
    int nodeDegree;                // Number of neighbors  
}