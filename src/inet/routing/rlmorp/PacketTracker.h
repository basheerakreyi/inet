// Author: Basheer Al-Qassab
// Simple feedback mechanism for tracking packet delivery success/failure

#ifndef __INET_PACKETTRACKER_H
#define __INET_PACKETTRACKER_H

#include <map>
#include <vector>
#include <deque>
#include "inet/common/INETDefs.h"
#include "inet/networklayer/common/L3Address.h"
#include "inet/common/packet/Packet.h"
#include "inet/common/INETMath.h"
#include "omnetpp.h"

namespace inet {

/**
 * Information about a forwarded packet for tracking
 */
struct PacketInfo {
    int treeId;                     // Unique packet identifier
    L3Address source;               // Source address
    L3Address destination;          // Destination address
    L3Address forwardedTo;          // Next hop we forwarded to
    simtime_t forwardTime;          // Time when packet was forwarded
    simtime_t timeout;              // Timeout for delivery confirmation
    std::vector<double> state;      // State when decision was made
    int action;                     // Action taken (neighbor index)
    double energyBefore;            // Energy before forwarding
    double energyAfter;             // Energy after completion/timeout
    simtime_t deliveryTime;         // Time of delivery or timeout
    int availableActions;           // Number of valid actions at decision time
    bool confirmed;                 // Whether delivery was confirmed
    
    // Default constructor
    PacketInfo() : treeId(0), forwardTime(SIMTIME_ZERO), timeout(SIMTIME_ZERO), action(0), energyBefore(0.0), energyAfter(0.0), deliveryTime(SIMTIME_ZERO), availableActions(0), confirmed(false) {}
    
    // Parameterized constructor
    PacketInfo(int tid, const L3Address& src, const L3Address& dst, 
               const L3Address& fwd, simtime_t time, simtime_t to,
               const std::vector<double>& s, int a, double energy, int actions)
        : treeId(tid), source(src), destination(dst), forwardedTo(fwd),
          forwardTime(time), timeout(to), state(s), action(a), 
          energyBefore(energy), energyAfter(0.0), deliveryTime(SIMTIME_ZERO),
          availableActions(actions), confirmed(false) {}
    
    // Copy constructor (explicitly defined to ensure proper copying)
    PacketInfo(const PacketInfo& other)
        : treeId(other.treeId), source(other.source), destination(other.destination),
          forwardedTo(other.forwardedTo), forwardTime(other.forwardTime),
          timeout(other.timeout), state(other.state), action(other.action),
          energyBefore(other.energyBefore), energyAfter(other.energyAfter),
          deliveryTime(other.deliveryTime), availableActions(other.availableActions),
          confirmed(other.confirmed) {}
    
    // Assignment operator
    PacketInfo& operator=(const PacketInfo& other) {
        if (this != &other) {
            treeId = other.treeId;
            source = other.source;
            destination = other.destination;
            forwardedTo = other.forwardedTo;
            forwardTime = other.forwardTime;
            timeout = other.timeout;
            state = other.state;
            action = other.action;
            energyBefore = other.energyBefore;
            energyAfter = other.energyAfter;
            deliveryTime = other.deliveryTime;
            availableActions = other.availableActions;
            confirmed = other.confirmed;
        }
        return *this;
    }
};

/**
 * Simple packet tracker for providing feedback to reinforcement learning.
 * 
 * This class tracks packets that have been forwarded and provides feedback
 * about their delivery success or failure. It uses a simple timeout-based
 * mechanism and packet arrival notifications to determine success.
 * 
 * Features:
 * - Track forwarded packets with timeout
 * - Confirm delivery based on arrival at destination
 * - Calculate rewards based on delivery success, energy, and delay
 * - Provide experience tuples for RL training
 */
class INET_API PacketTracker
{
private:
    // Tracking data structures
    std::map<int, PacketInfo> trackedPackets;       // treeId -> PacketInfo
    std::deque<PacketInfo> completedPackets;        // Recently completed packets
    
    // Configuration
    simtime_t defaultTimeout;                       // Default timeout for packet delivery
    int maxCompletedHistory;                        // Maximum completed packets to keep
    
    // Reward calculation parameters
    double successReward;                           // Reward for successful delivery
    double failureReward;                           // Reward for failed delivery
    double energyWeight;                            // Weight for energy efficiency
    double delayWeight;                             // Weight for delivery delay

public:
    /**
     * Constructor
     * @param timeout Default timeout for packet tracking (default: 10s)
     * @param maxHistory Maximum number of completed packets to keep (default: 1000)
     */
    PacketTracker(simtime_t timeout = 10.0, int maxHistory = 1000);
    
    /**
     * Destructor
     */
    ~PacketTracker();
    
    /**
     * Track a forwarded packet
     * @param treeId Unique packet identifier
     * @param source Source address
     * @param destination Destination address
     * @param forwardedTo Next hop address
     * @param currentTime Current simulation time
     * @param state State features when decision was made
     * @param action Action taken (neighbor index)
     * @param currentEnergy Current energy level
     * @param availableActions Number of valid actions when the decision was made
     */
    void trackPacket(int treeId, const L3Address& source, const L3Address& destination,
                    const L3Address& forwardedTo, simtime_t currentTime,
                    const std::vector<double>& state, int action, double currentEnergy,
                    int availableActions);
    
    /**
     * Confirm packet delivery (called when packet reaches destination)
     * @param treeId Packet identifier
     * @param deliveryTime Time of delivery
     * @param currentEnergy Current energy level on delivery
     */
    void confirmDelivery(int treeId, simtime_t deliveryTime, double currentEnergy);
    
    /**
     * Check for timed-out packets and mark them as failed
     * @param currentTime Current simulation time
     * @param currentEnergy Current energy level
     * @return Number of packets that timed out
     */
    int checkTimeouts(simtime_t currentTime, double currentEnergy);
    
    /**
     * Get completed packets (delivered or failed) for RL training
     * @param clear Whether to clear the completed packets list after retrieval
     * @return Vector of completed packet information
     */
    std::vector<PacketInfo> getCompletedPackets(bool clear = true);
    
    /**
     * Check if a packet is being tracked
     * @param treeId Packet identifier
     * @return True if packet is being tracked
     */
    bool isTracking(int treeId) const;
    
    /**
     * Get number of currently tracked packets
     * @return Number of packets being tracked
     */
    size_t getTrackedCount() const { return trackedPackets.size(); }
    
    /**
     * Get number of completed packets in history
     * @return Number of completed packets
     */
    size_t getCompletedCount() const { return completedPackets.size(); }
    
    /**
     * Set reward calculation parameters
     * @param success Reward for successful delivery
     * @param failure Reward for failed delivery (typically negative)
     * @param energyW Weight for energy efficiency component
     * @param delayW Weight for delay component
     */
    void setRewardParameters(double success, double failure, double energyW, double delayW);
    
    /**
     * Calculate reward based on delivery outcome, energy, and delay
     * @param info Packet information
     * @param delivered Whether packet was successfully delivered
     * @param currentEnergy Current energy level
     * @param deliveryTime Time of delivery (for successful packets)
     * @return Calculated reward
     */
    double calculateReward(const PacketInfo& info, bool delivered, 
                          double currentEnergy, simtime_t deliveryTime = 0);
    
    /**
     * Clear all tracking data
     */
    void clear();
    
    /**
     * Get statistics about packet delivery
     * @return String with delivery statistics
     */
    std::string getStatistics() const;
};

} // namespace inet

#endif // __INET_PACKETTRACKER_H

