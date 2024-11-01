
// Author: Basheer Al-Qassab

#include "inet/routing/morp/Morp.h"

#include "inet/common/IProtocolRegistrationListener.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/networklayer/common/L3Tools.h"

namespace inet {

Define_Module(Morp);

Morp::Morp()
{

}

Morp::~Morp()
{
    stop();

    // Dispose of dynamically allocated the objects
    delete event;
}

void Morp::initialize(int stage)
{
    RoutingProtocolBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        sequenceNumber = 0;
        event = new cMessage("event");

        // Getting MORP parameters
        routeLifetime = par("routeLifetime").doubleValue();
        beaconInterval = par("beaconInterval");
        broadcastDelay = &par("broadcastDelay");

        // Context Setup
        host = getContainingNode(this);
        ift.reference(this, "interfaceTableModule", true);
        rt.reference(this, "routingTableModule", true);
    }

    else if (stage == INITSTAGE_ROUTING_PROTOCOLS) {
        registerProtocol(Protocol::manet, gate("ipOut"), gate("ipIn"));
    }
}

void Morp::start()
{
    // Search for the 80211 interfaces
    int num_80211 = 0;
    NetworkInterface *ie;
    NetworkInterface *i_face;

    for (int i = 0; i < ift->getNumInterfaces(); i++) {
        ie = ift->getInterface(i);
        if (ie->isWireless()) {
            i_face = ie;
            num_80211++;
            interfaceId = i;
        }
    }

    // One 80211 interface must be enabled in total.
    if (num_80211 == 1)
        interface80211ptr = i_face;
    else
        throw cRuntimeError("MORP has found %i 80211 interfaces", num_80211);

    // Purge the routes related to wireless interface
    if (par("manetPurgeRoutingTables").boolValue()) {
        Ipv4Route *entry;
        for (int i = rt->getNumRoutes() - 1; i >= 0; i--) {
            entry = rt->getRoute(i);
            if (entry->getInterface()->isWireless())
                rt->deleteRoute(entry);
        }
    }

    // Schedules the first event with a random time delay between zero and max variance
    scheduleAfter(uniform(0.0, par("maxVariance").doubleValue()), event);
}

void Morp::stop()
{
    cancelEvent(event);
}

void Morp::handleMessageWhenUp(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        handleSelfMessage(msg);
    }
    else if (check_and_cast<Packet*>(msg)->getTag<PacketProtocolTag>()->getProtocol() == &Protocol::manet) {

        // Create new packet that will be used later to carry new information.
        auto packet = new Packet("Beacon");
        auto addressReq = packet->addTag<L3AddressReq>();
        addressReq->setDestAddress(Ipv4Address(255, 255, 255, 255)); // let's try the limited broadcast 255.255.255.255
        addressReq->setSrcAddress(interface80211ptr->getProtocolData<Ipv4InterfaceData>()->getIPAddress());
        packet->addTag<InterfaceReq>()->setInterfaceId(interface80211ptr->getInterfaceId());
        packet->addTag<PacketProtocolTag>()->setProtocol(&Protocol::manet);
        packet->addTag<DispatchProtocolReq>()->setProtocol(&Protocol::ipv4);

        // When MORP module receives MorpBeacon from other host
        // it adds/replaces the information in routing table for the one contained in the message
        // but only if it's useful/up-to-date. If not the MORP module ignores the message.
        auto recBeacon = staticPtrCast<MorpBeacon>(check_and_cast<Packet*>(msg)->peekData<MorpBeacon>()->dupShared());
        if (msg->arrivedOn("ipIn")) {

            ASSERT(recBeacon);

            // reads MORP beacon message fields
            Ipv4Address src;
            Ipv4Address next;
            unsigned int msgSequenceNumber;
            float numHops;

            src = recBeacon->getSrcAddress();
            next = recBeacon->getNextAddress();
            msgSequenceNumber = recBeacon->getSequenceNumber();
            numHops = recBeacon->getCost();

            Ipv4Address source = interface80211ptr->getProtocolData<Ipv4InterfaceData>()->getIPAddress();

            if (src == source) {
                EV_INFO << "Beacon message is dropped because the message is returned to the original node.\n";
                delete packet;
                delete msg;
                return;
            }

            Ipv4Route *_input_routing = rt->findBestMatchingRoute(src);
            MorpRouteData *input_routing = dynamic_cast<MorpRouteData*>(_input_routing);

            // Tests if the MORP beacon message that arrived is useful
            if (_input_routing == nullptr
                            || (_input_routing != nullptr && _input_routing->getNetmask() != Ipv4Address::ALLONES_ADDRESS)
                            || (input_routing != nullptr && (msgSequenceNumber > (input_routing->getSequenceNumber()) || (msgSequenceNumber == (input_routing->getSequenceNumber()) && numHops < (input_routing->getMetric())))))
            {
                // remove old entry
                if (input_routing != nullptr)
                    rt->deleteRoute(input_routing);

                // adds new information to routing table according to information in beacon message
                {
                    Ipv4Address netmask = Ipv4Address::ALLONES_ADDRESS;
                    MorpRouteData *e = new MorpRouteData();
                    e->setDestination(src);
                    e->setNetmask(netmask);
                    e->setGateway(next);
                    e->setInterface(interface80211ptr);
                    e->setSourceType(IRoute::MANET);
                    e->setMetric(numHops);
                    e->setSequenceNumber(msgSequenceNumber);
                    e->setExpirTime(simTime() + routeLifetime);
                    rt->addRoute(e);
                }

                recBeacon->setNextAddress(source);
                numHops++;
                recBeacon->setCost(numHops);
                double waitTime = intuniform(1, 50);
                waitTime = waitTime / 100;
                packet->insertAtBack(recBeacon);
                sendDelayed(packet, waitTime, "ipOut");
                packet = nullptr;

            }
            delete packet;
            delete msg;
        } else
            throw cRuntimeError("Message arrived on unknown gate %s", msg->getArrivalGate()->getName());
    } else
        throw cRuntimeError("Message not supported %s", msg->getName());
}

void Morp::handleSelfMessage(cMessage *msg)
{
    // When MORP module receives self-message (scheduled event)
    // it means that it's time for beacon message broadcast event
    if (msg == event) {

        // Purge the routing table (this to remove the expired routes)
        rt->purge();

        auto beacon = makeShared<MorpBeacon>(); // Created new MORP beacon

        // Set the packet fields in MorpBeacon
        Ipv4Address source = (interface80211ptr->getProtocolData<Ipv4InterfaceData>()->getIPAddress());
        beacon->setChunkLength(b(128)); // size of beacon message in bits
        beacon->setSrcAddress(source);
        sequenceNumber += 2;
        beacon->setSequenceNumber(sequenceNumber);
        beacon->setNextAddress(source);
        beacon->setCost(1);

        // Created new packet for MorpBeacon
        auto packet = new Packet("Beacon", beacon);
        auto addressReq = packet->addTag<L3AddressReq>();
        addressReq->setDestAddress(Ipv4Address(255, 255, 255, 255)); // This to broadcast the packet to all neighbor
        addressReq->setSrcAddress(source);                           // Set the source address in the packet
        packet->addTag<InterfaceReq>()->setInterfaceId(interface80211ptr->getInterfaceId());
        packet->addTag<PacketProtocolTag>()->setProtocol(&Protocol::manet);
        packet->addTag<DispatchProtocolReq>()->setProtocol(&Protocol::ipv4);

        // broadcast the beacon message to other nodes
        send(packet, "ipOut");

        packet = nullptr;
        beacon = nullptr;

        // schedule new broadcast beacon message event
        scheduleAfter(beaconInterval + broadcastDelay->doubleValue(), event);
    }
}

} /* namespace inet */
