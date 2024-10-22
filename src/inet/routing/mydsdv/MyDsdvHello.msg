
import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;
import inet.networklayer.contract.ipv4.Ipv4Address;

namespace inet;

class MyDsdvHello extends FieldsChunk
{
    Ipv4Address srcAddress; //terminal que gerou o pacote
    unsigned int sequencenumber;//sequence number created by the source node(node's ID = srcIPAddress) of the hello msg and associated with that node
    Ipv4Address nextAddress; //terminal que re-/transmite o pacote
    int hopdistance; //distancia em hops/custo
}