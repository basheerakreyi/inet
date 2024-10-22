
#include "inet/routing/mmlrp/MmlrpSerializer.h"

#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
#include "inet/routing/mmlrp/Mmlrp_m.h"

namespace inet {

Register_Serializer(MmlrpBeacon, MmlrpBeaconSerializer);

void MmlrpBeaconSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    const auto& mmlrpBeacon = staticPtrCast<const MmlrpBeacon>(chunk);
    stream.writeIpv4Address(mmlrpBeacon->getSrcAddress());
    stream.writeUint32Be(mmlrpBeacon->getSequencenumber());
    stream.writeIpv4Address(mmlrpBeacon->getNextAddress());
    stream.writeUint32Be(mmlrpBeacon->getDistanceCost());
}

const Ptr<Chunk> MmlrpBeaconSerializer::deserialize(MemoryInputStream& stream) const
{
    auto mmlrpBeacon = makeShared<MmlrpBeacon>();
    mmlrpBeacon->setSrcAddress(stream.readIpv4Address());
    mmlrpBeacon->setSequencenumber(stream.readUint32Be());
    mmlrpBeacon->setNextAddress(stream.readIpv4Address());
    mmlrpBeacon->setDistanceCost(stream.readUint32Be());
    mmlrpBeacon->setChunkLength(B(16));
    return mmlrpBeacon;
}

} // namespace inet

