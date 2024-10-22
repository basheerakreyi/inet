
#ifndef __INET_MMLRPBEACONSERIALIZER_H
#define __INET_MMLRPBEACONSERIALIZER_H

#include "inet/common/packet/serializer/FieldsChunkSerializer.h"

namespace inet {

/**
 * Converts between DsdvHello and binary (network byte order) dsdv hello packet.
 */
class INET_API MmlrpBeaconSerializer : public FieldsChunkSerializer
{
  protected:
    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

  public:
    MmlrpBeaconSerializer() : FieldsChunkSerializer() {}
};

} // namespace inet

#endif

