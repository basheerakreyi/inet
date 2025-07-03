#!/usr/bin/env python3
"""
MLMORP DNN Training Script

This script trains a Deep Neural Network model for the MLMORP routing protocol
using data collected from OMNeT++ simulations.

Data Format (from results/output.csv):
- simTime: Simulation time
- treeId: Packet tree ID
- sourceAddress: Source IP address
- destAddress: Destination IP address
- srcMacAddress: Source MAC address
- destMacAddress: Destination MAC address
- nodeDegree: Number of neighbors
- residualEnergy: Residual energy capacity
- dataRate: Interface data rate
- signalPower: Signal power (if available)
- snir: Signal-to-Noise-and-Interference Ratio (if available)
- packetDelay: Packet delay

Features used for training:
- residualEnergy: Normalized residual energy
- dataRate: Normalized data rate
- signalPower: Normalized signal power
- nodeDegree: Normalized node degree
- snir: Normalized SNIR
- packetDelay: Normalized packet delay

Target/Label: Routing success/failure based on packet delivery
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from sklearn.preprocessing import StandardScaler
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score, classification_report, confusion_matrix
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
import os
import argparse
import json

class MLMORPDNNTrainer:
    def __init__(self, input_size=6, hidden_size=12, learning_rate=0.001):
        """
        Initialize the MLMORP DNN Trainer
        
        Args:
            input_size: Number of input features (default: 6)
            hidden_size: Number of neurons in hidden layer (default: 12)
            learning_rate: Learning rate for training (default: 0.001)
        """
        self.input_size = input_size
        self.hidden_size = hidden_size
        self.learning_rate = learning_rate
        self.scaler = StandardScaler()
        self.model = None
        self.feature_means = None
        self.feature_stds = None
        
    def load_data(self, csv_file):
        """
        Load and preprocess the collected data from CSV file
        
        Args:
            csv_file: Path to the output.csv file
            
        Returns:
            X: Feature matrix
            y: Target labels
        """
        print(f"Loading data from {csv_file}...")
        
        # Read CSV file
        try:
            df = pd.read_csv(csv_file, header=None)
        except FileNotFoundError:
            print(f"Error: File {csv_file} not found!")
            return None, None
        
        # Define column names based on the data collection format
        columns = [
            'simTime', 'treeId', 'sourceAddress', 'destAddress',
            'srcMacAddress', 'destMacAddress', 'nodeDegree', 'residualEnergy',
            'dataRate', 'signalPower', 'snir', 'packetDelay'
        ]
        
        # Assign column names
        df.columns = columns[:len(df.columns)]
        
        print(f"Loaded {len(df)} data points")
        print(f"Columns: {list(df.columns)}")
        
        # Handle missing columns (signalPower and snir might not be available)
        if 'signalPower' not in df.columns:
            df['signalPower'] = 1e-6  # Default value
        if 'snir' not in df.columns:
            df['snir'] = 10.0  # Default value

        # Extract features
        features = ['residualEnergy', 'dataRate', 'signalPower', 'nodeDegree', 'snir', 'packetDelay']
        
        # Check if all required features are available
        missing_features = [f for f in features if f not in df.columns]
        if missing_features:
            print(f"Warning: Missing features: {missing_features}")
            print("Using default values...")
            for feature in missing_features:
                if feature == 'signalPower':
                    df[feature] = 1e-6
                elif feature == 'snir':
                    df[feature] = 10.0
                else:
                    df[feature] = 0.0
        
        # Extract feature matrix
        X = df[features].values
        
        # Create target labels based on packet reception at destination (routing success/failure)
        # Target MAC address for destination
        target_dest_mac = '0A-AA-00-00-00-02'
    
        # Find all treeIds that successfully reached the destination
        successful_tree_ids = set(df[df['destMacAddress'] == target_dest_mac]['treeId'].unique())
    
        # Create labels: 1 if packet's treeId reached destination, 0 otherwise
        y = df['treeId'].apply(lambda tree_id: 1 if tree_id in successful_tree_ids else 0).values
        
        print(f"Feature matrix shape: {X.shape}")
        print(f"Target distribution: {np.bincount(y)}")

        # Save formatted data with labels to a new CSV file
        df_formatted = df.copy()
        df_formatted['label'] = y
    
        output_file = 'output_formatted.csv'
        df_formatted.to_csv(output_file, index=False)
        print(f"Formatted data saved to: {output_file}")
        
        return X, y
    
    def preprocess_data(self, X, y):
        """
        Preprocess the data for training
        
        Args:
            X: Feature matrix
            y: Target labels
            
        Returns:
            X_train, X_test, y_train, y_test: Split and scaled data
        """
        print("Preprocessing data...")
        
        # Split data into training and testing sets
        X_train, X_test, y_train, y_test = train_test_split(
            X, y, test_size=0.2, random_state=42, stratify=y
        )
        
        # Fit scaler on training data only
        X_train_scaled = self.scaler.fit_transform(X_train)
        X_test_scaled = self.scaler.transform(X_test)
        
        # Store normalization parameters for C++ model
        self.feature_means = self.scaler.mean_
        self.feature_stds = self.scaler.scale_
        
        print(f"Training set shape: {X_train_scaled.shape}")
        print(f"Testing set shape: {X_test_scaled.shape}")
        print(f"Feature means: {self.feature_means}")
        print(f"Feature stds: {self.feature_stds}")
        
        return X_train_scaled, X_test_scaled, y_train, y_test
    
    def build_model(self):
        """
        Build the neural network model
        
        Returns:
            model: Compiled Keras model
        """
        print("Building neural network model...")
        
        model = keras.Sequential([
            layers.Dense(self.hidden_size, activation='relu', 
                        input_shape=(self.input_size,),
                        kernel_initializer='glorot_uniform'),
            layers.Dense(1, activation='sigmoid',
                        kernel_initializer='glorot_uniform')
        ])
        
        model.compile(
            optimizer=keras.optimizers.Adam(learning_rate=self.learning_rate),
            loss='binary_crossentropy',
            metrics=['accuracy']
        )
        
        print(model.summary())
        self.model = model
        return model
    
    def train_model(self, X_train, y_train, X_test, y_test, epochs=100, batch_size=32):
        """
        Train the neural network model
        
        Args:
            X_train, y_train: Training data
            X_test, y_test: Testing data
            epochs: Number of training epochs
            batch_size: Batch size for training
            
        Returns:
            history: Training history
        """
        print("Training model...")
        
        # Add early stopping to prevent overfitting
        early_stopping = keras.callbacks.EarlyStopping(
            monitor='val_loss',
            patience=10,
            restore_best_weights=True
        )
        
        # Train the model
        history = self.model.fit(
            X_train, y_train,
            validation_data=(X_test, y_test),
            epochs=epochs,
            batch_size=batch_size,
            callbacks=[early_stopping],
            verbose=1
        )
        
        return history
    
    def evaluate_model(self, X_test, y_test):
        """
        Evaluate the trained model
        
        Args:
            X_test, y_test: Testing data
        """
        print("Evaluating model...")
        
        # Make predictions
        y_pred_proba = self.model.predict(X_test)
        y_pred = (y_pred_proba > 0.5).astype(int).flatten()
        
        # Calculate metrics
        accuracy = accuracy_score(y_test, y_pred)
        
        print(f"Test Accuracy: {accuracy:.4f}")
        print("\nClassification Report:")
        print(classification_report(y_test, y_pred))
        
        # Confusion matrix
        cm = confusion_matrix(y_test, y_pred)
        print("\nConfusion Matrix:")
        print(cm)
        
        return accuracy, y_pred_proba
    
    def save_model(self, output_file):
        """
        Save the trained model in the format expected by the C++ code
        
        Args:
            output_file: Path to save the model file
        """
        print(f"Saving model to {output_file}...")
        
        # Get model weights
        weights = self.model.get_weights()
        hidden_weights = weights[0]  # Input to hidden layer weights
        hidden_bias = weights[1]     # Hidden layer bias
        output_weights = weights[2]  # Hidden to output layer weights
        output_bias = weights[3]     # Output layer bias
        
        with open(output_file, 'w') as f:
            # Architecture
            f.write("Architecture:\n")
            f.write(f"{self.input_size} {self.hidden_size} 1 1\n")
            
            # Normalization parameters
            f.write("Normalization:\n")
            for i in range(self.input_size):
                f.write(f"{self.feature_means[i]} {self.feature_stds[i]}\n")
            
            # Hidden layer weights
            f.write("HiddenWeights:\n")
            for i in range(self.hidden_size):
                for j in range(self.input_size):
                    f.write(f"{hidden_weights[j, i]} ")
                f.write("\n")
            
            # Hidden layer bias
            f.write("HiddenBias:\n")
            for i in range(self.hidden_size):
                f.write(f"{hidden_bias[i]} ")
            f.write("\n")
            
            # Output layer weights
            f.write("OutputWeights:\n")
            for i in range(1):  # Only 1 output neuron
                for j in range(self.hidden_size):
                    f.write(f"{output_weights[j, i]} ")
                f.write("\n")
            
            # Output layer bias
            f.write("OutputBias:\n")
            f.write(f"{output_bias[0]}\n")
        
        print(f"Model saved successfully to {output_file}")
        
        # Also save as JSON for easier inspection
        json_file = output_file.replace('.txt', '.json')
        model_info = {
            'architecture': [self.input_size, self.hidden_size, 1],
            'feature_means': self.feature_means.tolist(),
            'feature_stds': self.feature_stds.tolist(),
            'hidden_weights': hidden_weights.tolist(),
            'hidden_bias': hidden_bias.tolist(),
            'output_weights': output_weights.tolist(),
            'output_bias': output_bias.tolist()
        }
        
        with open(json_file, 'w') as f:
            json.dump(model_info, f, indent=2)
        
        print(f"Model info also saved to {json_file}")
    
    def plot_training_history(self, history, save_plot=True):
        """
        Plot training history
        
        Args:
            history: Training history from model.fit()
            save_plot: Whether to save the plot
        """
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 4))
        
        # Plot accuracy
        ax1.plot(history.history['accuracy'], label='Training Accuracy')
        ax1.plot(history.history['val_accuracy'], label='Validation Accuracy')
        ax1.set_title('Model Accuracy')
        ax1.set_xlabel('Epoch')
        ax1.set_ylabel('Accuracy')
        ax1.legend()
        ax1.grid(True)
        
        # Plot loss
        ax2.plot(history.history['loss'], label='Training Loss')
        ax2.plot(history.history['val_loss'], label='Validation Loss')
        ax2.set_title('Model Loss')
        ax2.set_xlabel('Epoch')
        ax2.set_ylabel('Loss')
        ax2.legend()
        ax2.grid(True)
        
        plt.tight_layout()
        
        if save_plot:
            plt.savefig('training_history.png', dpi=300, bbox_inches='tight')
            print("Training history plot saved as training_history.png")
        
        plt.show()

def main():
    parser = argparse.ArgumentParser(description='Train MLMORP DNN Model')
    parser.add_argument('--input', '-i', default='../results/output.csv',
                       help='Input CSV file with collected data (default: ../results/output.csv)')
    parser.add_argument('--output', '-o', default='trained_model.txt',
                       help='Output model file (default: trained_model.txt)')
    parser.add_argument('--input-size', type=int, default=6,
                       help='Number of input features (default: 6)')
    parser.add_argument('--hidden-size', type=int, default=12,
                       help='Number of hidden neurons (default: 12)')
    parser.add_argument('--epochs', type=int, default=100,
                       help='Number of training epochs (default: 100)')
    parser.add_argument('--batch-size', type=int, default=32,
                       help='Batch size for training (default: 32)')
    parser.add_argument('--learning-rate', type=float, default=0.001,
                       help='Learning rate (default: 0.001)')
    parser.add_argument('--no-plot', action='store_true',
                       help='Disable plotting training history')
    
    args = parser.parse_args()
    
    # Check if input file exists
    if not os.path.exists(args.input):
        print(f"Error: Input file {args.input} not found!")
        print("Please run the OMNeT++ simulation first to generate the data file.")
        return
    
    # Create trainer
    trainer = MLMORPDNNTrainer(
        input_size=args.input_size,
        hidden_size=args.hidden_size,
        learning_rate=args.learning_rate
    )
    
    # Load and preprocess data
    X, y = trainer.load_data(args.input)
    if X is None:
        return
    
    X_train, X_test, y_train, y_test = trainer.preprocess_data(X, y)
    
    # Build and train model
    model = trainer.build_model()
    history = trainer.train_model(X_train, y_train, X_test, y_test, 
                                 epochs=args.epochs, batch_size=args.batch_size)
    
    # Evaluate model
    accuracy, predictions = trainer.evaluate_model(X_test, y_test)
    
    # Save model
    trainer.save_model(args.output)
    
    # Plot training history
    if not args.no_plot:
        trainer.plot_training_history(history)
    
    print("\nTraining completed successfully!")
    print(f"Model saved to: {args.output}")
    print(f"Test accuracy: {accuracy:.4f}")

if __name__ == "__main__":
    main() 