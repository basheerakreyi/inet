
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
    delete purgeTimer;
}

void Morp::initialize(int stage)
{
    RoutingProtocolBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        sequenceNumber = 0;
        event = new cMessage("event");
        purgeTimer = new cMessage("purge");

        // Getting MORP parameters
        routeLifetime = par("routeLifetime").doubleValue();
        neighborLifetime = par("neighborLifetime").doubleValue();
        beaconInterval = par("beaconInterval");
        broadcastDelay = &par("broadcastDelay");
        alpha = par("alpha").doubleValue();
        beta = par("beta").doubleValue();
        gamma = par("gamma").doubleValue();

        // Context Setup
        host = getContainingNode(this);
        ift.reference(this, "interfaceTableModule", true);
        rt.reference(this, "routingTableModule", true);
        mobility = check_and_cast<IMobility *>(host->getSubmodule("mobility"));
        energyStorage = check_and_cast<power::IEpEnergyStorage *>(host->getSubmodule("energyStorage"));
        nodeStatus = check_and_cast<NodeStatus *>(host->getSubmodule("status"));
    }

    else if (stage == INITSTAGE_ROUTING_PROTOCOLS) {
        registerProtocol(Protocol::manet, gate("ipOut"), gate("ipIn"));
        host->subscribe(NodeStatus::nodeStatusChangedSignal, this);
        WATCH(neighborTable);
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
    cancelEvent(purgeTimer);
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
            float cost;
            double hopCost;
            double energyCost;
            double bandwidthCost;

            src = recBeacon->getSrcAddress();
            next = recBeacon->getNextAddress();
            msgSequenceNumber = recBeacon->getSequenceNumber();

            // get the cost in beacon and calculate the new cost based on the information in received beacon
            // if (recBeacon->getCost() == 0)
            //    cost = (1 / recBeacon->getResidualEnergy());
            //else
            //    cost = 1 / ((1 / recBeacon->getCost()) + recBeacon->getResidualEnergy());
            //cost = recBeacon->getCost() + (1 / recBeacon->getResidualEnergy());   // Energy only cost
            //cost = recBeacon->getCost() + 1;   // Hop only cost
            //cost = recBeacon->getCost() + 56000000/recBeacon->getDataRate();   // Data rate only cost
            //cost = recBeacon->getCost() + (alpha + (1-alpha)*(1 / recBeacon->getResidualEnergy()));

            hopCost = recBeacon->getCost() + 1;

            //energyCost = recBeacon->getCost() + 1/recBeacon->getResidualEnergy();
            //energyCost = recBeacon->getCost() + energyStorage->getNominalEnergyCapacity().get()/recBeacon->getResidualEnergy(); // Normalized
            energyCost = recBeacon->getCost() + (1 - recBeacon->getResidualEnergy()/energyStorage->getNominalEnergyCapacity().get());

            bandwidthCost = recBeacon->getCost() + 56000000/recBeacon->getDataRate();
            cost = alpha*hopCost + beta*energyCost + gamma*bandwidthCost;


            Ipv4Address source = interface80211ptr->getProtocolData<Ipv4InterfaceData>()->getIPAddress();

            // add neighbor information into the neighbor table
            int interfaceID = check_and_cast<Packet*>(msg)->getTag<InterfaceInd>()->getInterfaceId();
            neighborTable.updateNeighbor(next, interfaceID, recBeacon->getNextPosition(), recBeacon->getNodeDegree(), recBeacon->getResidualEnergy());
            neighborTable.removeOldNeighbors(simTime() - neighborLifetime); // To remove the old neighbor that lost the connection

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
                            || (input_routing != nullptr && (msgSequenceNumber > input_routing->getSequenceNumber() || (msgSequenceNumber == input_routing->getSequenceNumber() && cost < input_routing->getRouteCost()))))
            {
                // remove old entry
                if (input_routing != nullptr){
                    rt->deleteRoute(input_routing);
                //    std::cout << "host " << host->getFullName() << " deleted a route at " << simTime() << endl;
                }


                // adds new information to routing table according to information in beacon message
                {
                    Ipv4Address netmask = Ipv4Address::ALLONES_ADDRESS;
                    MorpRouteData *e = new MorpRouteData();
                    e->setDestination(src);
                    e->setNetmask(netmask);
                    e->setGateway(next);
                    e->setInterface(interface80211ptr);
                    e->setSourceType(IRoute::MANET);
                    //e->setMetric(numHops);
                    e->setRouteCost(cost);
                    e->setSequenceNumber(msgSequenceNumber);
                    e->setExpirTime(simTime() + routeLifetime);
                    rt->addRoute(e);
                    reschedulePurgeTimer();
                }

                // Modify the content of the received beacon and send it to other neighbors
                recBeacon->setCost(cost);
                recBeacon->setNextAddress(source);
                recBeacon->setNextPosition(mobility->getCurrentPosition());
                recBeacon->setNodeDegree(neighborTable.getAddresses().size());
                recBeacon->setResidualEnergy(energyStorage->getResidualEnergyCapacity().get());
                recBeacon->setDataRate(interface80211ptr->getDatarate());
                packet->insertAtBack(recBeacon);
                send(packet, "ipOut");
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
        // rt->purge();
        purge();

        auto beacon = makeShared<MorpBeacon>(); // Created new MORP beacon

        // Set the packet fields in MorpBeacon
        Ipv4Address source = (interface80211ptr->getProtocolData<Ipv4InterfaceData>()->getIPAddress());
        beacon->setChunkLength(b(128)); // size of beacon message in bits
        beacon->setSrcAddress(source);
        sequenceNumber += 2;
        beacon->setSequenceNumber(sequenceNumber);
        beacon->setNextAddress(source);
        beacon->setCost(0);
        beacon->setNextPosition(mobility->getCurrentPosition());
        beacon->setNodeDegree(neighborTable.getAddresses().size());
        beacon->setResidualEnergy(energyStorage->getResidualEnergyCapacity().get());
        beacon->setDataRate(interface80211ptr->getDatarate());

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
    else if (msg == purgeTimer) {
        purge();
        reschedulePurgeTimer();
    }
}

void Morp::reschedulePurgeTimer()
{
    simtime_t purgeTime = SimTime::getMaxTime();
    for (int i = 0; i < rt->getNumRoutes(); i++) {
        auto route = dynamic_cast<MorpRouteData *>(rt->getRoute(i));
        if (route && !route->isExpired() && route->getExpirTime() < purgeTime)
            purgeTime = route->getExpirTime();
    }
    cancelEvent(purgeTimer);
    if (purgeTime != SimTime::getMaxTime())
        scheduleAt(purgeTime, purgeTimer);
}

void Morp::purge()
{
    for (int i = 0; i < rt->getNumRoutes();) {
        auto route = dynamic_cast<MorpRouteData *>(rt->getRoute(i));
        if (route && route->isExpired())
            rt->deleteRoute(route);
        else
            i++;
    }
}

//
// notification
//

void Morp::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) {
    Enter_Method("%s", cComponent::getSignalName(signalID));

    if (nodeStatus->getState() == NodeStatus::DOWN) {
        std::cout << simTime() << endl; // << "The node is down at "
    }
}

} /* namespace inet */
