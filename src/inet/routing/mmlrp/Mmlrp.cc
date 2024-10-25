#include "inet/routing/mmlrp/Mmlrp.h"

#include "inet/common/IProtocolRegistrationListener.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/networklayer/common/L3Tools.h"



namespace inet {

Define_Module(Mmlrp);

Mmlrp::Mmlrp()
{

}

Mmlrp::~Mmlrp()
{
    stop();

    // Dispose of dynamically allocated the objects
    delete event;
}

void Mmlrp::initialize(int stage)
{
    RoutingProtocolBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        sequencenumber = 0;
        event = new cMessage("event");

        // Getting MMLRP parameters
        routeLifetime = par("routeLifetime").doubleValue();
        beaconInterval = par("beaconInterval");
        broadcastDelay = &par("broadcastDelay");

        // Context Setup
        host = getContainingNode(this);
        ift.reference(this, "interfaceTableModule", true);
        rt.reference(this, "routingTableModule", true);
        networkProtocol.reference(this, "networkProtocolModule", true);
//        energyStorage = check_and_cast<power::IEpEnergyStorage *>(host->getSubmodule("energyStorage"));
    }

    else if (stage == INITSTAGE_ROUTING_PROTOCOLS) {
        registerProtocol(Protocol::manet, gate("ipOut"), gate("ipIn"));
        networkProtocol->registerHook(0, this);
        host->subscribe(packetReceivedFromLowerSignal, this);
        WATCH(neighborTable);
    }
}

void Mmlrp::start()
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
        throw cRuntimeError("MMLRP has found %i 80211 interfaces", num_80211);
    if (par("manetPurgeRoutingTables").boolValue()) {
        Ipv4Route *entry;
        // clean the route entries in routing table related to wLan interface
        for (int i = rt->getNumRoutes() - 1; i >= 0; i--) {
            entry = rt->getRoute(i);
            if (entry->getInterface()->isWireless())
                rt->deleteRoute(entry);
        }
    }

    // Join the multicast IP address group for MANET routing protocols 224.0.0.109
    CHK(interface80211ptr->getProtocolDataForUpdate<Ipv4InterfaceData>())->joinMulticastGroup(Ipv4Address::LL_MANET_ROUTERS);

    // schedules a random periodic event: the beacon message broadcast from MMLRP module
    scheduleAfter(uniform(0.0, par("maxVariance").doubleValue()), event);
}

void Mmlrp::stop()
{
    cancelEvent(event);
}

void Mmlrp::handleMessageWhenUp(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        handleSelfMessage(msg);
    }
    else if (check_and_cast<Packet*>(msg)->getTag<PacketProtocolTag>()->getProtocol() == &Protocol::manet) {

        Packet *pkt = check_and_cast<Packet*>(msg);
        EV_INFO << "----- Packet ID, TreeID ------ " << pkt->getId() << ", " << pkt->getTreeId() << endl;

        auto packet = new Packet("Beacon");

        // When MMLRP module receives MmlrpBeacon from other host
        // it adds/replaces the information in routing table for the one contained in the message
        // but only if it's useful/up-to-date. If not the MMLRP module ignores the message.
        auto addressReq = packet->addTag<L3AddressReq>();
        addressReq->setDestAddress(Ipv4Address(255, 255, 255, 255)); // let's try the limited broadcast 255.255.255.255
        addressReq->setSrcAddress(interface80211ptr->getProtocolData<Ipv4InterfaceData>()->getIPAddress());
        packet->addTag<InterfaceReq>()->setInterfaceId(interface80211ptr->getInterfaceId());
        packet->addTag<PacketProtocolTag>()->setProtocol(&Protocol::manet);
        packet->addTag<DispatchProtocolReq>()->setProtocol(&Protocol::ipv4);

        auto recBeacon = staticPtrCast<MmlrpBeacon>(check_and_cast<Packet*>(msg)->peekData<MmlrpBeacon>()->dupShared());


        if (msg->arrivedOn("ipIn")) {
            ASSERT(recBeacon);

//            getContainingNode(host)->bubble("Received beacon message");
            Ipv4Address source = interface80211ptr->getProtocolData<Ipv4InterfaceData>()->getIPAddress();

            // reads MMLRP beacon message fields
            Ipv4Address src;
            unsigned int msgsequencenumber;
            int numHops;
            Ipv4Address next;

            src = recBeacon->getSrcAddress();
            msgsequencenumber = recBeacon->getSequencenumber();
            next = recBeacon->getNextAddress();
            numHops = recBeacon->getDistanceCost();

            // Output the information about the received packet
            std::ofstream outFile("results/output.csv", std::ios::app);
            if (outFile.is_open()) {
                outFile << simTime()
                        << "," << pkt->getTreeId()
                        << "," << src
                        << "," << next
                        << "," << source
                        << "," << msgsequencenumber
                        << "," << numHops
                        << endl;
                outFile.close();
            } else {
                std::cout << "Error opening file!" << std::endl;
            }
            // ----------------------------------------------- //

            if (src == source) {
                EV_INFO << "beacon msg dropped. The message returned to the original creator.\n";
                delete packet;
                delete msg;
                return;
            }

            Ipv4Route *_input_routing = rt->findBestMatchingRoute(src);
            MmlrpIpv4Route *input_routing = dynamic_cast<MmlrpIpv4Route*>(_input_routing);

            // Tests if the MMLRP beacon message that arrived is useful
            if (_input_routing == nullptr
                            || (_input_routing != nullptr && _input_routing->getNetmask() != Ipv4Address::ALLONES_ADDRESS)
                            || (input_routing != nullptr && (msgsequencenumber > (input_routing->getSequencenumber()) || (msgsequencenumber == (input_routing->getSequencenumber()) && numHops < (input_routing->getMetric())))))
            {
                // remove old entry
                if (input_routing != nullptr)
                    rt->deleteRoute(input_routing);

                // adds new information to routing table according to information in beacon message
                {
                    Ipv4Address netmask = Ipv4Address::ALLONES_ADDRESS;
                    MmlrpIpv4Route *e = new MmlrpIpv4Route();
                    e->setDestination(src);
                    e->setNetmask(netmask);
                    e->setGateway(next);
                    e->setInterface(interface80211ptr);
                    e->setSourceType(IRoute::MANET);
                    e->setMetric(numHops);
                    e->setSequencenumber(msgsequencenumber);
                    e->setExpiryTime(simTime() + routeLifetime);
                    rt->addRoute(e);
                }

                recBeacon->setNextAddress(source);
                numHops++;
                recBeacon->setDistanceCost(numHops);
                double waitTime = intuniform(1, 50);
                waitTime = waitTime / 100;
                EV_DETAIL << "waitime for forward is " << waitTime << " And host is " << source << "\n";
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

void Mmlrp::handleSelfMessage(cMessage *msg)
{
    // When MMLRP module receives selfmessage (scheduled event)
    // it means that it's time for beacon message broadcast event
    // i.e. broadcast beacon messages to other nodes when selfmessage = event
    if (msg == event) {
        // Purge the routing table. this to remove the expired routes
        rt->purge();

        auto beacon = makeShared<MmlrpBeacon>(); // Created new MMLRP beacon
        // Set the packet fields in MmlrpBeacon
        Ipv4Address source = (interface80211ptr->getProtocolData<Ipv4InterfaceData>()->getIPAddress());
        beacon->setChunkLength(b(128)); // size of beacon message in bits
        beacon->setSrcAddress(source);
        sequencenumber += 2;
        beacon->setSequencenumber(sequencenumber);
        beacon->setNextAddress(source);
        beacon->setDistanceCost(1);

        // Created new packet for MmlrpBeacon
        auto packet = new Packet("Beacon", beacon);
        auto addressReq = packet->addTag<L3AddressReq>();
        addressReq->setDestAddress(Ipv4Address(255, 255, 255, 255)); // This to broadcast the packet to all neighbor
        addressReq->setSrcAddress(source);                           // Set the source address in the packet
        packet->addTag<InterfaceReq>()->setInterfaceId(interface80211ptr->getInterfaceId());
        packet->addTag<PacketProtocolTag>()->setProtocol(&Protocol::manet);
        packet->addTag<DispatchProtocolReq>()->setProtocol(&Protocol::ipv4);

        // broadcast the beacon message to other nodes
        send(packet, "ipOut");
//        getContainingNode(host)->bubble("Sending new beacon message");

        packet = nullptr;
        beacon = nullptr;

        // schedule new broadcast beacon message event
        scheduleAfter(beaconInterval + broadcastDelay->doubleValue(), event);
    }
}

//
// netfilter
//

INetfilter::IHook::Result Mmlrp::datagramPreRoutingHook(Packet *datagram)
{
    Enter_Method("datagramPreRoutingHook");

    EV_INFO << "-------- Packet received with packet ID, TreeID --" << datagram->getId() << ", " << datagram->getTreeId() << endl;
//    EV_INFO << "RX power= " << datagram->getTag<SignalPowerInd>()->getPower() << "W" << endl;
//    EV_INFO << "Minimum SNIR Signal-to-Noise-and-Interference Ratio = " << datagram->getTag<SnirInd>()->getMinimumSnir() << endl;
//    EV_INFO << (datagram->getTag<PacketProtocolTag>()->getProtocol() == &Protocol::ipv4) << endl;

    const auto& networkHeader = getNetworkProtocolHeader(datagram);
    if ((networkHeader->getProtocol() == &Protocol::icmpv4)) {

        // Output the information about the received packet
        std::ofstream outFile("results/output2.csv", std::ios::app);
        if (outFile.is_open()) {
            outFile << simTime()
                    << "," << datagram->getTreeId()
                    << "," << networkHeader->getSourceAddress()
                    << "," << networkHeader->getDestinationAddress();
//                    << "," << energyStorage->getResidualEnergyCapacity();

            if (datagram->findTag<MacAddressInd>() != nullptr) {
                outFile << "," << datagram->getTag<MacAddressInd>()->getSrcAddress()
                        << "," << datagram->getTag<MacAddressInd>()->getDestAddress();

            }

            if (datagram->findTag<SignalPowerInd>() != nullptr) {
                outFile << "," << datagram->getTag<SignalPowerInd>()->getPower()
                        << "," << datagram->getTag<SnirInd>()->getMinimumSnir();
            }

            outFile << endl;
            outFile.close();
        } else {
            std::cout << "Error opening file!" << std::endl;
        }
        // ----------------------------------------------- //
    }

    return ACCEPT;
}

//
// notification
//

void Mmlrp::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{
    Enter_Method("%s", cComponent::getSignalName(signalID));

//    Packet *datagram = check_and_cast<Packet *>(obj);

    EV_INFO << "------- Packet Received ------------" << endl;

}


} // namespace inet
