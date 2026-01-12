# MLMORP Online RL Troubleshooting Guide

## Low Packet Delivery Ratio Issues

If you're experiencing low packet delivery ratio (PDR) when using online RL, check the following:

### 1. Check Neighbor Table Population
- **Symptom**: Packets are dropped with "No next hop found" warnings
- **Cause**: Neighbor table may be empty when packets start being sent
- **Solution**: 
  - Check beacon interval: `beaconInterval` should be reasonable (default 5s)
  - Ensure beacons are being sent and received
  - Check logs for "Neighbor table is empty" warnings

### 2. High Initial Epsilon
- **Symptom**: Very poor routing decisions, packets taking wrong paths
- **Cause**: `dqnEpsilon=1.0` means 100% random exploration initially
- **Solution**: 
  - Lower initial epsilon: `dqnEpsilon = 0.3` or `0.5`
  - Or use faster epsilon decay: modify `epsilonDecay` in DQNModel (default 0.995)

### 3. Insufficient Training Data
- **Symptom**: Model makes poor decisions even after many packets
- **Cause**: Replay buffer may not have enough experiences
- **Solution**:
  - Check `rlUpdateInterval` - lower values mean more frequent training
  - Ensure `packetTrackingTimeout` is sufficient for packets to be delivered
  - Check logs for "Buffer too small for training" messages

### 4. Action Selection Issues
- **Symptom**: Packets dropped even when neighbors exist
- **Cause**: Action indices may be out of bounds or selection failing
- **Solution**:
  - Ensure `dqnMaxActions` is at least as large as maximum number of neighbors
  - Check logs for "Action selection failed" warnings

### 5. Confirmation Propagation
- **Symptom**: Packets delivered but not confirmed, leading to poor learning
- **Cause**: Previous hop detection may be failing
- **Solution**:
  - Check logs for "Previous hop unspecified" warnings
  - Ensure `packetTrackingTimeout` is long enough for confirmations to propagate

## Recommended Configuration for Better Initial Performance

```ini
# Lower initial epsilon for less random exploration
*.host[*].routingTable.routingProtocol[*].dqnEpsilon = 0.3

# More frequent RL updates
*.host[*].routingTable.routingProtocol[*].rlUpdateInterval = 5

# Longer timeout for packet tracking
*.host[*].routingTable.routingProtocol[*].packetTrackingTimeout = 15s

# Ensure maxActions is sufficient
*.host[*].routingTable.routingProtocol[*].dqnMaxActions = 20
```

## Debugging Steps

1. **Enable detailed logging**: Check EV_INFO and EV_WARN messages in log files
2. **Monitor neighbor table**: Use `neighborTable.getAddresses().size()` to check if neighbors are discovered
3. **Check packet drops**: Look for "No next hop found" warnings
4. **Monitor RL training**: Check if replay buffer is filling up and training is occurring
5. **Verify beacon exchange**: Ensure beacons are being sent and received

## Common Error Messages

- **"No neighbors available"**: Neighbor table is empty - check beacon exchange
- **"Action selection failed"**: Action index out of bounds - increase `dqnMaxActions`
- **"Previous hop unspecified"**: Confirmation propagation may fail - check previous hop detection
- **"Buffer too small for training"**: Need more experiences - lower `rlUpdateInterval` or wait longer















