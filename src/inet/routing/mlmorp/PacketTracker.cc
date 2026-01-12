// Author: Basheer Al-Qassab
// Simple feedback mechanism for tracking packet delivery success/failure

#include "PacketTracker.h"
#include <sstream>
#include <algorithm>

namespace inet {

PacketTracker::PacketTracker(simtime_t timeout, int maxHistory)
    : defaultTimeout(timeout), maxCompletedHistory(maxHistory),
      successReward(1.0), failureReward(-1.0), energyWeight(0.1), delayWeight(0.1)
{
}

PacketTracker::~PacketTracker()
{
    // Maps and deques will be automatically cleaned up
}

void PacketTracker::trackPacket(int treeId, const L3Address& source, const L3Address& destination,
                               const L3Address& forwardedTo, simtime_t currentTime,
                               const std::vector<double>& neighborFeatures, double currentEnergy)
{
    // Create packet info
    simtime_t timeout = currentTime + defaultTimeout;
    
    // Use emplace to construct in place and avoid copy issues
    trackedPackets.emplace(treeId, PacketInfo(treeId, source, destination, forwardedTo, 
                                             currentTime, timeout, neighborFeatures, currentEnergy));
}

void PacketTracker::confirmDelivery(int treeId, simtime_t deliveryTime, double currentEnergy)
{
    auto it = trackedPackets.find(treeId);
    if (it != trackedPackets.end()) {
        // Mark as confirmed and move to completed
        it->second.confirmed = true;
        it->second.deliveryTime = deliveryTime;
        it->second.energyAfter = currentEnergy;
        completedPackets.emplace_back(it->second);
        
        // Remove from tracking
        trackedPackets.erase(it);
        
        // Limit completed history size
        if (completedPackets.size() > maxCompletedHistory) {
            completedPackets.pop_front();
        }
    }
}

int PacketTracker::checkTimeouts(simtime_t currentTime, double currentEnergy)
{
    int timeoutCount = 0;
    auto it = trackedPackets.begin();
    
    while (it != trackedPackets.end()) {
        if (currentTime >= it->second.timeout) {
            // Packet timed out - mark as failed
            PacketInfo info = it->second;
            info.confirmed = false;  // Explicitly mark as failed
            info.deliveryTime = currentTime;
            info.energyAfter = currentEnergy;
            completedPackets.emplace_back(info);
            
            // Remove from tracking
            it = trackedPackets.erase(it);
            timeoutCount++;
        } else {
            ++it;
        }
    }
    
    // Limit completed history size
    while (completedPackets.size() > maxCompletedHistory) {
        completedPackets.pop_front();
    }
    
    return timeoutCount;
}

std::vector<PacketInfo> PacketTracker::getCompletedPackets(bool clear)
{
    std::vector<PacketInfo> result(completedPackets.begin(), completedPackets.end());
    
    if (clear) {
        completedPackets.clear();
    }
    
    return result;
}

bool PacketTracker::isTracking(int treeId) const
{
    return trackedPackets.find(treeId) != trackedPackets.end();
}

void PacketTracker::setRewardParameters(double success, double failure, double energyW, double delayW)
{
    successReward = success;
    failureReward = failure;
    energyWeight = energyW;
    delayWeight = delayW;
}

double PacketTracker::calculateReward(const PacketInfo& info, bool delivered, 
                                     double currentEnergy, simtime_t deliveryTime)
{
    double baseReward = delivered ? successReward : failureReward;
    
    // Energy efficiency component
    double energyUsed = info.energyBefore - currentEnergy;
    double energyEfficiencyReward = -energyWeight * energyUsed;  // Negative because using energy is bad
    
    // Delay component (only for successful deliveries)
    double delayReward = 0.0;
    if (delivered && deliveryTime > 0) {
        double delay = (deliveryTime - info.forwardTime).dbl();
        delayReward = -delayWeight * delay;  // Negative because longer delay is bad
    }
    
    return baseReward + energyEfficiencyReward + delayReward;
}

void PacketTracker::clear()
{
    trackedPackets.clear();
    completedPackets.clear();
}

std::string PacketTracker::getStatistics() const
{
    std::ostringstream oss;
    
    // Count successful and failed deliveries in completed packets
    int successful = 0;
    int failed = 0;
    
    for (const auto& packet : completedPackets) {
        if (packet.confirmed) {
            successful++;
        } else {
            failed++;
        }
    }
    
    int total = successful + failed;
    double successRate = total > 0 ? (double)successful / total * 100.0 : 0.0;
    
    oss << "PacketTracker Stats: "
        << "Tracked=" << trackedPackets.size()
        << ", Completed=" << total
        << ", Success=" << successful
        << ", Failed=" << failed
        << ", Success Rate=" << successRate << "%";
    
    return oss.str();
}

} // namespace inet
