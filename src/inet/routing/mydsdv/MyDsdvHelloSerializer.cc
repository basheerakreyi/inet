//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/routing/mydsdv/MyDsdvHelloSerializer.h"

#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
#include "inet/routing/mydsdv/MyDsdvHello_m.h"

namespace inet {

Register_Serializer(MyDsdvHello, MyDsdvHelloSerializer);

void MyDsdvHelloSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    const auto& myDsdvHello = staticPtrCast<const MyDsdvHello>(chunk);
    stream.writeIpv4Address(myDsdvHello->getSrcAddress());
    stream.writeUint32Be(myDsdvHello->getSequencenumber());
    stream.writeIpv4Address(myDsdvHello->getNextAddress());
    stream.writeUint32Be(myDsdvHello->getHopdistance());
}

const Ptr<Chunk> MyDsdvHelloSerializer::deserialize(MemoryInputStream& stream) const
{
    auto myDsdvHello = makeShared<MyDsdvHello>();
    myDsdvHello->setSrcAddress(stream.readIpv4Address());
    myDsdvHello->setSequencenumber(stream.readUint32Be());
    myDsdvHello->setNextAddress(stream.readIpv4Address());
    myDsdvHello->setHopdistance(stream.readUint32Be());
    myDsdvHello->setChunkLength(B(16));
    return myDsdvHello;
}

} // namespace inet

