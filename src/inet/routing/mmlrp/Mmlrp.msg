
import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;
import inet.networklayer.contract.ipv4.Ipv4Address;

namespace inet;

class MmlrpBeacon extends FieldsChunk
{
    Ipv4Address srcAddress;        //Node that generate the Beacon
    unsigned int sequencenumber;   //sequence number created by the source node(node's ID = srcIPAddress) of the beacon msg and associated with that node
    Ipv4Address nextAddress;       //terminal that transmits or re-transmits the packet
    int distanceCost;              //distance in hops/cost
}