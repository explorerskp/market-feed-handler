import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys
import os


INPUT_FILE = "latency_metrics.csv"

def analyze_collision():
    if not os.path.exists(INPUT_FILE):
        print(f"Error: {INPUT_FILE} not found.")
        return

    print(f"Loading {INPUT_FILE}...")
    
    
    
    try:
        df = pd.read_csv(INPUT_FILE, names=["Timestamp", "Price", "Latency_ns"], header=None, low_memory=False)
    except Exception as e:
        print(f"Failed to read CSV: {e}")
        return

    
    print("Cleaning data...")
    original_count = len(df)
    
    
    df['Timestamp'] = pd.to_numeric(df['Timestamp'], errors='coerce')
    df['Price'] = pd.to_numeric(df['Price'], errors='coerce')
    df['Latency_ns'] = pd.to_numeric(df['Latency_ns'], errors='coerce')
    
    
    df.dropna(inplace=True)
    
    cleaned_count = len(df)
    dropped = original_count - cleaned_count
    if dropped > 0:
        print(f"Removed {dropped} non-numeric rows (headers/garbage).")

    if cleaned_count == 0:
        print("Error: No valid data found in file.")
        return

    
    avg_lat = df['Latency_ns'].mean()
    p50_lat = df['Latency_ns'].median()
    p99_lat = df['Latency_ns'].quantile(0.99)
    max_lat = df['Latency_ns'].max()
    
    
    df['ts_diff'] = df['Timestamp'].diff()
    stale_count = df[df['ts_diff'] < 0].shape[0]
    stale_pct = (stale_count / cleaned_count) * 100.0

    
    
    df['Symbol_ID'] = df['Timestamp'] % 5
    symbol_counts = df['Symbol_ID'].value_counts().sort_index()

    
    print("\n" + "="*40)
    print(f"      COLLISION-TREE ARCHITECTURE REPORT      ")
    print("="*40)
    print(f"Valid Updates:    {cleaned_count:,}")
    print(f"Staleness:        {stale_pct:.6f}%")
    print("-" * 40)
    print(f"Avg Latency:      {avg_lat:,.0f} ns")
    print(f"p99 Latency:      {p99_lat:,.0f} ns")
    print(f"Max Latency:      {max_lat:,.0f} ns")
    print("-" * 40)
    print("Load Balancing (Updates per Asset):")
    for sym, c in symbol_counts.items():
        print(f"  Asset {sym}: {c:,}")
    print("="*40 + "\n")

    
    plt.style.use('ggplot')
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle(f'Collision-Tree Architecture Deep Dive\n(Staleness: {stale_pct:.4f}%)', fontsize=16)

    
    axes[0,0].hist(df['Latency_ns'], bins=50, color='blue', alpha=0.7, log=True)
    axes[0,0].set_title("Latency Distribution (Log Scale)")
    axes[0,0].set_xlabel("Latency (ns)")
    axes[0,0].set_ylabel("Frequency")

    
    
    step = max(1, cleaned_count // 2000)
    subset = df.iloc[::step]
    
    axes[0,1].plot(subset.index, subset['Latency_ns'], color='green', alpha=0.6, linewidth=1)
    axes[0,1].set_title("Latency Stability Over Time")
    axes[0,1].set_xlabel("Sequence")
    axes[0,1].set_ylabel("Latency (ns)")
    
    
    axes[1,0].bar(symbol_counts.index, symbol_counts.values, color='purple', alpha=0.7)
    axes[1,0].set_title("Load Balancing (Assets)")
    axes[1,0].set_xlabel("Symbol ID")
    axes[1,0].set_xticks(range(5))

    
    axes[1,1].plot(subset.index, subset['Price'], color='orange', alpha=0.6)
    axes[1,1].set_title("Price Feed Reconstruction")
    axes[1,1].set_xlabel("Sequence")

    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.savefig("collision_deep_dive.png")
    print("Saved graph: collision_deep_dive.png")

if __name__ == "__main__":
    analyze_collision()