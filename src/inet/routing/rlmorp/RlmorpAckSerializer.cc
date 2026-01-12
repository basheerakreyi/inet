//
// Author: Basheer Al-Qassab
//

#include "inet/routing/rlmorp/RlmorpAckSerializer.h"

#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
#include "inet/routing/rlmorp/Rlmorp_m.h"

namespace inet {

Register_Serializer(RlmorpAck, RlmorpAckSerializer);

void RlmorpAckSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    const auto& ack = staticPtrCast<const RlmorpAck>(chunk);
    // Cast signed int to unsigned for serialization
    stream.writeUint32Be(static_cast<uint32_t>(ack->getTreeId()));
    stream.writeIpv4Address(ack->getOriginalSource());
    stream.writeIpv4Address(ack->getOriginalDestination());
    stream.writeUint64Be(SimTime(ack->getDeliveryTime()).raw());
}

const Ptr<Chunk> RlmorpAckSerializer::deserialize(MemoryInputStream& stream) const
{
    auto ack = makeShared<RlmorpAck>();
    // Read as unsigned and cast back to signed int
    ack->setTreeId(static_cast<int>(stream.readUint32Be()));
    ack->setOriginalSource(stream.readIpv4Address());
    ack->setOriginalDestination(stream.readIpv4Address());
    int64_t timeRaw = stream.readUint64Be();
    ack->setDeliveryTime(SimTime(timeRaw).dbl());
    // Calculate chunk length: int32 (4) + Ipv4Address (4) + Ipv4Address (4) + simtime_t (8) = 20 bytes
    ack->setChunkLength(B(20));
    return ack;
}

} // namespace inet

