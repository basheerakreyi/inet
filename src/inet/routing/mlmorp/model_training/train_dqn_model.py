#!/usr/bin/env python3
"""
DQN Training Script for MLMORP Online Reinforcement Learning

This script trains a Deep Q-Network (DQN) model for the MLMORP routing protocol
using data collected from OMNeT++ simulations. The trained model can be used
as initialization for online learning in the simulation.

The model architecture matches the C++ DQNModel implementation:
- Input: 5 features (neighbor feature vector)
- Hidden1: 64 neurons (default)
- Hidden2: 32 neurons (default)
- Output: Single Q-value (not multiple actions)

Features:
- Train DQN model on historical routing data
- Export weights in OMNeT++ compatible format
- Support for experience replay and target networks
- Configurable network architecture and hyperparameters
- Comprehensive training visualizations
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from sklearn.preprocessing import StandardScaler
from sklearn.model_selection import train_test_split
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
import os
import argparse
import json
from collections import deque
import random
import math

class DQNTrainer:
    def __init__(self, state_size=5, hidden_size1=64, hidden_size2=32, 
                 learning_rate=0.001, gamma=0.95, 
                 epsilon=1.0, epsilon_min=0.01, epsilon_decay=0.995,
                 success_reward=1.0, failure_reward=-1.0, 
                 energy_weight=0.1, delay_weight=0.01):
        """
        Initialize the DQN Trainer
        
        Args:
            state_size: Number of state features (default: 5)
            hidden_size1: First hidden layer size (default: 64)
            hidden_size2: Second hidden layer size (default: 32)
            learning_rate: Learning rate for training (default: 0.001)
            gamma: Discount factor (default: 0.95)
            epsilon: Initial exploration rate (default: 1.0)
            epsilon_min: Minimum exploration rate (default: 0.01)
            epsilon_decay: Exploration decay rate (default: 0.995)
            success_reward: Reward for successful delivery (default: 1.0)
            failure_reward: Reward for failed delivery (default: -1.0)
            energy_weight: Weight for energy efficiency (default: 0.1)
            delay_weight: Weight for delay penalty (default: 0.01)
        """
        self.state_size = state_size
        self.hidden_size1 = hidden_size1
        self.hidden_size2 = hidden_size2
        self.learning_rate = learning_rate
        self.gamma = gamma
        self.epsilon = epsilon
        self.epsilon_min = epsilon_min
        self.epsilon_decay = epsilon_decay
        
        # Reward parameters (matching C++ PacketTracker defaults)
        self.success_reward = success_reward
        self.failure_reward = failure_reward
        self.energy_weight = energy_weight
        self.delay_weight = delay_weight
        
        self.scaler = StandardScaler()
        self.main_model = None
        self.target_model = None
        self.feature_means = None
        self.feature_stds = None
        
        # Training parameters (matching C++ DQNModel defaults)
        self.batch_size = 32
        self.target_update_freq = 100
        self.memory = deque(maxlen=10000)
        
    def build_model(self):
        """
        Build the DQN model with single output Q-value.
        Uses Xavier initialization matching C++ implementation.
        """
        # Xavier initialization: scale = sqrt(2.0 / input_size)
        scale1 = math.sqrt(2.0 / self.state_size)
        scale2 = math.sqrt(2.0 / self.hidden_size1)
        scale3 = math.sqrt(2.0 / self.hidden_size2)
        
        model = keras.Sequential([
            layers.Dense(self.hidden_size1, activation='relu', 
                        input_shape=(self.state_size,),
                        kernel_initializer=keras.initializers.RandomNormal(mean=0.0, stddev=scale1),
                        bias_initializer='zeros'),
            layers.Dense(self.hidden_size2, activation='relu',
                        kernel_initializer=keras.initializers.RandomNormal(mean=0.0, stddev=scale2),
                        bias_initializer='zeros'),
            layers.Dense(1, activation='linear',  # Single Q-value output
                        kernel_initializer=keras.initializers.RandomNormal(mean=0.0, stddev=scale3),
                        bias_initializer='zeros')
        ])
        
        model.compile(
            optimizer=keras.optimizers.Adam(learning_rate=self.learning_rate),
            loss='mse',  # Mean squared error for Q-learning
            metrics=['mae']
        )
        
        return model
    
    def calculate_reward(self, delivered, energy_before, energy_after, 
                        forward_time, delivery_time):
        """
        Calculate reward matching C++ PacketTracker::calculateReward
        
        Args:
            delivered: Whether packet was successfully delivered
            energy_before: Energy before forwarding
            energy_after: Energy after delivery/timeout
            forward_time: Time when packet was forwarded
            delivery_time: Time when packet was delivered (or timed out)
            
        Returns:
            Calculated reward value
        """
        # Base reward
        base_reward = self.success_reward if delivered else self.failure_reward
        
        # Energy efficiency component (negative because using energy is bad)
        energy_used = energy_before - energy_after
        energy_efficiency_reward = -self.energy_weight * energy_used
        
        # Delay component (only for successful deliveries)
        delay_reward = 0.0
        if delivered and delivery_time > 0:
            delay = delivery_time - forward_time
            delay_reward = -self.delay_weight * delay
        
        return base_reward + energy_efficiency_reward + delay_reward
    
    def load_data(self, csv_file):
        """
        Load and preprocess the collected data from CSV file
        
        Args:
            csv_file: Path to the output.csv file
            
        Returns:
            experiences: List of (state, reward, next_state, done) tuples
        """
        print(f"Loading data from {csv_file}...")
        
        try:
            df = pd.read_csv(csv_file, header=None)
        except FileNotFoundError:
            print(f"Error: File {csv_file} not found!")
            return None
        
        # Define column names based on the data collection format
        columns = [
            'simTime', 'treeId', 'sourceAddress', 'destAddress',
            'srcMacAddress', 'destMacAddress', 'nodeDegree', 'residualEnergy',
            'dataRate', 'signalPower', 'buffPktNo'
        ]
        
        # Assign column names
        df.columns = columns[:len(df.columns)]
        
        print(f"Loaded {len(df)} data points")
        
        # Handle missing columns
        if 'signalPower' not in df.columns:
            df['signalPower'] = 1e-6
        if 'buffPktNo' not in df.columns:
            df['buffPktNo'] = 10.0
        
        # Extract features for states (matching buildNeighborFeatureVector)
        state_features = ['residualEnergy', 'dataRate', 'signalPower', 'nodeDegree', 'buffPktNo']
        
        # Check if all required features are available
        missing_features = [f for f in state_features if f not in df.columns]
        if missing_features:
            print(f"Warning: Missing features: {missing_features}")
            for feature in missing_features:
                if feature == 'signalPower':
                    df[feature] = 1e-6
                elif feature == 'buffPktNo':
                    df[feature] = 10.0
                else:
                    df[feature] = 0.0
        
        # Sort by time to maintain temporal order
        df = df.sort_values('simTime').reset_index(drop=True)
        
        # Create experiences from the data
        experiences = []
        
        # Group by treeId to create episodes
        grouped = df.groupby('treeId')
        
        for tree_id, group in grouped:
            group = group.sort_values('simTime').reset_index(drop=True)
            
            if len(group) < 2:
                continue  # Need at least 2 hops for an experience
            
            # Determine if this packet was successfully delivered
            # Check if any row has the destination MAC (assuming destination is known)
            # For simplicity, we'll use a heuristic: if the last hop exists, consider it delivered
            # In practice, you'd track actual delivery confirmations
            delivered = len(group) > 0  # Simplified: assume delivered if we have data
            
            # Get initial energy (first row)
            initial_energy = group.iloc[0]['residualEnergy']
            
            for i in range(len(group) - 1):
                # Current state (neighbor features at forwarding time)
                current_state = group.iloc[i][state_features].values
                
                # Next state (neighbor features at next hop)
                next_state = group.iloc[i + 1][state_features].values
                
                # Energy before and after
                energy_before = group.iloc[i]['residualEnergy']
                energy_after = group.iloc[i + 1]['residualEnergy']
                
                # Time information
                forward_time = group.iloc[i]['simTime']
                delivery_time = group.iloc[i + 1]['simTime']
                
                # Determine if this is terminal
                # Terminal if: (1) last hop, or (2) packet failed
                is_last_hop = (i == len(group) - 2)
                is_terminal = is_last_hop
                
                # For intermediate hops, we don't know final outcome yet
                # Use intermediate reward (small penalty per hop)
                if is_last_hop:
                    # Final hop: use actual reward calculation
                    reward = self.calculate_reward(
                        delivered, energy_before, energy_after,
                        forward_time, delivery_time
                    )
                else:
                    # Intermediate hop: small penalty (matching C++ intermediateReward = -0.01)
                    reward = -0.01
                
                experiences.append({
                    'state': current_state,
                    'reward': reward,
                    'next_state': next_state,
                    'done': is_terminal
                })
        
        print(f"Created {len(experiences)} experiences")
        
        # Print reward distribution
        rewards = [exp['reward'] for exp in experiences]
        print(f"Reward distribution: mean={np.mean(rewards):.3f}, std={np.std(rewards):.3f}, "
              f"min={np.min(rewards):.3f}, max={np.max(rewards):.3f}")
        
        return experiences
    
    def preprocess_data(self, experiences):
        """
        Preprocess experiences and prepare for training
        
        Args:
            experiences: List of experience dictionaries
            
        Returns:
            states, rewards, next_states, dones: Preprocessed arrays
        """
        print("Preprocessing data...")
        
        # Extract arrays
        states = np.array([exp['state'] for exp in experiences])
        rewards = np.array([exp['reward'] for exp in experiences])
        next_states = np.array([exp['next_state'] for exp in experiences])
        dones = np.array([exp['done'] for exp in experiences])
        
        # Combine states and next_states for normalization
        all_states = np.vstack([states, next_states])
        
        # Fit scaler on all states
        normalized_all = self.scaler.fit_transform(all_states)
        
        # Split back
        normalized_states = normalized_all[:len(states)]
        normalized_next_states = normalized_all[len(states):]
        
        # Store normalization parameters
        self.feature_means = self.scaler.mean_
        self.feature_stds = self.scaler.scale_
        
        print(f"Feature means: {self.feature_means}")
        print(f"Feature stds: {self.feature_stds}")
        
        return normalized_states, rewards, normalized_next_states, dones
    
    def train_model(self, states, rewards, next_states, dones, epochs=1000):
        """
        Train the DQN model using experience replay
        
        Args:
            states: Normalized state features
            rewards: Reward values
            next_states: Normalized next state features
            dones: Episode termination flags
            epochs: Number of training epochs
            
        Returns:
            losses, q_values, rewards: Training metrics
        """
        print("Building and training DQN model...")
        
        # Build models
        self.main_model = self.build_model()
        self.target_model = self.build_model()
        
        # Copy weights to target model
        self.target_model.set_weights(self.main_model.get_weights())
        
        print(self.main_model.summary())
        
        # Create experiences for training
        for i in range(len(states)):
            self.memory.append((states[i], rewards[i], next_states[i], dones[i]))
        
        # Training loop
        losses = []
        avg_q_values = []
        avg_rewards = []
        update_count = 0
        best_loss = float('inf')
        patience = 200
        patience_counter = 0
        
        for epoch in range(epochs):
            if len(self.memory) < self.batch_size:
                continue
            
            # Sample batch from memory
            batch = random.sample(self.memory, self.batch_size)
            batch_states = np.array([e[0] for e in batch])
            batch_rewards = np.array([e[1] for e in batch])
            batch_next_states = np.array([e[2] for e in batch])
            batch_dones = np.array([e[3] for e in batch])
            
            # Compute target Q-values using target network
            target_q_values = self.target_model.predict(batch_next_states, verbose=0)
            
            # Compute targets (matching C++ trainStep logic)
            targets = []
            for i in range(self.batch_size):
                if batch_dones[i]:
                    # Terminal transition: target = reward only
                    target = batch_rewards[i]
                else:
                    # Non-terminal transition: Q-learning bootstrapping
                    # target = reward + gamma * max_j Q(x'(i,j))
                    # Since we have single Q-value per neighbor, we use the Q-value directly
                    max_next_q = target_q_values[i][0]
                    target = batch_rewards[i] + self.gamma * max_next_q
                targets.append(target)
            
            targets = np.array(targets).reshape(-1, 1)
            
            # Get current Q-values
            current_q_values = self.main_model.predict(batch_states, verbose=0)
            
            # Track average Q-value
            avg_max_q = np.mean(current_q_values)
            avg_q_values.append(avg_max_q)
            
            # Track average reward in batch
            avg_reward = np.mean(batch_rewards)
            avg_rewards.append(avg_reward)
            
            # Train main model
            history = self.main_model.fit(batch_states, targets, 
                                        epochs=1, verbose=0, batch_size=self.batch_size)
            current_loss = history.history['loss'][0]
            losses.append(current_loss)
            
            # Early stopping check
            if current_loss > 10.0:
                print(f"Warning: Loss exploded to {current_loss:.4f} at epoch {epoch}. Stopping training.")
                break
            
            # Track best loss for early stopping
            if current_loss < best_loss:
                best_loss = current_loss
                patience_counter = 0
            else:
                patience_counter += 1
                if patience_counter >= patience and epoch > 500:
                    print(f"Early stopping at epoch {epoch} (loss: {current_loss:.4f})")
                    break
            
            # Update target network periodically
            update_count += 1
            if update_count % self.target_update_freq == 0:
                self.target_model.set_weights(self.main_model.get_weights())
            
            # Decay epsilon
            if self.epsilon > self.epsilon_min:
                self.epsilon *= self.epsilon_decay
            
            if epoch % 100 == 0:
                avg_loss = np.mean(losses[-100:]) if losses else 0
                avg_q = np.mean(avg_q_values[-100:]) if avg_q_values else 0
                avg_r = np.mean(avg_rewards[-100:]) if avg_rewards else 0
                print(f"Epoch {epoch}, Loss: {avg_loss:.4f}, Avg Q: {avg_q:.4f}, "
                      f"Avg Reward: {avg_r:.4f}, Epsilon: {self.epsilon:.3f}")
        
        return losses, avg_q_values, avg_rewards
    
    def save_model(self, output_file):
        """
        Save the trained model in OMNeT++ compatible format
        Matches the format expected by DQNModel::loadModel
        """
        print(f"Saving DQN model to {output_file}...")
        
        # Get model weights
        weights = self.main_model.get_weights()
        hidden1_weights = weights[0]  # Input to first hidden layer (state_size x hidden_size1)
        hidden1_bias = weights[1]     # First hidden layer bias (hidden_size1,)
        hidden2_weights = weights[2]  # First to second hidden layer (hidden_size1 x hidden_size2)
        hidden2_bias = weights[3]     # Second hidden layer bias (hidden_size2,)
        output_weights = weights[4]   # Second hidden to output layer (hidden_size2 x 1)
        output_bias = weights[5]     # Output layer bias (1,)
        
        with open(output_file, 'w') as f:
            # Architecture (matching C++ format: stateSize hiddenSize1 hiddenSize2 1 0)
            f.write("Architecture:\n")
            f.write(f"{self.state_size} {self.hidden_size1} {self.hidden_size2} 1 0\n")
            
            # Normalization parameters
            f.write("Normalization:\n")
            for i in range(self.state_size):
                f.write(f"{self.feature_means[i]} {self.feature_stds[i]}\n")
            
            # Hidden1 weights (matching C++ format: each row is one neuron)
            f.write("Hidden1Weights:\n")
            for i in range(self.hidden_size1):
                for j in range(self.state_size):
                    f.write(f"{hidden1_weights[j, i]} ")
                f.write("\n")
            
            # Hidden1 bias
            f.write("Hidden1Bias:\n")
            for i in range(self.hidden_size1):
                f.write(f"{hidden1_bias[i]} ")
            f.write("\n")
            
            # Hidden2 weights
            f.write("Hidden2Weights:\n")
            for i in range(self.hidden_size2):
                for j in range(self.hidden_size1):
                    f.write(f"{hidden2_weights[j, i]} ")
                f.write("\n")
            
            # Hidden2 bias
            f.write("Hidden2Bias:\n")
            for i in range(self.hidden_size2):
                f.write(f"{hidden2_bias[i]} ")
            f.write("\n")
            
            # Output weights (single output)
            f.write("OutputWeights:\n")
            for j in range(self.hidden_size2):
                f.write(f"{output_weights[j, 0]} ")
            f.write("\n")
            
            # Output bias
            f.write("OutputBias:\n")
            f.write(f"{output_bias[0]}\n")
        
        print(f"DQN model saved successfully to {output_file}")
        
        # Also save as JSON for inspection
        json_file = output_file.replace('.txt', '.json')
        model_info = {
            'architecture': [self.state_size, self.hidden_size1, self.hidden_size2, 1],
            'feature_means': self.feature_means.tolist(),
            'feature_stds': self.feature_stds.tolist(),
            'hidden1_weights': hidden1_weights.tolist(),
            'hidden1_bias': hidden1_bias.tolist(),
            'hidden2_weights': hidden2_weights.tolist(),
            'hidden2_bias': hidden2_bias.tolist(),
            'output_weights': output_weights.flatten().tolist(),
            'output_bias': float(output_bias[0])
        }
        
        with open(json_file, 'w') as f:
            json.dump(model_info, f, indent=2)
        
        print(f"Model info also saved to {json_file}")
    
    def plot_training_history(self, losses, save_plot=True, output_dir='.'):
        """Plot training loss history"""
        plt.figure(figsize=(12, 6))
        
        # Plot raw losses
        plt.plot(losses, alpha=0.3, color='blue', linewidth=0.5, label='Raw Loss')
        
        # Plot smoothed losses (moving average)
        if len(losses) > 100:
            window_size = min(100, len(losses) // 10)
            smoothed = pd.Series(losses).rolling(window=window_size).mean()
            plt.plot(smoothed, color='blue', linewidth=2, 
                    label=f'Moving Average (window={window_size})')
        
        plt.title('DQN Training Loss', fontsize=14, fontweight='bold')
        plt.xlabel('Training Step', fontsize=12)
        plt.ylabel('Loss (MSE)', fontsize=12)
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.tight_layout()
        
        if save_plot:
            plot_path = os.path.join(output_dir, 'dqn_training_loss.png')
            plt.savefig(plot_path, dpi=300, bbox_inches='tight')
            print(f"Training loss plot saved as {plot_path}")
        
        plt.close()
    
    def plot_q_values(self, q_values, save_plot=True, output_dir='.'):
        """Plot Q-values over training"""
        plt.figure(figsize=(12, 6))
        
        # Plot raw Q-values
        plt.plot(q_values, alpha=0.3, label='Raw Q-values', color='red', linewidth=0.5)
        
        # Plot smoothed Q-values (moving average)
        if len(q_values) > 100:
            window_size = min(100, len(q_values) // 10)
            smoothed = pd.Series(q_values).rolling(window=window_size).mean()
            plt.plot(smoothed, label=f'Moving Average (window={window_size})', 
                    color='darkred', linewidth=2)
        
        plt.title('Average Q-Values During Training', fontsize=14, fontweight='bold')
        plt.xlabel('Training Step', fontsize=12)
        plt.ylabel('Average Q-Value', fontsize=12)
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.tight_layout()
        
        if save_plot:
            plot_path = os.path.join(output_dir, 'dqn_q_values.png')
            plt.savefig(plot_path, dpi=300, bbox_inches='tight')
            print(f"Q-values plot saved as {plot_path}")
        
        plt.close()
    
    def plot_rewards(self, rewards, save_plot=True, output_dir='.'):
        """Plot average rewards over training"""
        plt.figure(figsize=(12, 6))
        
        # Plot raw rewards
        plt.plot(rewards, alpha=0.3, label='Raw Rewards', color='green', linewidth=0.5)
        
        # Plot smoothed rewards (moving average)
        if len(rewards) > 100:
            window_size = min(100, len(rewards) // 10)
            smoothed = pd.Series(rewards).rolling(window=window_size).mean()
            plt.plot(smoothed, label=f'Moving Average (window={window_size})', 
                    color='darkgreen', linewidth=2)
        
        plt.title('Average Rewards During Training', fontsize=14, fontweight='bold')
        plt.xlabel('Training Step', fontsize=12)
        plt.ylabel('Average Reward', fontsize=12)
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.tight_layout()
        
        if save_plot:
            plot_path = os.path.join(output_dir, 'dqn_rewards.png')
            plt.savefig(plot_path, dpi=300, bbox_inches='tight')
            print(f"Rewards plot saved as {plot_path}")
        
        plt.close()
    
    def plot_comprehensive_training(self, losses, q_values, rewards, 
                                   save_plot=True, output_dir='.'):
        """Plot comprehensive training metrics in subplots"""
        fig, axes = plt.subplots(3, 1, figsize=(14, 10))
        
        window_size = min(100, len(losses) // 10) if len(losses) > 100 else 10
        
        # Plot 1: Loss
        axes[0].plot(losses, alpha=0.3, color='blue', linewidth=0.5)
        if len(losses) > window_size:
            smoothed_loss = pd.Series(losses).rolling(window=window_size).mean()
            axes[0].plot(smoothed_loss, color='blue', linewidth=2, 
                        label=f'Moving Avg (window={window_size})')
        axes[0].set_title('Training Loss', fontsize=12, fontweight='bold')
        axes[0].set_xlabel('Training Step', fontsize=10)
        axes[0].set_ylabel('Loss (MSE)', fontsize=10)
        axes[0].legend()
        axes[0].grid(True, alpha=0.3)
        
        # Plot 2: Q-values
        axes[1].plot(q_values, alpha=0.3, color='red', linewidth=0.5)
        if len(q_values) > window_size:
            smoothed_q = pd.Series(q_values).rolling(window=window_size).mean()
            axes[1].plot(smoothed_q, color='red', linewidth=2, 
                        label=f'Moving Avg (window={window_size})')
        axes[1].set_title('Average Q-Values', fontsize=12, fontweight='bold')
        axes[1].set_xlabel('Training Step', fontsize=10)
        axes[1].set_ylabel('Average Q-Value', fontsize=10)
        axes[1].legend()
        axes[1].grid(True, alpha=0.3)
        
        # Plot 3: Rewards
        axes[2].plot(rewards, alpha=0.3, color='green', linewidth=0.5)
        if len(rewards) > window_size:
            smoothed_reward = pd.Series(rewards).rolling(window=window_size).mean()
            axes[2].plot(smoothed_reward, color='green', linewidth=2, 
                       label=f'Moving Avg (window={window_size})')
        axes[2].set_title('Average Rewards', fontsize=12, fontweight='bold')
        axes[2].set_xlabel('Training Step', fontsize=10)
        axes[2].set_ylabel('Average Reward', fontsize=10)
        axes[2].legend()
        axes[2].grid(True, alpha=0.3)
        
        plt.tight_layout()
        
        if save_plot:
            plot_path = os.path.join(output_dir, 'dqn_comprehensive_training.png')
            plt.savefig(plot_path, dpi=300, bbox_inches='tight')
            print(f"Comprehensive training plot saved as {plot_path}")
        
        plt.close()
    
    def plot_convergence_analysis(self, losses, q_values, rewards, 
                                  save_plot=True, output_dir='.'):
        """Plot convergence analysis with statistics"""
        fig, axes = plt.subplots(2, 2, figsize=(14, 10))
        
        window_size = min(100, len(losses) // 10) if len(losses) > 100 else 10
        
        # Plot 1: Loss convergence (log scale for better visualization)
        axes[0, 0].plot(losses, alpha=0.3, color='blue', linewidth=0.5)
        if len(losses) > window_size:
            smoothed_loss = pd.Series(losses).rolling(window=window_size).mean()
            axes[0, 0].plot(smoothed_loss, color='blue', linewidth=2)
        axes[0, 0].set_yscale('log')
        axes[0, 0].set_title('Loss Convergence (Log Scale)', fontsize=12, fontweight='bold')
        axes[0, 0].set_xlabel('Training Step')
        axes[0, 0].set_ylabel('Loss (MSE)')
        axes[0, 0].grid(True, alpha=0.3)
        
        # Plot 2: Q-value progression
        axes[0, 1].plot(q_values, alpha=0.3, color='red', linewidth=0.5)
        if len(q_values) > window_size:
            smoothed_q = pd.Series(q_values).rolling(window=window_size).mean()
            axes[0, 1].plot(smoothed_q, color='red', linewidth=2)
        axes[0, 1].set_title('Q-Value Progression', fontsize=12, fontweight='bold')
        axes[0, 1].set_xlabel('Training Step')
        axes[0, 1].set_ylabel('Average Q-Value')
        axes[0, 1].grid(True, alpha=0.3)
        
        # Plot 3: Reward progression
        axes[1, 0].plot(rewards, alpha=0.3, color='green', linewidth=0.5)
        if len(rewards) > window_size:
            smoothed_reward = pd.Series(rewards).rolling(window=window_size).mean()
            axes[1, 0].plot(smoothed_reward, color='green', linewidth=2)
        axes[1, 0].set_title('Reward Progression', fontsize=12, fontweight='bold')
        axes[1, 0].set_xlabel('Training Step')
        axes[1, 0].set_ylabel('Average Reward')
        axes[1, 0].grid(True, alpha=0.3)
        
        # Plot 4: Statistics summary
        axes[1, 1].axis('off')
        stats_text = f"""
Training Statistics Summary

Loss:
  Final: {losses[-1]:.4f}
  Mean: {np.mean(losses):.4f}
  Min: {np.min(losses):.4f}
  Max: {np.max(losses):.4f}

Q-Values:
  Final: {q_values[-1]:.4f}
  Mean: {np.mean(q_values):.4f}
  Min: {np.min(q_values):.4f}
  Max: {np.max(q_values):.4f}

Rewards:
  Final: {rewards[-1]:.4f}
  Mean: {np.mean(rewards):.4f}
  Min: {np.min(rewards):.4f}
  Max: {np.max(rewards):.4f}

Training Steps: {len(losses)}
        """
        axes[1, 1].text(0.1, 0.5, stats_text, fontsize=10, 
                        verticalalignment='center', family='monospace')
        
        plt.tight_layout()
        
        if save_plot:
            plot_path = os.path.join(output_dir, 'dqn_convergence_analysis.png')
            plt.savefig(plot_path, dpi=300, bbox_inches='tight')
            print(f"Convergence analysis plot saved as {plot_path}")
        
        plt.close()

def main():
    parser = argparse.ArgumentParser(description='Train DQN Model for MLMORP')
    parser.add_argument('--input', '-i', default='../results/output.csv',
                       help='Input CSV file with collected data')
    parser.add_argument('--output', '-o', default='trained_dqn_model.txt',
                       help='Output DQN model file')
    parser.add_argument('--state-size', type=int, default=5,
                       help='Number of state features (default: 5)')
    parser.add_argument('--hidden-size1', type=int, default=64,
                       help='First hidden layer size (default: 64)')
    parser.add_argument('--hidden-size2', type=int, default=32,
                       help='Second hidden layer size (default: 32)')
    parser.add_argument('--epochs', type=int, default=1000,
                       help='Number of training epochs (default: 1000)')
    parser.add_argument('--learning-rate', type=float, default=0.001,
                       help='Learning rate (default: 0.001)')
    parser.add_argument('--gamma', type=float, default=0.95,
                       help='Discount factor (default: 0.95)')
    parser.add_argument('--success-reward', type=float, default=1.0,
                       help='Reward for successful delivery (default: 1.0)')
    parser.add_argument('--failure-reward', type=float, default=-1.0,
                       help='Reward for failed delivery (default: -1.0)')
    parser.add_argument('--energy-weight', type=float, default=0.1,
                       help='Weight for energy efficiency (default: 0.1)')
    parser.add_argument('--delay-weight', type=float, default=0.01,
                       help='Weight for delay penalty (default: 0.01)')
    parser.add_argument('--no-plot', action='store_true',
                       help='Disable plotting training history')
    parser.add_argument('--output-dir', default='.',
                       help='Directory for output files (default: current directory)')
    
    args = parser.parse_args()
    
    # Check if input file exists
    if not os.path.exists(args.input):
        print(f"Error: Input file {args.input} not found!")
        print("Please run the OMNeT++ simulation first to generate the data file.")
        return
    
    # Create output directory if it doesn't exist
    os.makedirs(args.output_dir, exist_ok=True)
    
    # Create trainer
    trainer = DQNTrainer(
        state_size=args.state_size,
        hidden_size1=args.hidden_size1,
        hidden_size2=args.hidden_size2,
        learning_rate=args.learning_rate,
        gamma=args.gamma,
        success_reward=args.success_reward,
        failure_reward=args.failure_reward,
        energy_weight=args.energy_weight,
        delay_weight=args.delay_weight
    )
    
    # Load and preprocess data
    experiences = trainer.load_data(args.input)
    if experiences is None:
        return
    
    states, rewards, next_states, dones = trainer.preprocess_data(experiences)
    
    # Train model
    losses, avg_q_values, avg_rewards = trainer.train_model(
        states, rewards, next_states, dones, args.epochs)
    
    # Save model
    output_path = os.path.join(args.output_dir, args.output)
    trainer.save_model(output_path)
    
    # Plot training history
    if not args.no_plot:
        trainer.plot_training_history(losses, output_dir=args.output_dir)
        trainer.plot_q_values(avg_q_values, output_dir=args.output_dir)
        trainer.plot_rewards(avg_rewards, output_dir=args.output_dir)
        trainer.plot_comprehensive_training(losses, avg_q_values, avg_rewards, 
                                           output_dir=args.output_dir)
        trainer.plot_convergence_analysis(losses, avg_q_values, avg_rewards, 
                                         output_dir=args.output_dir)
    
    print(f"\nDQN training completed successfully!")
    print(f"Model saved to: {output_path}")
    print(f"Training statistics:")
    print(f"  Final loss: {losses[-1]:.4f}")
    print(f"  Final Q-value: {avg_q_values[-1]:.4f}")
    print(f"  Final reward: {avg_rewards[-1]:.4f}")

if __name__ == "__main__":
    main()
