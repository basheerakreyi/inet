// Author: Basheer Al-Qassab

#include "inet/routing/mlmorp/Mlmorp.h"

#include "inet/common/IProtocolRegistrationListener.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/networklayer/common/NextHopAddressTag_m.h"
#include "inet/networklayer/common/L3Tools.h"
#include "inet/queueing/contract/IPacketQueue.h"

namespace inet {

Define_Module(Mlmorp);

Mlmorp::Mlmorp()
{

}

Mlmorp::~Mlmorp()
{
    stop();

    // Dispose of dynamically allocated the objects
    delete event;
    delete purgeTimer;
    delete dnnModel;
}

void Mlmorp::initialize(int stage)
{
    RoutingProtocolBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        sequenceNumber = 0;
        event = new cMessage("event");
        purgeTimer = new cMessage("purge");

        // Getting MLMORP parameters
        routeLifetime = par("routeLifetime").doubleValue();
        neighborLifetime = par("neighborLifetime").doubleValue();
        beaconInterval = par("beaconInterval");
        broadcastDelay = &par("broadcastDelay");
        alpha = par("alpha").doubleValue();
        beta = par("beta").doubleValue();
        gamma = par("gamma").doubleValue();

        // Initialize DNN Model
        int inputSize = par("dnnInputSize").intValue();
        int hiddenSize = par("dnnHiddenSize").intValue();
        bool isClassification = par("dnnClassification").boolValue();
        dnnModel = new SimpleDNNModel(inputSize, hiddenSize, isClassification);
        
        // Load pre-trained model if specified
        std::string modelFile = par("dnnModelFile").stringValue();
        if (!modelFile.empty()) {
            if (dnnModel->loadModel(modelFile)) {
                EV_INFO << "DNN model loaded successfully from " << modelFile << endl;
            } else {
                EV_WARN << "Failed to load DNN model from " << modelFile << ", using random initialization" << endl;
            }
        } else {
            EV_INFO << "DNN model initialized with random weights" << endl;
        }
        
        EV_INFO << "DNN Model Info: " << dnnModel->getModelInfo() << endl;

        // Context Setup
        host = getContainingNode(this);
        ift.reference(this, "interfaceTableModule", true);
        rt.reference(this, "routingTableModule", true);
        networkProtocol.reference(this, "networkProtocolModule", true); // added to make NetFilter work
        mobility = check_and_cast<IMobility *>(host->getSubmodule("mobility"));
        energyStorage = check_and_cast<power::IEpEnergyStorage *>(host->getSubmodule("energyStorage"));
        nodeStatus = check_and_cast<NodeStatus *>(host->getSubmodule("status"));
    }

    else if (stage == INITSTAGE_ROUTING_PROTOCOLS) {
        registerProtocol(Protocol::manet, gate("ipOut"), gate("ipIn"));
        networkProtocol->registerHook(0, this); // added to make NetFilter work
        host->subscribe(NodeStatus::nodeStatusChangedSignal, this);
        WATCH(neighborTable);
    }

    // Delete previously created output file if exist
    std::string filename = "results/output.csv";
    std::remove(filename.c_str());

}

void Mlmorp::start()
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
        throw cRuntimeError("MLMORP has found %i 80211 interfaces", num_80211);

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

void Mlmorp::stop()
{
    cancelEvent(event);
    cancelEvent(purgeTimer);
}

void Mlmorp::handleMessageWhenUp(cMessage *msg)
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

        // When MLMORP module receives MlmorpBeacon from other host
        // it adds/replaces the information in routing table for the one contained in the message
        // but only if it's useful/up-to-date. If not the MLMORP module ignores the message.
        auto recBeacon = staticPtrCast<MlmorpBeacon>(check_and_cast<Packet*>(msg)->peekData<MlmorpBeacon>()->dupShared());
        if (msg->arrivedOn("ipIn")) {
            ASSERT(recBeacon);

            // reads MLMORP beacon message fields
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

            // Extract actual signal power from packet
            double signalPower = -1;  // Default value
            if (check_and_cast<Packet*>(msg)->findTag<SignalPowerInd>()!= nullptr) {
                signalPower = check_and_cast<Packet*>(msg)->getTag<SignalPowerInd>()->getPower().get();
            }

            double buffPktNo = getCurrentBufferPacketNum();  // Default value

            // Update neighbor table for each received beacon
            int interfaceID = check_and_cast<Packet*>(msg)->getTag<InterfaceInd>()->getInterfaceId();
            neighborTable.updateNeighbor(next, interfaceID, recBeacon->getNextPosition(), recBeacon->getNodeDegree(),
                                         recBeacon->getResidualEnergy(), signalPower,
                                         buffPktNo);
            neighborTable.removeOldNeighbors(simTime() - neighborLifetime); // To remove the old neighbor that lost the connection

            // Check if DNN-based routing is enabled
            bool useDNN = par("useDNNRouting").boolValue();
            if (useDNN) {

                // Use DNN model to calculate routing cost (Can be implemented)

                // Clean up and exit
                delete packet;
                delete msg;
            } else {
                // Use traditional cost calculation
                hopCost = recBeacon->getCost() + 1;
                energyCost = recBeacon->getCost() + (1 - recBeacon->getResidualEnergy()/energyStorage->getNominalEnergyCapacity().get());
                bandwidthCost = recBeacon->getCost() + 56000000/recBeacon->getDataRate();
                cost = alpha*hopCost + beta*energyCost + gamma*bandwidthCost;

                Ipv4Address source = interface80211ptr->getProtocolData<Ipv4InterfaceData>()->getIPAddress();

                if (src == source) {
                    EV_INFO << "Beacon message is dropped because the message is returned to the original node.\n";
                    delete packet;
                    delete msg;
                    return;
                }

                Ipv4Route *_input_routing = rt->findBestMatchingRoute(src);
                MlmorpRouteData *input_routing = dynamic_cast<MlmorpRouteData*>(_input_routing);

                // Tests if the MLMORP beacon message that arrived is useful
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
                        MlmorpRouteData *e = new MlmorpRouteData();
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

                    recBeacon->setSignalPower(signalPower);  // Default signal power in dBm
                    recBeacon->setBuffPktNo(buffPktNo);      // Default buffPktNo

                    packet->insertAtBack(recBeacon);
                    send(packet, "ipOut");
                    packet = nullptr;
                }

                // Clean up
                delete packet;
                delete msg;
            }
        } else
            throw cRuntimeError("Message arrived on unknown gate %s", msg->getArrivalGate()->getName());
    } else
        throw cRuntimeError("Message not supported %s", msg->getName());
}

void Mlmorp::handleSelfMessage(cMessage *msg)
{
    // When MLMORP module receives self-message (scheduled event)
    // it means that it's time for beacon message broadcast event
    if (msg == event) {

        // Purge the routing table (this to remove the expired routes)
        // rt->purge();
        purge();

        auto beacon = makeShared<MlmorpBeacon>(); // Created new MLMORP beacon

        // Set the packet fields in MlmorpBeacon
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

        beacon->setSignalPower(-1);  // Default signal power in dBm
        beacon->setBuffPktNo(getCurrentBufferPacketNum());    // Default buffPktNo

        // Created new packet for MlmorpBeacon
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

void Mlmorp::reschedulePurgeTimer()
{
    simtime_t purgeTime = SimTime::getMaxTime();
    for (int i = 0; i < rt->getNumRoutes(); i++) {
        auto route = dynamic_cast<MlmorpRouteData *>(rt->getRoute(i));
        if (route && !route->isExpired() && route->getExpirTime() < purgeTime)
            purgeTime = route->getExpirTime();
    }
    cancelEvent(purgeTimer);
    if (purgeTime != SimTime::getMaxTime())
        scheduleAt(purgeTime, purgeTimer);
}

void Mlmorp::purge()
{
    for (int i = 0; i < rt->getNumRoutes();) {
        auto route = dynamic_cast<MlmorpRouteData *>(rt->getRoute(i));
        if (route && route->isExpired())
            rt->deleteRoute(route);
        else
            i++;
    }
}

//
// NetFilter
//

INetfilter::IHook::Result Mlmorp::routeDatagram(Packet *datagram)
{
    // Only perform ML-based routing if enabled
    bool useDNN = par("useDNNRouting").boolValue();
    if (!useDNN) {
        // If ML-based routing is not enabled, fall back to default behavior (could be legacy/routing table)
        return ACCEPT;
    }

    const auto& networkHeader = getNetworkProtocolHeader(datagram);
    const L3Address& source = networkHeader->getSourceAddress();
    const L3Address& destination = networkHeader->getDestinationAddress();
    EV_INFO << "MLMORP: Finding next hop: source = " << source << ", destination = " << destination << endl;
    // Use DNN-based logic to select next hop
    L3Address nextHop = selectBestNeighborDNN(destination);
    datagram->addTagIfAbsent<NextHopAddressReq>()->setNextHopAddress(nextHop);
    if (nextHop.isUnspecified()) {
        EV_WARN << "MLMORP: No next hop found, dropping packet: source = " << source << ", destination = " << destination << endl;
        if (hasGUI())
            getContainingNode(this)->bubble("No next hop found, dropping packet");
        return DROP;
    }
    else {
        EV_INFO << "MLMORP: Next hop found: source = " << source << ", destination = " << destination << ", nextHop: " << nextHop << endl;
        auto networkInterface = interface80211ptr;
        datagram->addTagIfAbsent<InterfaceReq>()->setInterfaceId(networkInterface->getInterfaceId());
        return ACCEPT;
    }
}

INetfilter::IHook::Result Mlmorp::datagramPreRoutingHook(Packet *datagram)
{
    Enter_Method("datagramPreRoutingHook");

    const auto& networkHeader = getNetworkProtocolHeader(datagram);

    // ---- Data Collection --- //
    // EV_INFO << "-------- Packet received with packet ID, TreeID --" << datagram->getId() << ", " << datagram->getTreeId() << endl;
    // EV_INFO << (datagram->getTag<PacketProtocolTag>()->getProtocol() == &Protocol::ipv4) << endl;

    if ((networkHeader->getProtocol() == &Protocol::udp)) {
          
        // Output the information about the received packet
        std::ofstream outFile("results/output.csv", std::ios::app);
        if (outFile.is_open()) {
            outFile << simTime()
                    << "," << datagram->getTreeId()
                    << "," << networkHeader->getSourceAddress()
                    << "," << networkHeader->getDestinationAddress();

            if (datagram->findTag<MacAddressInd>() != nullptr) {
                outFile << "," << datagram->getTag<MacAddressInd>()->getSrcAddress()
                        << "," << datagram->getTag<MacAddressInd>()->getDestAddress();

            }

            outFile << "," << neighborTable.getAddresses().size()
                    << "," << energyStorage->getResidualEnergyCapacity().get()
                    << "," << interface80211ptr->getDatarate();

            if (datagram->findTag<SignalPowerInd>() != nullptr) {
                outFile << "," << datagram->getTag<SignalPowerInd>()->getPower().get()
                        // << "," << datagram->getTag<SnirInd>()->getMinimumSnir();
                        << "," << getCurrentBufferPacketNum();
            }

            // Adding the time delay of the packet to Data Collection
//            auto data = datagram->peekData(); // get all data from the packet
//            auto regions = data->getAllTags<CreationTimeTag>(); // get all tag regions
//            for (auto &region : regions) { // for each region do
//                auto creationTime = region.getTag()->getCreationTime().dbl(); // original time
//                auto delay = simTime() - creationTime; // compute delay
//                outFile << "," << delay;
//            }

            outFile << endl;
            outFile.close();
        } else {
            std::cout << "Error opening file!" << std::endl;
        }

    }
    // ------ End of Data Collection ----- //

    if ((networkHeader->getProtocol() == &Protocol::udp)) {
        const L3Address& destination = networkHeader->getDestinationAddress();

        // Only apply to UDP packets
        if (destination.isMulticast() || destination.isBroadcast() || rt->isLocalAddress(destination))
            return ACCEPT;
        else
            return routeDatagram(datagram);
    }

    // Accept the packet for forwarding
    return ACCEPT;
}

INetfilter::IHook::Result Mlmorp::datagramLocalOutHook(Packet *datagram)
{
    Enter_Method("datagramLocalOutHook");

    // Extract destination address from the network header
    const auto &networkHeader = getNetworkProtocolHeader(datagram);
    if ((networkHeader->getProtocol() == &Protocol::udp)) {
        const L3Address& destination = networkHeader->getDestinationAddress();

        // Only apply to UDP packets
        if (destination.isMulticast() || destination.isBroadcast() || rt->isLocalAddress(destination))
            return ACCEPT;
        else
            return routeDatagram(datagram);
    }

    // Accept the packet for forwarding
    return ACCEPT;
}

//
// notification
//

void Mlmorp::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) {
    Enter_Method("%s", cComponent::getSignalName(signalID));

    if (nodeStatus->getState() == NodeStatus::DOWN) {
        std::cout << simTime() << endl; // << "The node is down at "
    }
}

/**
 * Select the best next-hop neighbor using DNN model predictions
 * @param destination The destination address
 * @return The best neighbor address for routing
 */
L3Address Mlmorp::selectBestNeighborDNN(const L3Address& destination) const
{
    // Get all available neighbors
    std::vector<L3Address> neighbors = neighborTable.getAddresses();
    
    if (neighbors.empty()) {
        return L3Address();
    }
    
    // If only one neighbor, return it
    if (neighbors.size() == 1) {
        return neighbors[0];
    }
    
    // Create feature map for each neighbor
    std::map<L3Address, std::vector<double>> neighborFeatures;
    
    for (const auto& neighbor : neighbors) {
        std::vector<double> features;
        
        // Get neighbor information
        double residualEnergy = neighborTable.getResidualEnergy(neighbor);
        int nodeDegree = neighborTable.getNodeDegree(neighbor);
        
        // Get interface information for data rate
        double dataRate = interface80211ptr->getDatarate();
        
        // Get values for signal power and buffPktNo
        double signalPower = neighborTable.getSignalPower(neighbor);  // Default signal power
        double buffPktNo = neighborTable.getBuffPktNo(neighbor);         // Default buffPktNo
        
        // Create feature vector: [residualEnergy, dataRate, signalPower, nodeDegree, buffPktNo]
        features = {residualEnergy, dataRate, signalPower, static_cast<double>(nodeDegree), buffPktNo};
        
        neighborFeatures[neighbor] = features;
    }
    
    // Use DNN model to select best neighbor
    return dnnModel->selectBestNeighbor(neighbors, neighborFeatures);
}

// Utility function to get the current MAC buffer (pendingQueue) packet count
int Mlmorp::getCurrentBufferPacketNum() const
{
    if (interface80211ptr) {
        cModule *mac = interface80211ptr->getSubmodule("mac");
        if (mac) {
            cModule *dcf = mac->getSubmodule("dcf");
            if (dcf) {
                cModule *channelAccess = dcf->getSubmodule("channelAccess");
                if (channelAccess) {
                    cModule *pendingQueueModule = channelAccess->getSubmodule("pendingQueue");
                    if (pendingQueueModule) {
                        auto pendingQueue = check_and_cast<inet::queueing::IPacketQueue *>(pendingQueueModule);
                        int numPackets = pendingQueue->getNumPackets();
                        // EV_INFO << "MAC buffer (pendingQueue) contains " << numPackets << " packets\n";
                        return numPackets;
                    }
                }
            }
        }
    }
    return -1;
}

} /* namespace inet */
