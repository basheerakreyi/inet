
// Author: Basheer Al-Qassab

package inet.routing.morp;

import inet.routing.contract.IManetRouting;

simple Morp like IManetRouting
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
        bool manetPurgeRoutingTables = default(true);         // this parameter will be used if we want to purge the routing table
        volatile double broadcastDelay @unit(s) = default(uniform(0s,0.01s));  // 
    gates:
        input ipIn;
        output ipOut;
}
