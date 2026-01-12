// Author: Basheer Al-Qassab
// Deep Q-Network implementation for online reinforcement learning

#include "inet/routing/rlmorp/DQNModel.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <numeric>
#include <cstring>

namespace inet {

DQNModel::DQNModel(int stateSize, int hiddenSize1, int hiddenSize2, 
                   int maxActions, double learningRate, double epsilon, double gamma)
    : stateSize(stateSize), hiddenSize1(hiddenSize1), hiddenSize2(hiddenSize2),
      maxActions(maxActions), learningRate(learningRate), epsilon(epsilon),
      epsilonMin(0.01), epsilonDecay(0.995), gamma(gamma),
      targetUpdateFreq(100), batchSize(32), replayBufferSize(10000),
      updateCounter(0), totalSteps(0), rng(42), uniformDist(0.0, 1.0)
{
    // Initialize state normalization with defaults
    stateMeans.resize(stateSize, 0.0);
    stateStds.resize(stateSize, 1.0);
    
    // Initialize neural networks
    initializeWeights();
    
    // Copy main network to target network
    updateTargetNetwork();
}

DQNModel::~DQNModel()
{
    // Vectors will be automatically cleaned up
}

void DQNModel::initializeWeights()
{
    // Initialize main network weights using Xavier initialization
    double scale1 = sqrt(2.0 / stateSize);
    std::normal_distribution<double> dist1(0.0, scale1);
    
    // First hidden layer
    mainWeights1.resize(hiddenSize1);
    for (int i = 0; i < hiddenSize1; i++) {
        mainWeights1[i].resize(stateSize);
        for (int j = 0; j < stateSize; j++) {
            mainWeights1[i][j] = dist1(rng);
        }
    }
    mainBias1.resize(hiddenSize1, 0.0);
    
    // Second hidden layer
    double scale2 = sqrt(2.0 / hiddenSize1);
    std::normal_distribution<double> dist2(0.0, scale2);
    
    mainWeights2.resize(hiddenSize2);
    for (int i = 0; i < hiddenSize2; i++) {
        mainWeights2[i].resize(hiddenSize1);
        for (int j = 0; j < hiddenSize1; j++) {
            mainWeights2[i][j] = dist2(rng);
        }
    }
    mainBias2.resize(hiddenSize2, 0.0);
    
    // Output layer
    double scale3 = sqrt(2.0 / hiddenSize2);
    std::normal_distribution<double> dist3(0.0, scale3);
    
    mainWeightsOut.resize(maxActions);
    for (int i = 0; i < maxActions; i++) {
        mainWeightsOut[i].resize(hiddenSize2);
        for (int j = 0; j < hiddenSize2; j++) {
            mainWeightsOut[i][j] = dist3(rng);
        }
    }
    mainBiasOut.resize(maxActions, 0.0);
    
    // Initialize target network with same size
    targetWeights1.resize(hiddenSize1);
    for (int i = 0; i < hiddenSize1; i++) {
        targetWeights1[i].resize(stateSize);
    }
    targetBias1.resize(hiddenSize1);
    
    targetWeights2.resize(hiddenSize2);
    for (int i = 0; i < hiddenSize2; i++) {
        targetWeights2[i].resize(hiddenSize1);
    }
    targetBias2.resize(hiddenSize2);
    
    targetWeightsOut.resize(maxActions);
    for (int i = 0; i < maxActions; i++) {
        targetWeightsOut[i].resize(hiddenSize2);
    }
    targetBiasOut.resize(maxActions);
}

void DQNModel::updateTargetNetwork()
{
    // Copy main network weights to target network
    for (int i = 0; i < hiddenSize1; i++) {
        for (int j = 0; j < stateSize; j++) {
            targetWeights1[i][j] = mainWeights1[i][j];
        }
        targetBias1[i] = mainBias1[i];
    }
    
    for (int i = 0; i < hiddenSize2; i++) {
        for (int j = 0; j < hiddenSize1; j++) {
            targetWeights2[i][j] = mainWeights2[i][j];
        }
        targetBias2[i] = mainBias2[i];
    }
    
    for (int i = 0; i < maxActions; i++) {
        for (int j = 0; j < hiddenSize2; j++) {
            targetWeightsOut[i][j] = mainWeightsOut[i][j];
        }
        targetBiasOut[i] = mainBiasOut[i];
    }
}

std::vector<double> DQNModel::normalizeState(const std::vector<double>& state) const
{
    std::vector<double> normalized(stateSize);
    for (int i = 0; i < stateSize && i < state.size(); i++) {
        if (stateStds[i] > 1e-10) {
            normalized[i] = (state[i] - stateMeans[i]) / stateStds[i];
        } else {
            normalized[i] = state[i] - stateMeans[i];
        }
    }
    return normalized;
}

std::vector<double> DQNModel::forward(const std::vector<double>& state, bool useTarget) const
{
    // Normalize input state
    std::vector<double> normalizedState = normalizeState(state);
    
    // Choose which network to use
    const auto& weights1 = useTarget ? targetWeights1 : mainWeights1;
    const auto& bias1 = useTarget ? targetBias1 : mainBias1;
    const auto& weights2 = useTarget ? targetWeights2 : mainWeights2;
    const auto& bias2 = useTarget ? targetBias2 : mainBias2;
    const auto& weightsOut = useTarget ? targetWeightsOut : mainWeightsOut;
    const auto& biasOut = useTarget ? targetBiasOut : mainBiasOut;
    
    // First hidden layer
    std::vector<double> hidden1(hiddenSize1);
    for (int i = 0; i < hiddenSize1; i++) {
        double sum = bias1[i];
        for (int j = 0; j < stateSize; j++) {
            sum += weights1[i][j] * normalizedState[j];
        }
        hidden1[i] = relu(sum);
    }
    
    // Second hidden layer
    std::vector<double> hidden2(hiddenSize2);
    for (int i = 0; i < hiddenSize2; i++) {
        double sum = bias2[i];
        for (int j = 0; j < hiddenSize1; j++) {
            sum += weights2[i][j] * hidden1[j];
        }
        hidden2[i] = relu(sum);
    }
    
    // Output layer (Q-values)
    std::vector<double> qValues(maxActions);
    for (int i = 0; i < maxActions; i++) {
        double sum = biasOut[i];
        for (int j = 0; j < hiddenSize2; j++) {
            sum += weightsOut[i][j] * hidden2[j];
        }
        qValues[i] = sum;  // Linear activation for Q-values
    }
    
    return qValues;
}

int DQNModel::selectAction(const std::vector<double>& state, 
                          const std::vector<int>& availableActions)
{
    if (availableActions.empty()) {
        return -1;  // No valid actions
    }
    
    // Epsilon-greedy action selection
    if (uniformDist(rng) < epsilon) {
        // Random action (exploration)
        std::uniform_int_distribution<int> actionDist(0, availableActions.size() - 1);
        return availableActions[actionDist(rng)];
    } else {
        // Greedy action (exploitation)
        std::vector<double> qValues = forward(state, false);
        
        // Find best action among available actions
        int bestAction = availableActions[0];
        double bestQValue = qValues[bestAction];
        
        for (int action : availableActions) {
            if (action < maxActions && qValues[action] > bestQValue) {
                bestQValue = qValues[action];
                bestAction = action;
            }
        }
        
        return bestAction;
    }
}

void DQNModel::storeExperience(const std::vector<double>& state, int action, 
                              double reward, const std::vector<double>& nextState, 
                              bool done, int availableActions)
{
    replayBuffer.emplace_back(state, action, reward, nextState, done, availableActions);
    
    // Remove old experiences if buffer is full
    if (replayBuffer.size() > replayBufferSize) {
        replayBuffer.pop_front();
    }
}

double DQNModel::trainStep()
{
    if (replayBuffer.size() < batchSize) {
        return 0.0;  // Not enough experiences for training
    }
    
    // Sample random batch from replay buffer
    std::vector<Experience> batch;
    std::uniform_int_distribution<int> indexDist(0, replayBuffer.size() - 1);
    
    for (int i = 0; i < batchSize; i++) {
        int index = indexDist(rng);
        batch.push_back(replayBuffer[index]);
    }
    
    // Prepare training data
    std::vector<std::vector<double>> states;
    std::vector<int> actions;
    std::vector<double> targets;
    
    for (const auto& exp : batch) {
        states.push_back(exp.state);
        actions.push_back(exp.action);
        
        // Calculate target Q-value
        double target;
        if (exp.done) {
            target = exp.reward;
        } else {
            // Q-learning: target = reward + gamma * max(Q(next_state))
            std::vector<double> nextQValues = forward(exp.nextState, true);  // Use target network
            int limit = exp.availableActions > 0 ? std::min(exp.availableActions, maxActions) : maxActions;
            double maxNextQ = *std::max_element(nextQValues.begin(), nextQValues.begin() + limit);
            target = exp.reward + gamma * maxNextQ;
        }
        targets.push_back(target);
    }
    
    // Perform backward pass and update weights
    backward(states, actions, targets);
    
    // Update counters
    updateCounter++;
    totalSteps++;
    
    // Update target network periodically
    if (updateCounter >= targetUpdateFreq) {
        updateTargetNetwork();
        updateCounter = 0;
    }
    
    // Decay exploration rate
    decayEpsilon();
    
    return 0.0;  // Could return actual loss if needed
}

void DQNModel::backward(const std::vector<std::vector<double>>& states,
                       const std::vector<int>& actions,
                       const std::vector<double>& targets)
{
    // Simplified gradient descent update
    // In practice, this would implement full backpropagation
    // For now, we'll do a simplified update focusing on the output layer
    
    for (int b = 0; b < batchSize && b < states.size(); b++) {
        // Forward pass to get current Q-values
        std::vector<double> qValues = forward(states[b], false);
        
        // Calculate error for the taken action
        if (actions[b] < maxActions) {
            double error = targets[b] - qValues[actions[b]];
            
            // Update output layer weights (simplified)
            // Get hidden2 activations
            std::vector<double> normalizedState = normalizeState(states[b]);
            
            // First hidden layer
            std::vector<double> hidden1(hiddenSize1);
            for (int i = 0; i < hiddenSize1; i++) {
                double sum = mainBias1[i];
                for (int j = 0; j < stateSize; j++) {
                    sum += mainWeights1[i][j] * normalizedState[j];
                }
                hidden1[i] = relu(sum);
            }
            
            // Second hidden layer
            std::vector<double> hidden2(hiddenSize2);
            for (int i = 0; i < hiddenSize2; i++) {
                double sum = mainBias2[i];
                for (int j = 0; j < hiddenSize1; j++) {
                    sum += mainWeights2[i][j] * hidden1[j];
                }
                hidden2[i] = relu(sum);
            }
            
            // Update output weights for the taken action
            for (int j = 0; j < hiddenSize2; j++) {
                mainWeightsOut[actions[b]][j] += learningRate * error * hidden2[j];
            }
            mainBiasOut[actions[b]] += learningRate * error;
        }
    }
}

void DQNModel::decayEpsilon()
{
    if (epsilon > epsilonMin) {
        epsilon *= epsilonDecay;
    }
}

void DQNModel::setNormalization(const std::vector<double>& means, 
                               const std::vector<double>& stds)
{
    if (means.size() == stateSize && stds.size() == stateSize) {
        stateMeans = means;
        stateStds = stds;
        
        // Ensure no division by zero
        for (int i = 0; i < stateSize; i++) {
            if (stateStds[i] < 1e-10) {
                stateStds[i] = 1.0;
            }
        }
    }
}

bool DQNModel::loadModel(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    
    // Load architecture (skip for now, assume compatible)
    std::getline(file, line);  // "Architecture:"
    std::getline(file, line);
    
    // Load normalization
    std::getline(file, line);  // "Normalization:"
    for (int i = 0; i < stateSize; i++) {
        std::getline(file, line);
        std::istringstream iss(line);
        iss >> stateMeans[i] >> stateStds[i];
    }
    
    // Load main network weights
    // Hidden1 weights
    std::getline(file, line);  // "Hidden1Weights:"
    for (int i = 0; i < hiddenSize1; i++) {
        std::getline(file, line);
        std::istringstream iss(line);
        for (int j = 0; j < stateSize; j++) {
            iss >> mainWeights1[i][j];
        }
    }
    
    // Hidden1 bias
    std::getline(file, line);  // "Hidden1Bias:"
    std::getline(file, line);
    std::istringstream issBias1(line);
    for (int i = 0; i < hiddenSize1; i++) {
        issBias1 >> mainBias1[i];
    }
    
    // Hidden2 weights
    std::getline(file, line);  // "Hidden2Weights:"
    for (int i = 0; i < hiddenSize2; i++) {
        std::getline(file, line);
        std::istringstream iss(line);
        for (int j = 0; j < hiddenSize1; j++) {
            iss >> mainWeights2[i][j];
        }
    }
    
    // Hidden2 bias
    std::getline(file, line);  // "Hidden2Bias:"
    std::getline(file, line);
    std::istringstream issBias2(line);
    for (int i = 0; i < hiddenSize2; i++) {
        issBias2 >> mainBias2[i];
    }
    
    // Output weights (adapt to maxActions)
    std::getline(file, line);  // "OutputWeights:"
    for (int i = 0; i < std::min(maxActions, 1); i++) {  // Original model has 1 output
        std::getline(file, line);
        std::istringstream iss(line);
        for (int j = 0; j < hiddenSize2; j++) {
            if (i < maxActions) {
                iss >> mainWeightsOut[i][j];
            }
        }
    }
    
    // Output bias
    std::getline(file, line);  // "OutputBias:"
    std::getline(file, line);
    std::istringstream issOutputBias(line);
    if (maxActions > 0) {
        issOutputBias >> mainBiasOut[0];
    }
    
    // Copy to target network
    updateTargetNetwork();
    
    file.close();
    return true;
}

bool DQNModel::saveModel(const std::string& filename) const
{
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    // Architecture
    file << "Architecture:\n";
    file << stateSize << " " << hiddenSize1 << " " << hiddenSize2 << " " << maxActions << " 0\n";
    
    // Normalization
    file << "Normalization:\n";
    for (int i = 0; i < stateSize; i++) {
        file << stateMeans[i] << " " << stateStds[i] << "\n";
    }
    
    // Hidden1 weights
    file << "Hidden1Weights:\n";
    for (int i = 0; i < hiddenSize1; i++) {
        for (int j = 0; j < stateSize; j++) {
            file << mainWeights1[i][j] << " ";
        }
        file << "\n";
    }
    
    // Hidden1 bias
    file << "Hidden1Bias:\n";
    for (int i = 0; i < hiddenSize1; i++) {
        file << mainBias1[i] << " ";
    }
    file << "\n";
    
    // Hidden2 weights
    file << "Hidden2Weights:\n";
    for (int i = 0; i < hiddenSize2; i++) {
        for (int j = 0; j < hiddenSize1; j++) {
            file << mainWeights2[i][j] << " ";
        }
        file << "\n";
    }
    
    // Hidden2 bias
    file << "Hidden2Bias:\n";
    for (int i = 0; i < hiddenSize2; i++) {
        file << mainBias2[i] << " ";
    }
    file << "\n";
    
    // Output weights
    file << "OutputWeights:\n";
    for (int i = 0; i < maxActions; i++) {
        for (int j = 0; j < hiddenSize2; j++) {
            file << mainWeightsOut[i][j] << " ";
        }
        file << "\n";
    }
    
    // Output bias
    file << "OutputBias:\n";
    for (int i = 0; i < maxActions; i++) {
        file << mainBiasOut[i] << " ";
    }
    file << "\n";
    
    file.close();
    return true;
}

std::string DQNModel::getModelInfo() const
{
    std::ostringstream oss;
    oss << "DQNModel: (" << stateSize << "-" << hiddenSize1 << "-" << hiddenSize2 
        << "-" << maxActions << "), epsilon=" << epsilon << ", steps=" << totalSteps;
    return oss.str();
}

} // namespace inet

