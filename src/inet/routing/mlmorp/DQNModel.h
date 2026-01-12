// Author: Basheer Al-Qassab
// Deep Q-Network implementation for online reinforcement learning

#ifndef __INET_DQNMODEL_H
#define __INET_DQNMODEL_H

#include <vector>
#include <deque>
#include <random>
#include <map>
#include <memory>
#include <cmath>
#include <algorithm>

#include "inet/common/INETDefs.h"
#include "inet/networklayer/common/L3Address.h"
#include "inet/common/geometry/common/Coord.h"
#include "inet/common/INETMath.h"
#include "omnetpp.h"

namespace inet {

/**
 * Experience tuple for DQN replay buffer
 */
struct Experience {
    std::vector<double> neighborFeatures;      // Feature vector x(i,j) for chosen neighbor j
    double reward;                             // Reward received
    std::vector<std::vector<double>> nextNeighborFeaturesList;  // List of next feature vectors x'(i,j) for all neighbors j at update time (for bootstrapping)
    bool done;                                         // Episode termination flag (true for destination reached or timeout)
    
    // Fields for tracking and updating delayed rewards
    int packetTreeId;                         // Packet identifier for matching experiences to outcomes
    L3Address nodeAddress;                    // Node address that made this forwarding decision
    bool rewardPending;                       // true if reward is temporary and will be updated when outcome is known
    
    // Default constructor
    Experience() : reward(0.0), done(false), packetTreeId(0), rewardPending(false) {}
    
    // Parameterized constructor (for immediate storage with final reward)
    Experience(const std::vector<double>& nf, double r, 
               const std::vector<std::vector<double>>& nnfList, bool d)
        : neighborFeatures(nf), reward(r), nextNeighborFeaturesList(nnfList), done(d),
          packetTreeId(0), rewardPending(false) {}
    
    // Parameterized constructor (for storage with pending reward update)
    Experience(const std::vector<double>& nf, double r, 
               const std::vector<std::vector<double>>& nnfList, bool d,
               int treeId, const L3Address& nodeAddr, bool pending)
        : neighborFeatures(nf), reward(r), nextNeighborFeaturesList(nnfList), done(d),
          packetTreeId(treeId), nodeAddress(nodeAddr), rewardPending(pending) {}
};

/**
 * Deep Q-Network Model for online reinforcement learning in MLMORP.
 * 
 * This class implements a DQN agent that learns routing decisions online:
 * - State: network features (energy, signal strength, node degree, etc.)
 * - Action: select next-hop neighbor
 * - Reward: based on delivery success, energy efficiency, and delay
 * 
 * Features:
 * - Experience replay buffer
 * - Target network for stable training
 * - Epsilon-greedy exploration
 * - Online learning with periodic updates
 */
class INET_API DQNModel
{
private:
    // Network architecture
    int stateSize;              // Number of state features (neighbor feature vector size: 5)
    int hiddenSize1;            // First hidden layer size
    int hiddenSize2;            // Second hidden layer size
    
    // DQN parameters
    double learningRate;        // Learning rate for updates
    double epsilon;             // Exploration rate
    double epsilonMin;          // Minimum exploration rate
    double epsilonDecay;        // Exploration decay rate
    double gamma;               // Discount factor
    int targetUpdateFreq;       // Target network update frequency
    int batchSize;              // Mini-batch size for training
    int replayBufferSize;       // Maximum replay buffer size
    
    // Neural networks (main and target)
    std::vector<std::vector<double>> mainWeights1;      // Main network hidden1 weights
    std::vector<double> mainBias1;                      // Main network hidden1 bias
    std::vector<std::vector<double>> mainWeights2;      // Main network hidden2 weights
    std::vector<double> mainBias2;                      // Main network hidden2 bias
    std::vector<double> mainWeightsOut;                 // Main network output weights (single output)
    double mainBiasOut;                                 // Main network output bias
    
    std::vector<std::vector<double>> targetWeights1;    // Target network hidden1 weights
    std::vector<double> targetBias1;                    // Target network hidden1 bias
    std::vector<std::vector<double>> targetWeights2;    // Target network hidden2 weights
    std::vector<double> targetBias2;                    // Target network hidden2 bias
    std::vector<double> targetWeightsOut;               // Target network output weights (single output)
    double targetBiasOut;                               // Target network output bias
    
    // Experience replay
    std::deque<Experience> replayBuffer;
    
    // Training counters
    int updateCounter;          // Count of updates for target network sync
    int totalSteps;             // Total learning steps
    
    // Random number generation
    std::mt19937 rng;
    std::uniform_real_distribution<double> uniformDist;
    
    // Feature normalization
    std::vector<double> stateMeans;
    std::vector<double> stateStds;
    
    /**
     * Initialize neural network weights using Xavier initialization
     */
    void initializeWeights();
    
    /**
     * Copy main network weights to target network
     */
    void updateTargetNetwork();
    
    /**
     * Forward pass through neural network
     * @param state Input state vector (neighbor feature vector x(i,j))
     * @param useTarget Whether to use target network (default: false)
     * @return Single Q-value Q(x(i,j)) representing quality of forwarding to neighbor j
     */
    double forward(const std::vector<double>& state, bool useTarget = false) const;
    
    /**
     * Backward pass and weight update
     * @param states Batch of neighbor feature vectors
     * @param targets Batch of target Q-values
     */
    void backward(const std::vector<std::vector<double>>& states,
                  const std::vector<double>& targets);
    
    /**
     * ReLU activation function
     */
    double relu(double x) const { return std::max(0.0, x); }
    
    /**
     * ReLU derivative
     */
    double reluDerivative(double x) const { return x > 0 ? 1.0 : 0.0; }
    
    /**
     * Normalize state features
     * @param state Raw state features
     * @return Normalized state features
     */
    std::vector<double> normalizeState(const std::vector<double>& state) const;

public:
    /**
     * Constructor
     * @param stateSize Number of state features (neighbor feature vector size, default: 5)
     * @param hiddenSize1 First hidden layer size
     * @param hiddenSize2 Second hidden layer size
     * @param learningRate Learning rate (default: 0.001)
     * @param epsilon Initial exploration rate (default: 1.0)
     * @param gamma Discount factor (default: 0.95)
     */
    DQNModel(int stateSize = 5, int hiddenSize1 = 64, int hiddenSize2 = 32, 
             double learningRate = 0.001, double epsilon = 1.0, double gamma = 0.95);
    
    /**
     * Destructor
     */
    ~DQNModel();
    
    /**
     * Score a neighbor using the DQN model
     * @param neighborFeatures Feature vector x(i,j) for neighbor j
     * @return Q-value Q(x(i,j)) representing quality of forwarding to neighbor j
     */
    double scoreNeighbor(const std::vector<double>& neighborFeatures) const;
    
    /**
     * Compute maximum Q-value over all neighbor feature vectors (for bootstrapping)
     * @param neighborFeaturesList List of neighbor feature vectors x(i,j) for all neighbors j
     * @param useTarget Whether to use target network (default: true for bootstrapping)
     * @return Maximum Q-value max_j Q(x(i,j))
     */
    double computeMaxQ(const std::vector<std::vector<double>>& neighborFeaturesList, bool useTarget = true) const;
    
    /**
     * Store experience in replay buffer
     * @param neighborFeatures Feature vector x(i,j) for chosen neighbor j
     * @param reward Reward received (or temporary reward if rewardPending=true)
     * @param nextNeighborFeaturesList List of next feature vectors x'(i,j) for all neighbors j at update time (for bootstrapping)
     * @param done Episode termination flag (true for terminal transitions: destination reached or timeout)
     * @param treeId Optional packet identifier for tracking (0 if not tracking)
     * @param nodeAddress Optional node address that made decision (for updating pending rewards)
     * @param rewardPending true if reward is temporary and will be updated when outcome is known
     */
    void storeExperience(const std::vector<double>& neighborFeatures, 
                        double reward, const std::vector<std::vector<double>>& nextNeighborFeaturesList, 
                        bool done, int treeId = 0, const L3Address& nodeAddress = L3Address(), bool rewardPending = false);
    
    /**
     * Update pending experience reward when final outcome is known
     * @param treeId Packet identifier
     * @param nodeAddress Node address that made the forwarding decision
     * @param finalReward Final reward based on delivery outcome
     * @param isTerminal Whether this is a terminal transition
     */
    void updatePendingExperienceReward(int treeId, const L3Address& nodeAddress, 
                                       double finalReward, bool isTerminal);
    
    /**
     * Train the network using experience replay
     * @return Training loss (for monitoring)
     */
    double trainStep();
    
    /**
     * Update exploration rate
     */
    void decayEpsilon();
    
    /**
     * Get current exploration rate
     * @return Current epsilon value
     */
    double getEpsilon() const { return epsilon; }
    
    /**
     * Get total training steps
     * @return Number of training steps performed
     */
    int getTotalSteps() const { return totalSteps; }
    
    /**
     * Get batch size used for training
     * @return Batch size for mini-batch training
     */
    int getBatchSize() const { return batchSize; }
    
    
    /**
     * Set state normalization parameters
     * @param means Mean values for each state feature
     * @param stds Standard deviation values for each state feature
     */
    void setNormalization(const std::vector<double>& means, 
                         const std::vector<double>& stds);
    
    /**
     * Load pre-trained model weights
     * @param filename Path to model file
     * @return True if successful
     */
    bool loadModel(const std::string& filename);
    
    /**
     * Save model weights
     * @param filename Path to save model
     * @return True if successful
     */
    bool saveModel(const std::string& filename) const;
    
    /**
     * Get model information string
     * @return Model architecture and parameter info
     */
    std::string getModelInfo() const;
    
    /**
     * Reset exploration rate (for new episodes)
     */
    void resetEpsilon(double newEpsilon = 1.0) { epsilon = newEpsilon; }
    
    /**
     * Clear replay buffer
     */
    void clearReplayBuffer() { replayBuffer.clear(); }
    
    /**
     * Get replay buffer size
     * @return Current number of experiences in buffer
     */
    size_t getReplayBufferSize() const { return replayBuffer.size(); }
};

} // namespace inet

#endif // __INET_DQNMODEL_H
