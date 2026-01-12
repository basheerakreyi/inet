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
    std::vector<double> state;      // Current state features
    int action;                     // Action taken (neighbor index)
    double reward;                  // Reward received
    std::vector<double> nextState;  // Next state features
    bool done;                      // Episode termination flag
    int availableActions;           // Number of valid actions when action was chosen
    
    // Default constructor
    Experience() : action(0), reward(0.0), done(false), availableActions(0) {}
    
    // Parameterized constructor
    Experience(const std::vector<double>& s, int a, double r, 
               const std::vector<double>& ns, bool d, int avail)
        : state(s), action(a), reward(r), nextState(ns), done(d), availableActions(avail) {}
};

/**
 * Deep Q-Network Model for online reinforcement learning in RLMORP.
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
    int stateSize;              // Number of state features
    int hiddenSize1;            // First hidden layer size
    int hiddenSize2;            // Second hidden layer size
    int maxActions;             // Maximum number of actions (neighbors)
    
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
    std::vector<std::vector<double>> mainWeightsOut;    // Main network output weights
    std::vector<double> mainBiasOut;                    // Main network output bias
    
    std::vector<std::vector<double>> targetWeights1;    // Target network hidden1 weights
    std::vector<double> targetBias1;                    // Target network hidden1 bias
    std::vector<std::vector<double>> targetWeights2;    // Target network hidden2 weights
    std::vector<double> targetBias2;                    // Target network hidden2 bias
    std::vector<std::vector<double>> targetWeightsOut;  // Target network output weights
    std::vector<double> targetBiasOut;                  // Target network output bias
    
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
     * @param state Input state vector
     * @param useTarget Whether to use target network (default: false)
     * @return Q-values for all actions
     */
    std::vector<double> forward(const std::vector<double>& state, bool useTarget = false) const;
    
    /**
     * Backward pass and weight update
     * @param states Batch of states
     * @param actions Batch of actions
     * @param targets Batch of target Q-values
     */
    void backward(const std::vector<std::vector<double>>& states,
                  const std::vector<int>& actions,
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
     * @param stateSize Number of state features
     * @param hiddenSize1 First hidden layer size
     * @param hiddenSize2 Second hidden layer size
     * @param maxActions Maximum number of actions
     * @param learningRate Learning rate (default: 0.001)
     * @param epsilon Initial exploration rate (default: 1.0)
     * @param gamma Discount factor (default: 0.95)
     */
    DQNModel(int stateSize = 5, int hiddenSize1 = 64, int hiddenSize2 = 32, 
             int maxActions = 10, double learningRate = 0.001, 
             double epsilon = 1.0, double gamma = 0.95);
    
    /**
     * Destructor
     */
    ~DQNModel();
    
    /**
     * Select action using epsilon-greedy policy
     * @param state Current state features
     * @param availableActions List of available action indices
     * @return Selected action index
     */
    int selectAction(const std::vector<double>& state, 
                     const std::vector<int>& availableActions);
    
    /**
     * Store experience in replay buffer
     * @param state Current state
     * @param action Action taken
     * @param reward Reward received
     * @param nextState Next state
     * @param done Episode termination flag
     * @param availableActions Number of valid actions when the decision was made
     */
    void storeExperience(const std::vector<double>& state, int action, 
                        double reward, const std::vector<double>& nextState, 
                        bool done, int availableActions);
    
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
     * Get maximum number of actions
     * @return Maximum number of actions supported by the model
     */
    int getMaxActions() const { return maxActions; }
    
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

