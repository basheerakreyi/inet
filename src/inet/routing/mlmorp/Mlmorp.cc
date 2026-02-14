// Author: Basheer Al-Qassab

#include "inet/routing/mlmorp/Mlmorp.h"

#include <limits>
#include <algorithm>

#include "inet/common/IProtocolRegistrationListener.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/chunk/Chunk.h"
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
    delete dqnModel;
    delete packetTracker;
    delete rlUpdateTimer;
}

void Mlmorp::initialize(int stage)
{
    RoutingProtocolBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        sequenceNumber = 0;
        event = new cMessage("event");
        purgeTimer = new cMessage("purge");
        rlUpdateTimer = new cMessage("rlUpdate");

        // Getting MLMORP parameters
        routeLifetime = par("routeLifetime").doubleValue();
        neighborLifetime = par("neighborLifetime").doubleValue();
        beaconInterval = par("beaconInterval");
        broadcastDelay = &par("broadcastDelay");
        alpha = par("alpha").doubleValue();
        beta = par("beta").doubleValue();
        gamma = par("gamma").doubleValue();
        delta = par("delta").doubleValue();
        maxQueuePkts = par("maxQueuePkts").doubleValue();

        // Initialize DNN Model
        int inputSize = par("dnnInputSize").intValue();
        int hiddenSize1 = par("dnnHiddenSize1").intValue();
        int hiddenSize2 = par("dnnHiddenSize2").intValue();
        bool isClassification = par("dnnClassification").boolValue();
        dnnModel = new SimpleDNNModel(inputSize, hiddenSize1, hiddenSize2, isClassification);
        
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
        
        // Initialize Reinforcement Learning components
        useOnlineRL = par("useOnlineRL").boolValue();
        rlUpdateInterval = par("rlUpdateInterval").intValue();
        rlPacketCounter = 0;
        
        if (useOnlineRL) {
            // Initialize DQN model (single-output architecture)
            int dqnStateSize = par("dqnStateSize").intValue();  // Should be 5 for neighbor feature vector
            int dqnHiddenSize1 = par("dqnHiddenSize1").intValue();
            int dqnHiddenSize2 = par("dqnHiddenSize2").intValue();
            double dqnLearningRate = par("dqnLearningRate").doubleValue();
            double dqnEpsilon = par("dqnEpsilon").doubleValue();
            double dqnGamma = par("dqnGamma").doubleValue();
            
            dqnModel = new DQNModel(dqnStateSize, dqnHiddenSize1, dqnHiddenSize2, 
                                   dqnLearningRate, dqnEpsilon, dqnGamma);
            
            // Initialize packet tracker
            simtime_t trackingTimeout = par("packetTrackingTimeout");
            int maxTrackingHistory = par("maxTrackingHistory").intValue();
            packetTracker = new PacketTracker(trackingTimeout, maxTrackingHistory);
            
            // Set reward parameters
            double successReward = par("successReward").doubleValue();
            double failureReward = par("failureReward").doubleValue();
            double energyWeight = par("energyWeight").doubleValue();
            double delayWeight = par("delayWeight").doubleValue();
            packetTracker->setRewardParameters(successReward, failureReward, energyWeight, delayWeight);
            
            // Load pre-trained DQN model if specified
            std::string dqnModelFile = par("dqnModelFile").stringValue();
            if (!dqnModelFile.empty()) {
                if (dqnModel->loadModel(dqnModelFile)) {
                    EV_INFO << "DQN model loaded successfully from " << dqnModelFile << endl;
                } else {
                    EV_WARN << "Failed to load DQN model from " << dqnModelFile << ", using random initialization" << endl;
                }
            } else {
                EV_INFO << "DQN model initialized with random weights" << endl;
            }
            
            EV_INFO << "DQN Model Info: " << dqnModel->getModelInfo() << endl;
        } else {
            dqnModel = nullptr;
            packetTracker = nullptr;
        }

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
    
    // Start RL update timer if online RL is enabled
    if (useOnlineRL && rlUpdateTimer != nullptr) {
        scheduleAfter(1.0, rlUpdateTimer);  // Start first update after 1 second
    }
}

void Mlmorp::stop()
{
    cancelEvent(event);
    cancelEvent(purgeTimer);
    cancelEvent(rlUpdateTimer);
}

void Mlmorp::handleMessageWhenUp(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        handleSelfMessage(msg);
    }
    else if (check_and_cast<Packet*>(msg)->getTag<PacketProtocolTag>()->getProtocol() == &Protocol::manet) {
        
        // Check if this is an ACK packet - use peekData with nullptr flag to avoid conversion errors
        auto receivedPacket = check_and_cast<Packet*>(msg);
        const Ptr<const Chunk> chunk = receivedPacket->peekData();
        auto ackData = dynamicPtrCast<const MlmorpAck>(chunk);
        if (ackData != nullptr && msg->arrivedOn("ipIn")) {
            // Handle ACK packet
            int treeId = ackData->getTreeId();
            Ipv4Address originalSource = ackData->getOriginalSource();
            Ipv4Address originalDestination = ackData->getOriginalDestination();
            
            EV_INFO << "MLMORP: Received ACK for packet " << treeId 
                    << " (source=" << originalSource << ", dest=" << originalDestination << ")" << endl;
            
            // Check if we're the original source
            Ipv4Address currentNode = interface80211ptr->getProtocolData<Ipv4InterfaceData>()->getIPAddress();
            if (originalSource == currentNode) {
                // We're the source - confirm delivery locally and drop ACK
                if (useOnlineRL && packetTracker != nullptr) {
                    double energyNow = energyStorage->getResidualEnergyCapacity().get();
                    if (packetTracker->isTracking(treeId)) {
                        packetTracker->confirmDelivery(treeId, ackData->getDeliveryTime(), energyNow);
                        EV_INFO << "MLMORP: Confirmed delivery of packet " << treeId
                                << " at source via ACK" << endl;
                    } else {
                        EV_DETAIL << "MLMORP: ACK reached source for packet " << treeId
                                  << " but not tracking" << endl;
                    }
                }
                EV_INFO << "MLMORP: ACK reached original source, dropping" << endl;
                delete msg;
                return;
            }
            
            // We're an intermediate node - confirm delivery in our tracker
            if (useOnlineRL && packetTracker != nullptr) {
                double energyNow = energyStorage->getResidualEnergyCapacity().get();
                if (packetTracker->isTracking(treeId)) {
                    packetTracker->confirmDelivery(treeId, ackData->getDeliveryTime(), energyNow);
                    EV_INFO << "MLMORP: Confirmed delivery of packet " << treeId << " via ACK" << endl;
                } else {
                    EV_DETAIL << "MLMORP: ACK received for packet " << treeId 
                              << " but not tracking (may have timed out)" << endl;
                }
            }
            
            // Forward ACK back to original source using DQN/DNN policy (same as data packets)
            // Create a new packet with routing information
            auto forwardPacket = new Packet("MLMORP-ACK");
            auto forwardAckData = ackData->dupShared();
            forwardPacket->insertAtBack(forwardAckData);
            
            auto forwardAddressReq = forwardPacket->addTag<L3AddressReq>();
            forwardAddressReq->setDestAddress(L3Address(originalSource));
            forwardAddressReq->setSrcAddress(L3Address(currentNode));
            forwardPacket->addTag<InterfaceReq>()->setInterfaceId(interface80211ptr->getInterfaceId());
            forwardPacket->addTag<PacketProtocolTag>()->setProtocol(&Protocol::manet);
            forwardPacket->addTag<DispatchProtocolReq>()->setProtocol(&Protocol::ipv4);
            
            // Use DQN/DNN policy to find next hop (same as data packets)
            L3Address nextHop;
            if (useOnlineRL && dqnModel != nullptr) {
                // Use reinforcement learning for routing ACK
                nextHop = selectBestNeighborRL(L3Address(originalSource), L3Address(currentNode), treeId);
            } else {
                // Use DNN-based logic to select next hop for ACK
                nextHop = selectBestNeighborDNN(L3Address(originalSource), L3Address(currentNode));
            }
            
            if (!nextHop.isUnspecified()) {
                forwardPacket->addTagIfAbsent<NextHopAddressReq>()->setNextHopAddress(nextHop);
                send(forwardPacket, "ipOut");
                EV_INFO << "MLMORP: Forwarded ACK for packet " << treeId << " to " << originalSource 
                        << " via nextHop " << nextHop << " using DQN/DNN policy" << endl;
            } else {
                EV_WARN << "MLMORP: No next hop found for ACK to " << originalSource << ", dropping" << endl;
                delete forwardPacket;
            }
            delete msg;
            return;
        }

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
            double queueCost;

            src = recBeacon->getSrcAddress();
            next = recBeacon->getNextAddress();
            msgSequenceNumber = recBeacon->getSequenceNumber();

            int nodeDegree = recBeacon->getNodeDegree();
            double residualEnergy = recBeacon->getResidualEnergy();
            double dataRate = recBeacon->getDataRate();
            
            // Extract actual signal power from packet
            double signalPower = -1;  // Default value
            if (check_and_cast<Packet*>(msg)->findTag<SignalPowerInd>()!= nullptr) {
                signalPower = check_and_cast<Packet*>(msg)->getTag<SignalPowerInd>()->getPower().get();
            }

            double buffPktNo = recBeacon->getBuffPktNo();  // Default value

            // Update neighbor table for each received beacon
            int interfaceID = check_and_cast<Packet*>(msg)->getTag<InterfaceInd>()->getInterfaceId();
            neighborTable.updateNeighbor(next, interfaceID, recBeacon->getNextPosition(), nodeDegree, residualEnergy, signalPower, buffPktNo, dataRate);                                                               
            neighborTable.removeOldNeighbors(simTime() - neighborLifetime); // To remove the old neighbor that lost the connection

            bool useDNN = par("useDNNRouting").boolValue();
            bool useDNNforCost = par("useDNNforRoutingCost").boolValue();

            // Check if DNN-based routing is enabled
            if (useDNN || useOnlineRL) {

                // Use DNN model to find the next hope without using the routing table

                // Clean up and exit
                delete packet;
                delete msg;
            } else {
                if (useDNNforCost) {
                    // Use DNN model to calculate routing cost

                    // Get DNN prediction as routing score (higher is better)
                    double dnnScore = dnnModel->predict(residualEnergy, dataRate, signalPower, nodeDegree, buffPktNo);

                    // Convert DNN score to cost (lower is better for routing)
                    cost = recBeacon->getCost() + (1.0 - dnnScore);

                    EV_INFO << "DNN-based routing: score=" << dnnScore << ", cost=" << cost << endl;

                } else {
                    // Use traditional cost calculation
                    hopCost = recBeacon->getCost() + 1;
                    energyCost = recBeacon->getCost() + (1 - residualEnergy/energyStorage->getNominalEnergyCapacity().get());
                    bandwidthCost = recBeacon->getCost() + 56000000/dataRate;
                    queueCost = recBeacon->getCost() + std::min(getCurrentBufferPacketNum()/maxQueuePkts, 1.0);
                    cost = alpha*hopCost + beta*energyCost + gamma*bandwidthCost + delta*queueCost;
                    EV_INFO << "Traditional Cost calculation: " << cost << endl;
                }
                
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
                    recBeacon->setBuffPktNo(getCurrentBufferPacketNum());      // Default buffPktNo

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
    else if (msg == rlUpdateTimer) {
        handleRLUpdate();
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
    const auto& networkHeader = getNetworkProtocolHeader(datagram);
    L3Address source = networkHeader->getSourceAddress();
    const L3Address& destination = networkHeader->getDestinationAddress();
    if (source.isUnspecified()) {
        // For locally generated packets, set a concrete source address for routing/ACKs.
        Ipv4Address currentNode = interface80211ptr->getProtocolData<Ipv4InterfaceData>()->getIPAddress();
        source = L3Address(currentNode);
        datagram->addTagIfAbsent<L3AddressReq>()->setSrcAddress(source);
    }
    EV_INFO << "MLMORP: Finding next hop: source = " << source << ", destination = " << destination << endl;
    
    L3Address nextHop;
    if (useOnlineRL && dqnModel != nullptr) {
        // Use reinforcement learning for routing
        nextHop = selectBestNeighborRL(destination, source, datagram->getTreeId());
    } else {
        // Use DNN-based logic to select next hop
        nextHop = selectBestNeighborDNN(destination, source);
    }
    
    datagram->addTagIfAbsent<NextHopAddressReq>()->setNextHopAddress(nextHop);
    if (nextHop.isUnspecified()) {
        EV_WARN << "MLMORP: No next hop found, dropping packet: source = " << source << ", destination = " << destination << endl;
        // Note: Cannot provide feedback when no neighbors available (no neighbor feature vector to store)
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
        if (rt->isLocalAddress(networkHeader->getDestinationAddress())) {
            // Packet reached its destination - confirm delivery locally when tracking is enabled
            if (useOnlineRL && packetTracker != nullptr) {
                double energyNow = energyStorage->getResidualEnergyCapacity().get();
                packetTracker->confirmDelivery(datagram->getTreeId(), simTime(), energyNow);
                EV_INFO << "Packet " << datagram->getTreeId() << " delivered successfully" << endl;
            }
            // Always send ACK so upstream nodes can confirm delivery
            sendAcknowledgment(datagram->getTreeId(), networkHeader->getSourceAddress(), 
                             networkHeader->getDestinationAddress());
        }
          
        // Collect and write packet data to CSV
        collectPacketData(datagram);

    }
    // ------ End of Data Collection ----- //

    // Check if this is a MANET packet (ACKs are MANET too)
    bool isManetPacket = (networkHeader->getProtocol() == &Protocol::manet);
    if (!isManetPacket && datagram->findTag<PacketProtocolTag>() != nullptr) {
        const Protocol *packetProtocol = datagram->getTag<PacketProtocolTag>()->getProtocol();
        isManetPacket = (packetProtocol == &Protocol::manet);
    }

    if (isManetPacket) {
        // Always route MANET control packets so ACKs can traverse non-RL nodes
        const L3Address& destination = networkHeader->getDestinationAddress();
        if (destination.isMulticast() || destination.isBroadcast() || rt->isLocalAddress(destination))
            return ACCEPT;
        else
            return routeDatagram(datagram);
    }

    bool useDNN = par("useDNNRouting").boolValue();
    if (useDNN || useOnlineRL) {
        // If ML-based routing or RL is enabled, use DNN/RL to select the next hop
        if (networkHeader->getProtocol() == &Protocol::udp) {
            const L3Address& destination = networkHeader->getDestinationAddress();
            if (destination.isMulticast() || destination.isBroadcast() || rt->isLocalAddress(destination))
                return ACCEPT;
            else
                return routeDatagram(datagram);
        }
    } else {
        // If ML-based routing is not enabled, fall back to default behavior (could be legacy/routing table)
        return ACCEPT;
    }


    // Accept the packet for forwarding
    return ACCEPT;
}

INetfilter::IHook::Result Mlmorp::datagramLocalOutHook(Packet *datagram)
{
    Enter_Method("datagramLocalOutHook");

    // Extract destination address from the network header
    const auto& networkHeader = getNetworkProtocolHeader(datagram);

    // Check if this is a MANET packet (ACKs are MANET too)
    bool isManetPacket = (networkHeader->getProtocol() == &Protocol::manet);
    if (!isManetPacket && datagram->findTag<PacketProtocolTag>() != nullptr) {
        const Protocol *packetProtocol = datagram->getTag<PacketProtocolTag>()->getProtocol();
        isManetPacket = (packetProtocol == &Protocol::manet);
    }

    if (isManetPacket) {
        // Always route MANET control packets so ACKs can traverse non-RL nodes
        const L3Address& destination = networkHeader->getDestinationAddress();
        if (destination.isMulticast() || destination.isBroadcast() || rt->isLocalAddress(destination))
            return ACCEPT;
        else
            return routeDatagram(datagram);
    }

    bool useDNN = par("useDNNRouting").boolValue();
    if (useDNN || useOnlineRL) {
        // If ML-based routing or RL is enabled, use DNN/RL to select the next hop
        if (networkHeader->getProtocol() == &Protocol::udp) {
            const L3Address& destination = networkHeader->getDestinationAddress();
            if (destination.isMulticast() || destination.isBroadcast() || rt->isLocalAddress(destination))
                return ACCEPT;
            else
                return routeDatagram(datagram);
        }
    } else {
        // If ML-based routing is not enabled, fall back to default behavior (could be legacy/routing table)
        return ACCEPT;
    }

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
L3Address Mlmorp::selectBestNeighborDNN(const L3Address& destination, const L3Address& source) const
{
    // Get current node's address
    Ipv4Address currentNode = interface80211ptr->getProtocolData<Ipv4InterfaceData>()->getIPAddress();
    
    // Get all available neighbors
    std::vector<L3Address> neighbors = neighborTable.getAddresses();
    
    if (neighbors.empty()) {
        return L3Address();
    }
    
    // Filter out self and source address from neighbors (prevent routing loops)
    std::vector<L3Address> validNeighbors;
    for (const auto& neighbor : neighbors) {
        if (neighbor != currentNode && neighbor != source) {
            validNeighbors.push_back(neighbor);
        }
    }
    
    if (validNeighbors.empty()) {
        return L3Address();
    }
    
    // Check if any neighbor is the destination (direct delivery)
    for (const auto& neighbor : validNeighbors) {
        if (neighbor == destination) {
            return neighbor;
        }
    }
    
    // If only one valid neighbor, return it
    if (validNeighbors.size() == 1) {
        return validNeighbors[0];
    }
    
    // Create feature map for each neighbor
    std::map<L3Address, std::vector<double>> neighborFeatures;
    
    for (const auto& neighbor : validNeighbors) {
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
    return dnnModel->selectBestNeighbor(validNeighbors, neighborFeatures);
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

/**
 * Utility function to collect and write packet data to CSV file
 */
void Mlmorp::collectPacketData(Packet *datagram)
{
    const auto& networkHeader = getNetworkProtocolHeader(datagram);
    
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
        // auto data = datagram->peekData(); // get all data from the packet
        // auto regions = data->getAllTags<CreationTimeTag>(); // get all tag regions
        // for (auto &region : regions) { // for each region do
        //     auto creationTime = region.getTag()->getCreationTime().dbl(); // original time
        //     auto delay = simTime() - creationTime; // compute delay
        //     outFile << "," << delay;
        // }

        outFile << endl;
        outFile.close();
    } else {
        std::cout << "Error opening file!" << std::endl;
    }
}

/**
 * Select the best next-hop neighbor using DQN reinforcement learning
 */
L3Address Mlmorp::selectBestNeighborRL(const L3Address& destination, const L3Address& source, int treeId)
{
    // Get current node's address
    Ipv4Address currentNode = interface80211ptr->getProtocolData<Ipv4InterfaceData>()->getIPAddress();
    
    // Get all available neighbors
    std::vector<L3Address> neighbors = neighborTable.getAddresses();
    
    EV_INFO << "DQN Routing: destination=" << destination 
            << ", source=" << source
            << ", neighbors=" << neighbors.size() << ", treeId=" << treeId << endl;
    
    if (neighbors.empty()) {
        EV_WARN << "DQN Routing: No neighbors available" << endl;
        return L3Address();
    }
    
    // Filter out self and source address from neighbors (prevent routing loops)
    std::vector<L3Address> validNeighbors;
    for (const auto& neighbor : neighbors) {
        if (neighbor != currentNode && neighbor != source) {
            validNeighbors.push_back(neighbor);
        }
    }
    
    if (validNeighbors.empty()) {
        EV_WARN << "DQN Routing: No valid neighbors (all are self or source)" << endl;
        return L3Address();
    }
    
    // Check if any neighbor is the destination (direct delivery)
    for (const auto& neighbor : validNeighbors) {
        if (neighbor == destination) {
            EV_INFO << "DQN Routing: Neighbor is destination, direct delivery to " << destination << endl;
            
            // This is a terminal decision - store experience immediately
            bool isTerminalDecision = true;
            std::vector<std::vector<double>> nextNeighborFeaturesList;  // Empty for terminal
            double intermediateReward = -0.01;
            
            std::vector<double> neighborFeatures = buildNeighborFeatureVector(neighbor);
            dqnModel->storeExperience(neighborFeatures, intermediateReward, nextNeighborFeaturesList, 
                                    isTerminalDecision, treeId, currentNode, true);  // Will be updated with final reward
            
            // Still track for RL feedback
            if (packetTracker != nullptr) {
                double currentEnergy = energyStorage->getResidualEnergyCapacity().get();
                packetTracker->trackPacket(treeId, currentNode, destination, neighbor, simTime(),
                                         neighborFeatures, currentEnergy);
            }
            return neighbor;
        }
    }
    
    L3Address selectedNeighbor;
    
    // If only one valid neighbor, use it
    if (validNeighbors.size() == 1) {
        selectedNeighbor = validNeighbors[0];
        EV_INFO << "DQN Routing: Single neighbor, selected=" << selectedNeighbor << endl;
    } else {
        // Epsilon-greedy action selection over neighbors
        double randVal = uniform(0.0, 1.0);
        
        if (randVal < dqnModel->getEpsilon()) {
            // Explore: choose random neighbor
            int randomIndex = intuniform(0, validNeighbors.size() - 1);
            selectedNeighbor = validNeighbors[randomIndex];
            EV_INFO << "DQN Routing: Exploration - randomly selected neighbor=" << selectedNeighbor 
                    << " (epsilon=" << dqnModel->getEpsilon() << ")" << endl;
        } else {
            // Exploit: choose argmax_j Q(x(i,j))
            double bestQValue = -std::numeric_limits<double>::max();
            L3Address bestNeighbor = validNeighbors[0];
            
            for (const auto& neighbor : validNeighbors) {
                std::vector<double> neighborFeatures = buildNeighborFeatureVector(neighbor);
                double qValue = dqnModel->scoreNeighbor(neighborFeatures);
                
                if (qValue > bestQValue) {
                    bestQValue = qValue;
                    bestNeighbor = neighbor;
                }
            }
            
            selectedNeighbor = bestNeighbor;
            EV_INFO << "DQN Routing: Exploitation - selected neighbor=" << selectedNeighbor 
                    << " with Q-value=" << bestQValue << " (epsilon=" << dqnModel->getEpsilon() << ")" << endl;
        }
    }
    
    // Determine if this is a terminal forwarding decision (neighbor is the destination)
    bool isTerminalDecision = (selectedNeighbor == destination);
    
    // Build feature vectors for all neighbors at current time (for bootstrapping)
    std::vector<std::vector<double>> nextNeighborFeaturesList;
    std::vector<L3Address> currentNeighbors = neighborTable.getAddresses();
    
    // Build feature vectors for all valid neighbors (exclude self and source)
    for (const auto& neighbor : currentNeighbors) {
        if (neighbor != currentNode && neighbor != source) {
            std::vector<double> neighborFeatures = buildNeighborFeatureVector(neighbor);
            nextNeighborFeaturesList.push_back(neighborFeatures);
        }
    }
    
    // Calculate intermediate reward (small penalty per hop to encourage shorter paths)
    // This provides immediate learning signal; will be updated with final reward when outcome is known
    double intermediateReward = -0.01;  // Small penalty per hop
    
    // Store experience immediately at forwarding time with pending reward
    // For intermediate hops: done=false enables bootstrapping
    // For destination hops: done=true (terminal)
    std::vector<double> neighborFeatures = buildNeighborFeatureVector(selectedNeighbor);
    dqnModel->storeExperience(neighborFeatures, intermediateReward, nextNeighborFeaturesList, 
                            isTerminalDecision, treeId, currentNode, true);  // rewardPending = true
    
    EV_INFO << "DQN Routing: Stored experience at forwarding time with done=" << isTerminalDecision 
            << ", intermediateReward=" << intermediateReward << ", treeId=" << treeId << endl;
    
    // Track this packet for feedback (store chosen neighbor's feature vector)
    if (packetTracker != nullptr) {
        double currentEnergy = energyStorage->getResidualEnergyCapacity().get();
        packetTracker->trackPacket(treeId, currentNode, destination, selectedNeighbor, simTime(),
                                 neighborFeatures, currentEnergy);
        EV_INFO << "DQN Routing: Packet " << treeId << " tracked for destination " << destination << endl;
    } else {
        EV_WARN << "DQN Routing: packetTracker is null, cannot track packet" << endl;
    }
    
    // Increment packet counter for RL updates
    rlPacketCounter++;
    if (rlPacketCounter >= rlUpdateInterval) {
        performRLUpdate();
        rlPacketCounter = 0;
    }
    
    return selectedNeighbor;
}

/**
 * Build neighbor feature vector x(i,j) for neighbor j
 * Feature vector: [neighborResidualEnergy, neighborDataRate, linkSignalPower(i↔j), neighborQueueLength, neighborNodeDegree]
 */
std::vector<double> Mlmorp::buildNeighborFeatureVector(const L3Address& neighbor) const
{
    std::vector<double> features;
    
    // Feature 1: neighborResidualEnergy
    double residualEnergy = neighborTable.getResidualEnergy(neighbor);
    features.push_back(residualEnergy);
    
    // Feature 2: neighborDataRate
    double dataRate = neighborTable.getDataRate(neighbor);
    features.push_back(dataRate);
    
    // Feature 3: linkSignalPower(i↔j) - signal power from current node to neighbor j
    double signalPower = neighborTable.getSignalPower(neighbor);
    features.push_back(signalPower);
    
    // Feature 4: neighborQueueLength (buffPktNo)
    double buffPktNo = neighborTable.getBuffPktNo(neighbor);
    features.push_back(buffPktNo);
    
    // Feature 5: neighborNodeDegree
    int nodeDegree = neighborTable.getNodeDegree(neighbor);
    features.push_back(static_cast<double>(nodeDegree));
    
    return features;
}

/**
 * Perform reinforcement learning update
 */
void Mlmorp::performRLUpdate()
{
    if (packetTracker == nullptr || dqnModel == nullptr) {
        EV_WARN << "RL Update: Cannot perform update - packetTracker or dqnModel is null" << endl;
        return;
    }
    
    // Check for timed-out packets
    double currentEnergy = energyStorage->getResidualEnergyCapacity().get();
    int timeouts = packetTracker->checkTimeouts(simTime(), currentEnergy);
    if (timeouts > 0) {
        EV_INFO << "RL Update: " << timeouts << " packets timed out" << endl;
    }
    
    // Get completed packets for training
    std::vector<PacketInfo> completedPackets = packetTracker->getCompletedPackets(true);
    
    if (completedPackets.empty()) {
        EV_INFO << "RL Update: No completed packets available for training (tracked=" 
                << packetTracker->getTrackedCount() << ", buffer=" 
                << dqnModel->getReplayBufferSize() << ")" << endl;
        return;
    }
    
    EV_INFO << "RL Update: Processing " << completedPackets.size() << " completed packets" << endl;
    
    // Count confirmed vs failed packets
    int confirmedCount = 0;
    int failedCount = 0;
    for (const auto& packet : completedPackets) {
        if (packet.confirmed) {
            confirmedCount++;
        } else {
            failedCount++;
        }
    }
    if (completedPackets.size() > 0) {
        EV_INFO << "RL Update: Confirmed=" << confirmedCount << ", Failed=" << failedCount << endl;
    }
    
    // Check buffer size before adding experiences
    size_t bufferSizeBefore = dqnModel->getReplayBufferSize();
    
    // Update pending experiences with final rewards when packets complete
    Ipv4Address currentNode = interface80211ptr->getProtocolData<Ipv4InterfaceData>()->getIPAddress();
    
    for (const auto& packet : completedPackets) {
        // Calculate final reward based on delivery outcome
        double finalReward = packetTracker->calculateReward(packet, packet.confirmed, 
                                                           packet.energyAfter, packet.deliveryTime);
        
        // All completed packets are terminal (destination reached or timeout)
        bool isTerminal;

        
        // Case 1: packet timed out or failed → terminal
        if (!packet.confirmed) {
            isTerminal = true;
        }
        // Case 2: packet delivered
        else {
            // terminal ONLY if this hop delivered to destination
            isTerminal = (packet.forwardedTo == packet.destination);
        }
        // Update the pending experience stored at forwarding time with final reward
        // This propagates delivery success/failure to the chosen neighbor
        dqnModel->updatePendingExperienceReward(packet.treeId, currentNode, finalReward, isTerminal);
        
        EV_INFO << "DQN Routing: Updated pending experience for treeId=" << packet.treeId 
                << " with finalReward=" << finalReward << ", confirmed=" << packet.confirmed << endl;
    }
    
    // Check buffer size after adding experiences
    size_t bufferSizeAfter = dqnModel->getReplayBufferSize();
    
    // Perform training step (requires at least batchSize finalized experiences)
    size_t bufferSize = dqnModel->getReplayBufferSize();
    int requiredBatchSize = dqnModel->getBatchSize();
    if (bufferSize < requiredBatchSize) {
        EV_INFO << "RL Update: Buffer too small for training (have " << bufferSize 
                << ", need " << requiredBatchSize << "). Added " << (bufferSizeAfter - bufferSizeBefore) 
                << " experiences. Total tracked=" << packetTracker->getTrackedCount() << endl;
    } else {
        double loss = dqnModel->trainStep();
        EV_INFO << "RL Update: Training step completed, epsilon=" << dqnModel->getEpsilon() 
                << ", replayBuffer=" << dqnModel->getReplayBufferSize() << endl;
    }

    // Log accurate packet tracking statistics (completedPackets has the processed batch)
    int completedTotal = static_cast<int>(completedPackets.size());
    double successRate = completedTotal > 0 ? (static_cast<double>(confirmedCount) / completedTotal) * 100.0 : 0.0;
    EV_INFO << "PacketTracker Stats: Tracked=" << packetTracker->getTrackedCount()
            << ", Completed=" << completedTotal
            << ", Success=" << confirmedCount
            << ", Failed=" << failedCount
            << ", Success Rate=" << successRate << "%" << endl;
}

/**
 * Handle RL update timer events
 */
void Mlmorp::handleRLUpdate()
{
    performRLUpdate();
    
    // Schedule next RL update
    scheduleAfter(1.0, rlUpdateTimer);  // Update every second
}

/**
 * Send acknowledgment packet back to source
 */
void Mlmorp::sendAcknowledgment(int treeId, const L3Address& originalSource, const L3Address& originalDestination)
{
    // Create ACK packet
    auto ackData = makeShared<MlmorpAck>();
    ackData->setTreeId(treeId);
    // Convert L3Address to Ipv4Address
    Ipv4Address origSource = originalSource.toIpv4();
    Ipv4Address origDest = originalDestination.toIpv4();
    ackData->setOriginalSource(origSource);
    ackData->setOriginalDestination(origDest);
    ackData->setDeliveryTime(simTime());
    ackData->setChunkLength(B(20));  // Small ACK packet
    
    auto ackPacket = new Packet("MLMORP-ACK");
    ackPacket->insertAtBack(ackData);
    
    // Set up routing for ACK (send back to original source)
    Ipv4Address currentNode = interface80211ptr->getProtocolData<Ipv4InterfaceData>()->getIPAddress();
    auto addressReq = ackPacket->addTag<L3AddressReq>();
    addressReq->setDestAddress(originalSource);
    addressReq->setSrcAddress(currentNode);
    ackPacket->addTag<InterfaceReq>()->setInterfaceId(interface80211ptr->getInterfaceId());
    ackPacket->addTag<PacketProtocolTag>()->setProtocol(&Protocol::manet);
    ackPacket->addTag<DispatchProtocolReq>()->setProtocol(&Protocol::ipv4);
    
    // Use DQN/DNN policy to find next hop (same as data packets)
    L3Address nextHop;
    if (useOnlineRL && dqnModel != nullptr) {
        // Use reinforcement learning for routing ACK
        nextHop = selectBestNeighborRL(originalSource, L3Address(currentNode), treeId);
    } else {
        // Use DNN-based logic to select next hop for ACK
        nextHop = selectBestNeighborDNN(originalSource, L3Address(currentNode));
    }
    
    if (!nextHop.isUnspecified()) {
        ackPacket->addTagIfAbsent<NextHopAddressReq>()->setNextHopAddress(nextHop);
        // Send ACK packet
        send(ackPacket, "ipOut");
        EV_INFO << "MLMORP: Sent ACK for packet " << treeId << " to " << originalSource 
                << " via nextHop " << nextHop << " using DQN/DNN policy" << endl;
    } else {
        EV_WARN << "MLMORP: No next hop found for ACK to " << originalSource << ", dropping" << endl;
        delete ackPacket;
    }
}

} /* namespace inet */
