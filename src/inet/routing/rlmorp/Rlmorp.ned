// Author: Basheer Al-Qassab

package inet.routing.rlmorp;

import inet.routing.contract.IManetRouting;

simple Rlmorp like IManetRouting
{
    parameters:
        @display("i=block/routing");
        string interfaceTableModule;                          // The path to the InterfaceTable module
        string routingTableModule;                            // The path to the RoutingTable module
        string networkProtocolModule = default("^.ipv4.ip");  // 
        double beaconInterval @unit(s) = default(5s);         // how often beacon messages should be sent out
        string netmask = default("255.255.255.0");            // netmask for the ad-hoc network
        double maxVariance = default(1);                      // This is the maximum of a random value to determine when the first beacon message is sent out
        double routeLifetime @unit(s) = default(5s);          // the expiry time of the routing entries
        double neighborLifetime @unit(s) = default(5s);       // the expiry time of the neighbor
        bool manetPurgeRoutingTables = default(true);         // this parameter will be used if we want to purge the routing table
        volatile double broadcastDelay @unit(s) = default(uniform(0s,0.01s));  // 
        double alpha = default(1);                            // Weight for hop
        double beta = default(0);                             // Weight for energy
        double gamma = default(0);                            // Weight for bandwidth
        
        // Reinforcement Learning Parameters
        bool useOnlineRL = default(false);                    // Enable online reinforcement learning
        int rlUpdateInterval = default(10);                   // RL update interval (packets)
        int dqnStateSize = default(5);                        // DQN state size
        int dqnHiddenSize1 = default(64);                     // DQN first hidden layer size
        int dqnHiddenSize2 = default(32);                     // DQN second hidden layer size
        int dqnMaxActions = default(10);                      // Maximum number of actions (neighbors)
        double dqnLearningRate = default(0.001);              // DQN learning rate
        double dqnEpsilon = default(1.0);                     // Initial exploration rate
        double dqnGamma = default(0.95);                      // Discount factor
        string dqnModelFile = default("");                    // Path to pre-trained DQN model (optional)
        
        // Packet Tracking Parameters
        double packetTrackingTimeout @unit(s) = default(10s); // Timeout for packet delivery tracking
        int maxTrackingHistory = default(1000);               // Maximum tracking history size
        
        // Reward Parameters
        double successReward = default(1.0);                  // Reward for successful packet delivery
        double failureReward = default(-1.0);                 // Reward for failed packet delivery
        double energyWeight = default(0.1);                   // Weight for energy efficiency in reward
        double delayWeight = default(0.01);                   // Weight for delay in reward
    gates:
        input ipIn;
        output ipOut;
}

