//
// Author: Basheer Al-Qassab
//

#ifndef __INET_RLMORPACKSERIALIZER_H
#define __INET_RLMORPACKSERIALIZER_H

#include "inet/common/packet/serializer/FieldsChunkSerializer.h"

namespace inet {

/**
 * Converts between RlmorpAck and binary (network byte order) RLMORP ACK packet.
 */
class INET_API RlmorpAckSerializer : public FieldsChunkSerializer
{
  protected:
    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

  public:
    RlmorpAckSerializer() : FieldsChunkSerializer() {}
};

} // namespace inet

#endif

