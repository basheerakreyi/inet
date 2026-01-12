//
// Author: Basheer Al-Qassab
//

#ifndef __INET_MLMORPACKSERIALIZER_H
#define __INET_MLMORPACKSERIALIZER_H

#include "inet/common/packet/serializer/FieldsChunkSerializer.h"

namespace inet {

/**
 * Converts between MlmorpAck and binary (network byte order) MLMORP ACK packet.
 */
class INET_API MlmorpAckSerializer : public FieldsChunkSerializer
{
  protected:
    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

  public:
    MlmorpAckSerializer() : FieldsChunkSerializer() {}
};

} // namespace inet

#endif

