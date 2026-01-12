# MLMORP with Online Reinforcement Learning

This document describes the reinforcement learning extension for the MLMORP routing protocol, which adds online learning capabilities using Deep Q-Networks (DQN).

## Overview

The RL extension enables MLMORP nodes to learn optimal routing decisions online through interaction with the network environment. Key features include:

- **Online Learning**: Nodes adapt their routing strategies based on real-time feedback
- **DQN Algorithm**: Uses Deep Q-Networks with experience replay and target networks
- **Feedback Mechanism**: Tracks packet delivery success/failure for reward calculation
- **Modular Design**: Can be enabled/disabled without affecting existing functionality

## Architecture

### Core Components

1. **DQNModel**: Deep Q-Network implementation for routing decisions
2. **PacketTracker**: Tracks packet delivery for feedback and reward calculation
3. **Extended Mlmorp**: Main protocol with integrated RL capabilities

### State Representation

The RL agent uses the following state features:
- Residual energy (normalized)
- Data rate 
- Average signal power from neighbors
- Node degree (number of neighbors)
- Buffer packet count

### Action Space

Actions correspond to selecting the next-hop neighbor from available neighbors.

### Reward Function

The reward combines multiple objectives:
- **Base reward**: +1 for successful delivery, -1 for failure
- **Energy penalty**: -0.1 × energy_used
- **Delay penalty**: -0.01 × delay (for successful packets)

## Configuration

### NED Parameters

Add the following parameters to your NED file or configuration:

```ned
// Enable reinforcement learning
*.host[*].routingTable.routingProtocol[*].useOnlineRL = true

// RL update frequency
*.host[*].routingTable.routingProtocol[*].rlUpdateInterval = 10

// DQN model architecture
*.host[*].routingTable.routingProtocol[*].dqnStateSize = 5
*.host[*].routingTable.routingProtocol[*].dqnHiddenSize1 = 64
*.host[*].routingTable.routingProtocol[*].dqnHiddenSize2 = 32
*.host[*].routingTable.routingProtocol[*].dqnMaxActions = 10

// Learning parameters
*.host[*].routingTable.routingProtocol[*].dqnLearningRate = 0.001
*.host[*].routingTable.routingProtocol[*].dqnEpsilon = 1.0
*.host[*].routingTable.routingProtocol[*].dqnGamma = 0.95

// Packet tracking
*.host[*].routingTable.routingProtocol[*].packetTrackingTimeout = 10s
*.host[*].routingTable.routingProtocol[*].maxTrackingHistory = 1000

// Reward parameters
*.host[*].routingTable.routingProtocol[*].successReward = 1.0
*.host[*].routingTable.routingProtocol[*].failureReward = -1.0
*.host[*].routingTable.routingProtocol[*].energyWeight = 0.1
*.host[*].routingTable.routingProtocol[*].delayWeight = 0.01

// Optional: Load pre-trained model
*.host[*].routingTable.routingProtocol[*].dqnModelFile = "model_training/trained_dqn_model.txt"
```

### Key Parameters

| Parameter | Description | Default |
|-----------|-------------|---------|
| `useOnlineRL` | Enable online reinforcement learning | false |
| `rlUpdateInterval` | Number of packets between RL updates | 10 |
| `dqnLearningRate` | Learning rate for DQN | 0.001 |
| `dqnEpsilon` | Initial exploration rate | 1.0 |
| `dqnGamma` | Discount factor | 0.95 |
| `packetTrackingTimeout` | Timeout for packet delivery tracking | 10s |
| `successReward` | Reward for successful packet delivery | 1.0 |
| `failureReward` | Penalty for failed packet delivery | -1.0 |

## Training and Usage

### 1. Initial Model Training

Train an initial DQN model using historical data:

```bash
cd model_training

# Install dependencies
pip install -r requirements_rl.txt

# Train DQN model (requires simulation data)
python train_dqn_model.py --input ../results/output.csv --output trained_dqn_model.txt

# Train traditional DNN model for comparison
python train_mlmorp_dnn.py --input ../results/output.csv --output trained_dnn_model.txt
```

### 2. Running Simulations

1. **Baseline (Traditional MLMORP)**:
   ```ned
   *.host[*].routingTable.routingProtocol[*].useDNNRouting = false
   *.host[*].routingTable.routingProtocol[*].useOnlineRL = false
   ```

2. **Offline DNN**:
   ```ned
   *.host[*].routingTable.routingProtocol[*].useDNNRouting = true
   *.host[*].routingTable.routingProtocol[*].useOnlineRL = false
   *.host[*].routingTable.routingProtocol[*].dnnModelFile = "model_training/trained_dnn_model.txt"
   ```

3. **Online Reinforcement Learning**:
   ```ned
   *.host[*].routingTable.routingProtocol[*].useDNNRouting = false
   *.host[*].routingTable.routingProtocol[*].useOnlineRL = true
   *.host[*].routingTable.routingProtocol[*].dqnModelFile = "model_training/trained_dqn_model.txt"
   ```

### 3. Analyzing Results

Generate learning curves and performance analysis:

```bash
# Plot learning curves
python plot_learning_curves.py --input ../results/output.csv --output-dir results/

# Generate summary report only
python plot_learning_curves.py --input ../results/output.csv --summary-only
```

## Implementation Details

### Feedback Mechanism

The packet tracker uses a simple timeout-based approach:

1. **Packet Forwarding**: When a packet is forwarded, it's added to the tracking system
2. **Delivery Confirmation**: When a packet reaches its destination, delivery is confirmed
3. **Timeout Handling**: Packets that don't arrive within the timeout are marked as failed
4. **Reward Calculation**: Rewards are calculated based on delivery success, energy usage, and delay

### Online Learning Process

1. **Action Selection**: DQN selects next-hop using epsilon-greedy policy
2. **Experience Storage**: State, action, reward, and next state are stored in replay buffer
3. **Periodic Updates**: Network weights are updated using mini-batch gradient descent
4. **Target Network**: Target network is updated periodically for stability
5. **Exploration Decay**: Epsilon decreases over time to reduce exploration

### Performance Considerations

- **Memory Usage**: Experience replay buffer and neural networks consume memory
- **Computational Overhead**: Online learning adds CPU overhead for training
- **Network Traffic**: No additional network traffic (uses existing packet tracking)
- **Convergence Time**: May require time to learn optimal policies

## Troubleshooting

### Common Issues

1. **No Learning Progress**:
   - Check if `useOnlineRL` is enabled
   - Verify packet tracking timeout is appropriate
   - Ensure sufficient network traffic for learning

2. **Poor Performance**:
   - Adjust learning rate and exploration parameters
   - Check reward function parameters
   - Verify state feature normalization

3. **Memory Issues**:
   - Reduce replay buffer size (`replayBufferSize` in DQNModel)
   - Decrease network architecture size
   - Limit tracking history size

### Debug Information

Enable detailed logging to monitor RL progress:

```ned
**.routingProtocol[*].useOnlineRL = true
```

Check simulation logs for:
- RL update messages
- Packet tracking statistics
- Exploration rate changes
- Training step information

## File Structure

```
src/inet/routing/mlmorp/
├── Mlmorp.h/.cc                    # Extended protocol implementation
├── DQNModel.h/.cc                  # Deep Q-Network implementation
├── PacketTracker.h/.cc             # Packet delivery tracking
├── SimpleDNNModel.h/.cc            # Original DNN model
├── Mlmorp.ned                      # Protocol configuration
├── README_RL.md                    # This documentation
└── model_training/
    ├── train_dqn_model.py          # DQN training script
    ├── train_mlmorp_dnn.py         # Original DNN training script
    ├── plot_learning_curves.py     # Learning curve analysis
    ├── requirements_rl.txt         # Python dependencies
    └── trained_models/             # Saved model files
```

## Research Applications

This RL extension enables research in:

- **Adaptive Routing**: How do networks adapt to changing conditions?
- **Multi-Objective Optimization**: Balancing delivery, energy, and delay
- **Distributed Learning**: Each node learns independently
- **Transfer Learning**: Pre-trained models for faster convergence
- **Algorithm Comparison**: RL vs traditional routing algorithms

## Future Enhancements

Potential improvements include:

- **Multi-Agent RL**: Coordinated learning between nodes
- **Advanced Algorithms**: Actor-Critic, PPO, or other RL algorithms
- **Dynamic Architecture**: Adaptive network architecture based on network size
- **Federated Learning**: Sharing learned knowledge between nodes
- **Real-time Adaptation**: Faster adaptation to network changes

## References

- Deep Q-Networks (DQN): Mnih et al., "Human-level control through deep reinforcement learning"
- MLMORP Protocol: Original MLMORP implementation
- Experience Replay: Lin, "Self-improving reactive agents based on reinforcement learning"

