// Author: Basheer Al-Qassab

#include "inet/routing/rlmorp/Rlmorp.h"

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

Define_Module(Rlmorp);

Rlmorp::Rlmorp()
{

}

Rlmorp::~Rlmorp()
{
    stop();

    // Dispose of dynamically allocated the objects
    delete event;
    delete purgeTimer;
    delete dqnModel;
    delete packetTracker;
    delete rlUpdateTimer;
}

void Rlmorp::initialize(int stage)
{
    RoutingProtocolBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        sequenceNumber = 0;
        event = new cMessage("event");
        purgeTimer = new cMessage("purge");
        rlUpdateTimer = new cMessage("rlUpdate");

        // Getting RLMORP parameters
        routeLifetime = par("routeLifetime").doubleValue();
        neighborLifetime = par("neighborLifetime").doubleValue();
        beaconInterval = par("beaconInterval");
        broadcastDelay = &par("broadcastDelay");
        alpha = par("alpha").doubleValue();
        beta = par("beta").doubleValue();
        gamma = par("gamma").doubleValue();

        // Initialize Reinforcement Learning components
        useOnlineRL = par("useOnlineRL").boolValue();
        rlUpdateInterval = par("rlUpdateInterval").intValue();
        rlPacketCounter = 0;
        
        if (useOnlineRL) {
            // Initialize DQN model
            int dqnStateSize = par("dqnStateSize").intValue();
            int dqnHiddenSize1 = par("dqnHiddenSize1").intValue();
            int dqnHiddenSize2 = par("dqnHiddenSize2").intValue();
            int dqnMaxActions = par("dqnMaxActions").intValue();
            double dqnLearningRate = par("dqnLearningRate").doubleValue();
            double dqnEpsilon = par("dqnEpsilon").doubleValue();
            double dqnGamma = par("dqnGamma").doubleValue();
            
            dqnModel = new DQNModel(dqnStateSize, dqnHiddenSize1, dqnHiddenSize2, 
                                   dqnMaxActions, dqnLearningRate, dqnEpsilon, dqnGamma);
            
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

void Rlmorp::start()
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
        throw cRuntimeError("RLMORP has found %i 80211 interfaces", num_80211);

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

void Rlmorp::stop()
{
    cancelEvent(event);
    cancelEvent(purgeTimer);
    cancelEvent(rlUpdateTimer);
}

void Rlmorp::handleMessageWhenUp(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        handleSelfMessage(msg);
    }
    else if (check_and_cast<Packet*>(msg)->getTag<PacketProtocolTag>()->getProtocol() == &Protocol::manet) {
        
        // Check if this is an ACK packet - use peekData with nullptr flag to avoid conversion errors
        auto receivedPacket = check_and_cast<Packet*>(msg);
        const Ptr<const Chunk> chunk = receivedPacket->peekData();
        auto ackData = dynamicPtrCast<const RlmorpAck>(chunk);
        if (ackData != nullptr && msg->arrivedOn("ipIn")) {
            // Handle ACK packet
            int treeId = ackData->getTreeId();
            Ipv4Address originalSource = ackData->getOriginalSource();
            Ipv4Address originalDestination = ackData->getOriginalDestination();
            
            EV_INFO << "RLMORP: Received ACK for packet " << treeId 
                    << " (source=" << originalSource << ", dest=" << originalDestination << ")" << endl;
            
            // Check if we're the original source
            Ipv4Address currentNode = interface80211ptr->getProtocolData<Ipv4InterfaceData>()->getIPAddress();
            if (originalSource == currentNode) {
                // We're the source - just log and drop
                EV_INFO << "RLMORP: ACK reached original source, dropping" << endl;
                delete msg;
                return;
            }
            
            // We're an intermediate node - confirm delivery in our tracker
            if (useOnlineRL && packetTracker != nullptr) {
                double energyNow = energyStorage->getResidualEnergyCapacity().get();
                if (packetTracker->isTracking(treeId)) {
                    packetTracker->confirmDelivery(treeId, ackData->getDeliveryTime(), energyNow);
                    EV_INFO << "RLMORP: Confirmed delivery of packet " << treeId << " via ACK" << endl;
                } else {
                    EV_DETAIL << "RLMORP: ACK received for packet " << treeId 
                              << " but not tracking (may have timed out)" << endl;
                }
            }
            
            // Forward ACK back to original source using RL policy
            // Create a new packet with routing information
            auto forwardPacket = new Packet("RLMORP-ACK");
            auto forwardAckData = ackData->dupShared();
            forwardPacket->insertAtBack(forwardAckData);
            
            auto forwardAddressReq = forwardPacket->addTag<L3AddressReq>();
            forwardAddressReq->setDestAddress(L3Address(originalSource));
            forwardAddressReq->setSrcAddress(L3Address(currentNode));
            forwardPacket->addTag<InterfaceReq>()->setInterfaceId(interface80211ptr->getInterfaceId());
            forwardPacket->addTag<PacketProtocolTag>()->setProtocol(&Protocol::manet);
            forwardPacket->addTag<DispatchProtocolReq>()->setProtocol(&Protocol::ipv4);
            
            // Use RL policy to find next hop
            L3Address nextHop;
            if (useOnlineRL && dqnModel != nullptr) {
                // Use reinforcement learning for routing ACK
                nextHop = selectBestNeighborRL(L3Address(originalSource), L3Address(currentNode), treeId);
            } else {
                // Fallback: use routing table or drop
                EV_WARN << "RLMORP: No RL model available for ACK routing, dropping" << endl;
                delete forwardPacket;
                delete msg;
                return;
            }
            
            if (!nextHop.isUnspecified()) {
                forwardPacket->addTagIfAbsent<NextHopAddressReq>()->setNextHopAddress(nextHop);
                send(forwardPacket, "ipOut");
                EV_INFO << "RLMORP: Forwarded ACK for packet " << treeId << " to " << originalSource 
                        << " via nextHop " << nextHop << " using RL policy" << endl;
            } else {
                EV_WARN << "RLMORP: No next hop found for ACK to " << originalSource << ", dropping" << endl;
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

        // When RLMORP module receives RlmorpBeacon from other host
        // it adds/replaces the information in routing table for the one contained in the message
        // but only if it's useful/up-to-date. If not the RLMORP module ignores the message.
        auto recBeacon = staticPtrCast<RlmorpBeacon>(check_and_cast<Packet*>(msg)->peekData<RlmorpBeacon>()->dupShared());
        if (msg->arrivedOn("ipIn")) {
            ASSERT(recBeacon);

            // reads RLMORP beacon message fields
            Ipv4Address src;
            Ipv4Address next;
            unsigned int msgSequenceNumber;
            float cost;

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
            neighborTable.updateNeighbor(next, interfaceID, recBeacon->getNextPosition(), nodeDegree, residualEnergy, signalPower, buffPktNo);                                                               
            neighborTable.removeOldNeighbors(simTime() - neighborLifetime); // To remove the old neighbor that lost the connection

            // Always calculate traditional cost for dataset collection
            // This cost is computed regardless of whether RL is enabled
            cost = calculateTraditionalCost(recBeacon->getCost(), residualEnergy, dataRate);
            EV_INFO << "RLMORP: Traditional Cost calculation: " << cost << endl;
                
            Ipv4Address source = interface80211ptr->getProtocolData<Ipv4InterfaceData>()->getIPAddress();

            if (src == source) {
                EV_INFO << "Beacon message is dropped because the message is returned to the original node.\n";
                delete packet;
                delete msg;
                return;
            }

            Ipv4Route *_input_routing = rt->findBestMatchingRoute(src);
            RlmorpRouteData *input_routing = dynamic_cast<RlmorpRouteData*>(_input_routing);

            // Tests if the RLMORP beacon message that arrived is useful
            if (_input_routing == nullptr
                        || (_input_routing != nullptr && _input_routing->getNetmask() != Ipv4Address::ALLONES_ADDRESS)
                        || (input_routing != nullptr && (msgSequenceNumber > input_routing->getSequenceNumber() || (msgSequenceNumber == input_routing->getSequenceNumber() && cost < input_routing->getRouteCost()))))
            {
                // remove old entry
                if (input_routing != nullptr){
                    rt->deleteRoute(input_routing);
                }

                // adds new information to routing table according to information in beacon message
                {
                    Ipv4Address netmask = Ipv4Address::ALLONES_ADDRESS;
                    RlmorpRouteData *e = new RlmorpRouteData();
                    e->setDestination(src);
                    e->setNetmask(netmask);
                    e->setGateway(next);
                    e->setInterface(interface80211ptr);
                    e->setSourceType(IRoute::MANET);
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
        } else
            throw cRuntimeError("Message arrived on unknown gate %s", msg->getArrivalGate()->getName());
    } else {
        // throw cRuntimeError("Message not supported %s", msg->getName());
        // Accept and pass through non-MANET packets (ICMP, etc.)
        // These are normal network messages that shouldn't cause errors
        delete msg;
        return;
    }
}

void Rlmorp::handleSelfMessage(cMessage *msg)
{
    // When RLMORP module receives self-message (scheduled event)
    // it means that it's time for beacon message broadcast event
    if (msg == event) {

        // Purge the routing table (this to remove the expired routes)
        purge();

        auto beacon = makeShared<RlmorpBeacon>(); // Created new RLMORP beacon

        // Set the packet fields in RlmorpBeacon
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

        // Created new packet for RlmorpBeacon
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

void Rlmorp::reschedulePurgeTimer()
{
    simtime_t purgeTime = SimTime::getMaxTime();
    for (int i = 0; i < rt->getNumRoutes(); i++) {
        auto route = dynamic_cast<RlmorpRouteData *>(rt->getRoute(i));
        if (route && !route->isExpired() && route->getExpirTime() < purgeTime)
            purgeTime = route->getExpirTime();
    }
    cancelEvent(purgeTimer);
    if (purgeTime != SimTime::getMaxTime())
        scheduleAt(purgeTime, purgeTimer);
}

void Rlmorp::purge()
{
    for (int i = 0; i < rt->getNumRoutes();) {
        auto route = dynamic_cast<RlmorpRouteData *>(rt->getRoute(i));
        if (route && route->isExpired())
            rt->deleteRoute(route);
        else
            i++;
    }
}

//
// NetFilter
//

INetfilter::IHook::Result Rlmorp::routeDatagram(Packet *datagram)
{
    const auto& networkHeader = getNetworkProtocolHeader(datagram);
    const L3Address& source = networkHeader->getSourceAddress();
    const L3Address& destination = networkHeader->getDestinationAddress();
    EV_INFO << "RLMORP: Finding next hop: source = " << source << ", destination = " << destination << endl;
    
    L3Address nextHop;
    if (useOnlineRL && dqnModel != nullptr) {
        // Use reinforcement learning for routing
        nextHop = selectBestNeighborRL(destination, source, datagram->getTreeId());
    } else {
        // Fallback: use routing table
        IRoute *route = rt->findBestMatchingRoute(destination);
        if (route != nullptr) {
            nextHop = route->getNextHopAsGeneric();
        }
    }
    
    datagram->addTagIfAbsent<NextHopAddressReq>()->setNextHopAddress(nextHop);
    if (nextHop.isUnspecified()) {
        EV_WARN << "RLMORP: No next hop found, dropping packet: source = " << source << ", destination = " << destination << endl;
        if (useOnlineRL && dqnModel != nullptr) {
            // Provide immediate negative feedback for drops with no neighbor
            std::vector<double> state = getCurrentState();
            dqnModel->storeExperience(state, 0, -1.0, state, true, 0);
        }
        if (hasGUI())
            getContainingNode(this)->bubble("No next hop found, dropping packet");
        return DROP;
    }
    else {
        EV_INFO << "RLMORP: Next hop found: source = " << source << ", destination = " << destination << ", nextHop: " << nextHop << endl;
        auto networkInterface = interface80211ptr;
        datagram->addTagIfAbsent<InterfaceReq>()->setInterfaceId(networkInterface->getInterfaceId());
        return ACCEPT;
    }
}

INetfilter::IHook::Result Rlmorp::datagramPreRoutingHook(Packet *datagram)
{
    Enter_Method("datagramPreRoutingHook");

    const auto& networkHeader = getNetworkProtocolHeader(datagram);

    // ---- Data Collection --- //
    if ((networkHeader->getProtocol() == &Protocol::udp)) {
        
        // Check if this packet reached its destination (for RL feedback)
        if (useOnlineRL && packetTracker != nullptr) {
            if (rt->isLocalAddress(networkHeader->getDestinationAddress())) {
                // Packet reached its destination - confirm delivery locally
                double energyNow = energyStorage->getResidualEnergyCapacity().get();
                packetTracker->confirmDelivery(datagram->getTreeId(), simTime(), energyNow);
                EV_INFO << "Packet " << datagram->getTreeId() << " delivered successfully" << endl;
                
                // Send acknowledgment back to source so intermediate nodes can confirm delivery
                sendAcknowledgment(datagram->getTreeId(), networkHeader->getSourceAddress(), 
                                 networkHeader->getDestinationAddress());
            }
        }
          
        // Output the information about the received packet
        // This includes traditional cost calculation for dataset collection
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
                        << "," << getCurrentBufferPacketNum();
            }

            outFile << endl;
            outFile.close();
        } else {
            std::cout << "Error opening file!" << std::endl;
        }

    }
    // ------ End of Data Collection ----- //

    if (useOnlineRL) {
        // Check if this is an ACK packet (MANET protocol)
        bool isAckPacket = false;
        if (datagram->findTag<PacketProtocolTag>() != nullptr) {
            const Protocol *packetProtocol = datagram->getTag<PacketProtocolTag>()->getProtocol();
            if (packetProtocol == &Protocol::manet) {
                // Check if it's an ACK packet by trying to peek at the data
                const Ptr<const Chunk> chunk = datagram->peekData();
                auto ackData = dynamicPtrCast<const RlmorpAck>(chunk);
                isAckPacket = (ackData != nullptr);
            }
        }
        
        // If RL is enabled, use RL to select the next hop        
        if ((networkHeader->getProtocol() == &Protocol::udp) || isAckPacket) {
            // Apply to UDP packets or ACK packets
            // Extract destination address from the network header
            const L3Address& destination = networkHeader->getDestinationAddress();            
            if (destination.isMulticast() || destination.isBroadcast() || rt->isLocalAddress(destination))
                return ACCEPT;
            else
                return routeDatagram(datagram);
        }
    } else {
        // If RL is not enabled, fall back to routing table
        return ACCEPT;
    }

    // Accept the packet for forwarding
    return ACCEPT;
}

INetfilter::IHook::Result Rlmorp::datagramLocalOutHook(Packet *datagram)
{
    Enter_Method("datagramLocalOutHook");

    // Extract destination address from the network header
    const auto& networkHeader = getNetworkProtocolHeader(datagram);

    if (useOnlineRL) {
        // Check if this is an ACK packet (MANET protocol)
        bool isAckPacket = false;
        if (datagram->findTag<PacketProtocolTag>() != nullptr) {
            const Protocol *packetProtocol = datagram->getTag<PacketProtocolTag>()->getProtocol();
            if (packetProtocol == &Protocol::manet) {
                // Check if it's an ACK packet by trying to peek at the data
                const Ptr<const Chunk> chunk = datagram->peekData();
                auto ackData = dynamicPtrCast<const RlmorpAck>(chunk);
                isAckPacket = (ackData != nullptr);
            }
        }
        
        // If RL is enabled, use RL to select the next hop        
        if ((networkHeader->getProtocol() == &Protocol::udp) || isAckPacket) {
            // Apply to UDP packets or ACK packets
            // Extract destination address from the network header
            const L3Address& destination = networkHeader->getDestinationAddress();            
            if (destination.isMulticast() || destination.isBroadcast() || rt->isLocalAddress(destination))
                return ACCEPT;
            else
                return routeDatagram(datagram);
        }
    } else {
        // If RL is not enabled, fall back to routing table
        return ACCEPT;
    }

    return ACCEPT;
}

//
// notification
//

void Rlmorp::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) {
    Enter_Method("%s", cComponent::getSignalName(signalID));

    if (nodeStatus->getState() == NodeStatus::DOWN) {
        std::cout << simTime() << endl; // << "The node is down at "
    }
}

/**
 * Calculate traditional routing cost (for dataset collection)
 */
double Rlmorp::calculateTraditionalCost(float recBeaconCost, double residualEnergy, double dataRate) const
{
    double hopCost = recBeaconCost + 1;
    double energyCost = recBeaconCost + (1 - residualEnergy/energyStorage->getNominalEnergyCapacity().get());
    double bandwidthCost = recBeaconCost + 56000000/dataRate;
    double cost = alpha*hopCost + beta*energyCost + gamma*bandwidthCost;
    return cost;
}

// Utility function to get the current MAC buffer (pendingQueue) packet count
int Rlmorp::getCurrentBufferPacketNum() const
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
                        return numPackets;
                    }
                }
            }
        }
    }
    return -1;
}

/**
 * Select the best next-hop neighbor using DQN reinforcement learning
 */
L3Address Rlmorp::selectBestNeighborRL(const L3Address& destination, const L3Address& source, int treeId)
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
            // Still track for RL feedback
            if (packetTracker != nullptr) {
                std::vector<double> state = getCurrentState();
                double currentEnergy = energyStorage->getResidualEnergyCapacity().get();
                packetTracker->trackPacket(treeId, currentNode, destination, neighbor, simTime(),
                                         state, 0, currentEnergy, validNeighbors.size());
            }
            return neighbor;
        }
    }
    
    // Get current state features
    std::vector<double> state = getCurrentState();
    
    L3Address selectedNeighbor;
    int selectedAction = 0;
    int actionSpaceSize = 0;
    
    // If only one valid neighbor, use it (action 0)
    if (validNeighbors.size() == 1) {
        selectedNeighbor = validNeighbors[0];
        selectedAction = 0;
        actionSpaceSize = 1;
        EV_INFO << "DQN Routing: Single neighbor, selected=" << selectedNeighbor << endl;
    } else {
        // Convert neighbors to action indices
        std::vector<int> availableActions;
        for (int i = 0; i < validNeighbors.size() && i < dqnModel->getMaxActions(); i++) {
            availableActions.push_back(i);
        }
        actionSpaceSize = availableActions.size();
        
        // Select action using DQN
        selectedAction = dqnModel->selectAction(state, availableActions);
        
        if (selectedAction >= 0 && selectedAction < validNeighbors.size()) {
            selectedNeighbor = validNeighbors[selectedAction];
            EV_INFO << "DQN Routing: Selected neighbor=" << selectedNeighbor 
                    << " (action=" << selectedAction << ", epsilon=" << dqnModel->getEpsilon() << ")" << endl;
        } else {
            // Fallback to first neighbor if action selection fails
            selectedNeighbor = validNeighbors[0];
            selectedAction = 0;
            EV_WARN << "DQN Routing: Action selection failed, using first neighbor=" << selectedNeighbor << endl;
        }
    }
    
    // Track this packet for feedback
    if (packetTracker != nullptr) {
        double currentEnergy = energyStorage->getResidualEnergyCapacity().get();
        packetTracker->trackPacket(treeId, currentNode, destination, selectedNeighbor, simTime(),
                                 state, selectedAction, currentEnergy, actionSpaceSize);
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
 * Get current state features for reinforcement learning
 */
std::vector<double> Rlmorp::getCurrentState() const
{
    std::vector<double> state;
    
    // Feature 1: Residual energy (normalized)
    double residualEnergy = energyStorage->getResidualEnergyCapacity().get();
    state.push_back(residualEnergy);
    
    // Feature 2: Data rate
    double dataRate = interface80211ptr->getDatarate();
    state.push_back(dataRate);
    
    // Feature 3: Average signal power from neighbors
    double avgSignalPower = 0.0;
    std::vector<L3Address> neighbors = neighborTable.getAddresses();
    if (!neighbors.empty()) {
        for (const auto& neighbor : neighbors) {
            avgSignalPower += neighborTable.getSignalPower(neighbor);
        }
        avgSignalPower /= neighbors.size();
    }
    state.push_back(avgSignalPower);
    
    // Feature 4: Node degree (number of neighbors)
    state.push_back(static_cast<double>(neighbors.size()));
    
    // Feature 5: Buffer packet number
    double buffPktNo = getCurrentBufferPacketNum();
    state.push_back(buffPktNo);
    
    return state;
}

/**
 * Perform reinforcement learning update
 */
void Rlmorp::performRLUpdate()
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
    
    // Add experiences to DQN replay buffer
    for (const auto& packet : completedPackets) {
        // Calculate reward using configurable parameters from PacketTracker
        double reward = packetTracker->calculateReward(packet, packet.confirmed, packet.energyAfter, packet.deliveryTime);
        
        // Get next state (current state)
        std::vector<double> nextState = getCurrentState();
        
        // Store experience with the action space size captured at decision time
        dqnModel->storeExperience(packet.state, packet.action, reward, nextState, true, packet.availableActions);
    }
    
    // Check buffer size after adding experiences
    size_t bufferSizeAfter = dqnModel->getReplayBufferSize();
    
    // Perform training step (requires at least 32 experiences by default)
    size_t bufferSize = dqnModel->getReplayBufferSize();
    if (bufferSize < 32) {
        EV_INFO << "RL Update: Buffer too small for training (have " << bufferSize 
                << ", need 32). Added " << (bufferSizeAfter - bufferSizeBefore) 
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
void Rlmorp::handleRLUpdate()
{
    performRLUpdate();
    
    // Schedule next RL update
    scheduleAfter(1.0, rlUpdateTimer);  // Update every second
}

/**
 * Send acknowledgment packet back to source
 */
void Rlmorp::sendAcknowledgment(int treeId, const L3Address& originalSource, const L3Address& originalDestination)
{
    if (!useOnlineRL || packetTracker == nullptr) {
        return;  // ACK only needed for online RL
    }
    
    // Create ACK packet
    auto ackData = makeShared<RlmorpAck>();
    ackData->setTreeId(treeId);
    // Convert L3Address to Ipv4Address
    Ipv4Address origSource = originalSource.toIpv4();
    Ipv4Address origDest = originalDestination.toIpv4();
    ackData->setOriginalSource(origSource);
    ackData->setOriginalDestination(origDest);
    ackData->setDeliveryTime(simTime());
    ackData->setChunkLength(B(20));  // Small ACK packet
    
    auto ackPacket = new Packet("RLMORP-ACK");
    ackPacket->insertAtBack(ackData);
    
    // Set up routing for ACK (send back to original source)
    Ipv4Address currentNode = interface80211ptr->getProtocolData<Ipv4InterfaceData>()->getIPAddress();
    auto addressReq = ackPacket->addTag<L3AddressReq>();
    addressReq->setDestAddress(originalSource);
    addressReq->setSrcAddress(currentNode);
    ackPacket->addTag<InterfaceReq>()->setInterfaceId(interface80211ptr->getInterfaceId());
    ackPacket->addTag<PacketProtocolTag>()->setProtocol(&Protocol::manet);
    ackPacket->addTag<DispatchProtocolReq>()->setProtocol(&Protocol::ipv4);
    
    // Use RL policy to find next hop
    L3Address nextHop;
    if (useOnlineRL && dqnModel != nullptr) {
        // Use reinforcement learning for routing ACK
        nextHop = selectBestNeighborRL(originalSource, L3Address(currentNode), treeId);
    } else {
        EV_WARN << "RLMORP: No RL model available for ACK routing, dropping" << endl;
        delete ackPacket;
        return;
    }
    
    if (!nextHop.isUnspecified()) {
        ackPacket->addTagIfAbsent<NextHopAddressReq>()->setNextHopAddress(nextHop);
        // Send ACK packet
        send(ackPacket, "ipOut");
        EV_INFO << "RLMORP: Sent ACK for packet " << treeId << " to " << originalSource 
                << " via nextHop " << nextHop << " using RL policy" << endl;
    } else {
        EV_WARN << "RLMORP: No next hop found for ACK to " << originalSource << ", dropping" << endl;
        delete ackPacket;
    }
}

} /* namespace inet */

