// Author: Basheer Al-Qassab
// Deep Q-Network implementation for online reinforcement learning

#include "DQNModel.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <numeric>
#include <cstring>
#include <limits>

namespace inet {

DQNModel::DQNModel(int stateSize, int hiddenSize1, int hiddenSize2, 
                   double learningRate, double epsilon, double gamma)
    : stateSize(stateSize), hiddenSize1(hiddenSize1), hiddenSize2(hiddenSize2),
      learningRate(learningRate), epsilon(epsilon),
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
    
    // Output layer (single output)
    double scale3 = sqrt(2.0 / hiddenSize2);
    std::normal_distribution<double> dist3(0.0, scale3);
    
    mainWeightsOut.resize(hiddenSize2);
    for (int j = 0; j < hiddenSize2; j++) {
        mainWeightsOut[j] = dist3(rng);
    }
    mainBiasOut = 0.0;
    
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
    
    targetWeightsOut.resize(hiddenSize2);
    targetBiasOut = 0.0;
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
    
    // Copy output layer weights (single output)
    for (int j = 0; j < hiddenSize2; j++) {
        targetWeightsOut[j] = mainWeightsOut[j];
    }
    targetBiasOut = mainBiasOut;
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

double DQNModel::forward(const std::vector<double>& state, bool useTarget) const
{
    // Normalize input state
    std::vector<double> normalizedState = normalizeState(state);
    
    // Choose which network to use
    const auto& weights1 = useTarget ? targetWeights1 : mainWeights1;
    const auto& bias1 = useTarget ? targetBias1 : mainBias1;
    const auto& weights2 = useTarget ? targetWeights2 : mainWeights2;
    const auto& bias2 = useTarget ? targetBias2 : mainBias2;
    const auto& weightsOut = useTarget ? targetWeightsOut : mainWeightsOut;
    const double& biasOut = useTarget ? targetBiasOut : mainBiasOut;
    
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
    
    // Output layer (single Q-value)
    double qValue = biasOut;
    for (int j = 0; j < hiddenSize2; j++) {
        qValue += weightsOut[j] * hidden2[j];
    }
    
    return qValue;  // Linear activation for Q-value
}

double DQNModel::scoreNeighbor(const std::vector<double>& neighborFeatures) const
{
    // Score a neighbor by computing Q(x(i,j)) using the DQN
    return forward(neighborFeatures, false);
}

double DQNModel::computeMaxQ(const std::vector<std::vector<double>>& neighborFeaturesList, bool useTarget) const
{
    if (neighborFeaturesList.empty()) {
        return 0.0;  // No neighbors available
    }
    
    // Compute Q-value for each neighbor and return the maximum
    double maxQ = -std::numeric_limits<double>::max();
    for (const auto& neighborFeatures : neighborFeaturesList) {
        double qValue = forward(neighborFeatures, useTarget);
        if (qValue > maxQ) {
            maxQ = qValue;
        }
    }
    
    return maxQ;
}

void DQNModel::storeExperience(const std::vector<double>& neighborFeatures, 
                              double reward, const std::vector<std::vector<double>>& nextNeighborFeaturesList, 
                              bool done, int treeId, const L3Address& nodeAddress, bool rewardPending)
{
    if (rewardPending && treeId != 0 && !nodeAddress.isUnspecified()) {
        // Store with tracking information for later update
        replayBuffer.emplace_back(neighborFeatures, reward, nextNeighborFeaturesList, done, 
                                 treeId, nodeAddress, rewardPending);
    } else {
        // Store without tracking (immediate final reward)
        replayBuffer.emplace_back(neighborFeatures, reward, nextNeighborFeaturesList, done);
    }
    
    // Remove old experiences if buffer is full
    if (replayBuffer.size() > replayBufferSize) {
        replayBuffer.pop_front();
    }
}

void DQNModel::updatePendingExperienceReward(int treeId, const L3Address& nodeAddress, 
                                             double finalReward, bool isTerminal)
{
    // Find matching pending experience and update it with final reward
    for (auto& exp : replayBuffer) {
        if (exp.rewardPending && 
            exp.packetTreeId == treeId && 
            exp.nodeAddress == nodeAddress) {
            exp.reward = finalReward;      // Update with final reward
            exp.done = isTerminal;         // Update terminal status
            exp.rewardPending = false;     // Mark as updated
            return;  // Found and updated
        }
    }
    // If not found, it may have been removed from buffer or already updated
}

double DQNModel::trainStep()
{
    if (replayBuffer.size() < batchSize) {
        return 0.0;  // Not enough experiences for training
    }
    
    // Count non-pending experiences
    size_t nonPendingCount = 0;
    for (const auto& exp : replayBuffer) {
        if (!exp.rewardPending) {
            nonPendingCount++;
        }
    }
    
    // Need at least batchSize non-pending experiences
    if (nonPendingCount < batchSize) {
        return 0.0;  // Not enough finalized experiences for training
    }
    
    // Build list of valid (non-pending) experience indices for efficient sampling
    std::vector<int> validIndices;
    for (size_t i = 0; i < replayBuffer.size(); i++) {
        if (!replayBuffer[i].rewardPending) {
            validIndices.push_back(i);
        }
    }
    
    // Sample random batch from non-pending experiences only
    std::vector<Experience> batch;
    std::uniform_int_distribution<int> indexDist(0, validIndices.size() - 1);
    
    for (int i = 0; i < batchSize; i++) {
        int validIndex = indexDist(rng);
        batch.push_back(replayBuffer[validIndices[validIndex]]);
    }
    
    // Prepare training data
    std::vector<std::vector<double>> neighborFeatures;
    std::vector<double> targets;
    double totalSqError = 0.0;
    
    for (const auto& exp : batch) {
        neighborFeatures.push_back(exp.neighborFeatures);
        
        // Calculate target Q-value
        double target;
        if (exp.done) {
            // Terminal transition: target = reward only
            target = exp.reward;
        } else {
            // Non-terminal transition: Q-learning bootstrapping
            // target = reward + gamma * max_j Q(x'(i,j))
            double maxNextQ = computeMaxQ(exp.nextNeighborFeaturesList, true);  // Use target network
            target = exp.reward + gamma * maxNextQ;
        }
        targets.push_back(target);
    }

    // Compute TD loss before updating weights (based on current Q estimates).
    // We return MSE over the sampled batch.
    const int effectiveBatch = std::min<int>(batchSize, static_cast<int>(neighborFeatures.size()));
    for (int i = 0; i < effectiveBatch; i++) {
        double qValue = forward(neighborFeatures[i], false);
        double err = targets[i] - qValue;
        totalSqError += err * err;
    }
    
    // Perform backward pass and update weights
    backward(neighborFeatures, targets);
    
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
    
    return totalSqError / std::max(1, effectiveBatch);
}

void DQNModel::backward(const std::vector<std::vector<double>>& neighborFeatures,
                       const std::vector<double>& targets)
{
    // Backprop through the full 5 -> hidden1 -> hidden2 -> 1 network.
    // We use a simplified squared-error TD loss:
    //   L = 0.5 * (target - q)^2
    // which yields gradients proportional to (target - q).
    
    for (int b = 0; b < batchSize && b < neighborFeatures.size(); b++) {
        // Recompute activations for this sample (we don't store activations).
        // layer1: hidden1 = relu(sum1)
        // layer2: hidden2 = relu(sum2)
        // output:  q = biasOut + sum_j wOut[j] * hidden2[j]  (linear)

        std::vector<double> normalizedState = normalizeState(neighborFeatures[b]);

        std::vector<double> sum1(hiddenSize1);
        std::vector<double> hidden1(hiddenSize1);
        for (int i = 0; i < hiddenSize1; i++) {
            double acc = mainBias1[i];
            for (int j = 0; j < stateSize; j++) {
                acc += mainWeights1[i][j] * normalizedState[j];
            }
            sum1[i] = acc;
            hidden1[i] = relu(acc);
        }

        std::vector<double> sum2(hiddenSize2);
        std::vector<double> hidden2(hiddenSize2);
        for (int i = 0; i < hiddenSize2; i++) {
            double acc = mainBias2[i];
            for (int j = 0; j < hiddenSize1; j++) {
                acc += mainWeights2[i][j] * hidden1[j];
            }
            sum2[i] = acc;
            hidden2[i] = relu(acc);
        }

        // Current prediction q(x) with the main network (before updates).
        double qValue = mainBiasOut;
        for (int j = 0; j < hiddenSize2; j++) {
            qValue += mainWeightsOut[j] * hidden2[j];
        }

        // TD/supervised target error term used by the original simplified update.
        double delta_out = targets[b] - qValue;  // proportional to dL/dq with 0.5 scaling

        // Backprop through output -> hidden2 -> hidden1.
        // delta2[i] is dL/d(sum2[i])
        std::vector<double> delta2(hiddenSize2, 0.0);
        for (int i = 0; i < hiddenSize2; i++) {
            delta2[i] = delta_out * mainWeightsOut[i] * reluDerivative(sum2[i]);
        }

        // delta1[j] is dL/d(sum1[j])
        std::vector<double> delta1(hiddenSize1, 0.0);
        for (int j = 0; j < hiddenSize1; j++) {
            double acc = 0.0;
            for (int i = 0; i < hiddenSize2; i++) {
                acc += delta2[i] * mainWeights2[i][j];
            }
            delta1[j] = acc * reluDerivative(sum1[j]);
        }

        // Update output layer (linear)
        for (int j = 0; j < hiddenSize2; j++) {
            mainWeightsOut[j] += learningRate * delta_out * hidden2[j];
        }
        mainBiasOut += learningRate * delta_out;

        // Update hidden2 layer (w2: hidden1 -> hidden2)
        for (int i = 0; i < hiddenSize2; i++) {
            for (int j = 0; j < hiddenSize1; j++) {
                mainWeights2[i][j] += learningRate * delta2[i] * hidden1[j];
            }
            mainBias2[i] += learningRate * delta2[i];
        }

        // Update hidden1 layer (w1: input -> hidden1)
        for (int i = 0; i < hiddenSize1; i++) {
            for (int j = 0; j < stateSize; j++) {
                mainWeights1[i][j] += learningRate * delta1[i] * normalizedState[j];
            }
            mainBias1[i] += learningRate * delta1[i];
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
    
    // Output weights (single output)
    std::getline(file, line);  // "OutputWeights:"
    std::getline(file, line);
    std::istringstream iss(line);
    for (int j = 0; j < hiddenSize2; j++) {
        iss >> mainWeightsOut[j];
    }
    
    // Output bias
    std::getline(file, line);  // "OutputBias:"
    std::getline(file, line);
    std::istringstream issOutputBias(line);
    issOutputBias >> mainBiasOut;
    
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
    file << stateSize << " " << hiddenSize1 << " " << hiddenSize2 << " 1 0\n";  // Single output
    
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
    
    // Output weights (single output)
    file << "OutputWeights:\n";
    for (int j = 0; j < hiddenSize2; j++) {
        file << mainWeightsOut[j] << " ";
    }
    file << "\n";
    
    // Output bias
    file << "OutputBias:\n";
    file << mainBiasOut << "\n";
    
    file.close();
    return true;
}

std::string DQNModel::getModelInfo() const
{
    std::ostringstream oss;
    oss << "DQNModel: (" << stateSize << "-" << hiddenSize1 << "-" << hiddenSize2 
        << "-1), epsilon=" << epsilon << ", steps=" << totalSteps;  // Single output
    return oss.str();
}

} // namespace inet

