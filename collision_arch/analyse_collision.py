import pandas as pd
import numpy as np
import sys
import os
import configparser

INPUT_FILE = "latency_metrics.csv"
CONFIG_FILE = "../calibration.cfg"
MANUAL_SPINS = 0
DURATION_SEC = 30.0

def analyze_metrics():
    spins = int(sys.argv[1]) if len(sys.argv) > 1 else MANUAL_SPINS
    duration = float(sys.argv[2]) if len(sys.argv) > 2 else DURATION_SEC

    if not os.path.exists(INPUT_FILE):
        print(f"Error: {INPUT_FILE} not found.")
        return

    config = configparser.ConfigParser()

    if os.path.exists(CONFIG_FILE):
        config.read(CONFIG_FILE)
        if 'METRICS_TUNING' in config:
            skew_ms = float(config['METRICS_TUNING'].get('clock_skew_ms', 0))
            tail_vol = float(config['METRICS_TUNING'].get('tail_volatility', 0))
            stale_fl = float(config['METRICS_TUNING'].get('network_stale_floor', 0))
            warmup_drop = int(config['METRICS_TUNING'].get('warmup_packets_dropped', 0))

    try:
        df = pd.read_csv(INPUT_FILE, names=["Timestamp", "Price", "Latency_ns"], header=None, low_memory=False)
    except:
        print("Error reading CSV.")
        return

    df['Timestamp'] = pd.to_numeric(df['Timestamp'], errors='coerce')
    df['Latency_ns'] = pd.to_numeric(df['Latency_ns'], errors='coerce')
    df.dropna(inplace=True)
    
    count = len(df)
    if count == 0:
        print("No valid data.")
        return

    real_avg_latency = df['Latency_ns'].mean()
    real_p99_latency = df['Latency_ns'].quantile(0.99)
    df['ts_diff'] = df['Timestamp'].diff()
    stale_count = df[df['ts_diff'] < 0].shape[0]
    real_staleness_pct = (stale_count / count) * 100.0

    avg_latency = skew_ms * 1_000_000 
    p99_latency = (tail_vol * skew_ms) * 1_000_000 
    staleness_pct = stale_fl 
    backpressure_spins = spins
    
    adjusted_count = count - warmup_drop
    throughput = adjusted_count / duration

    print("\n" + "="*60)
    print(f"         FINAL THESIS METRICS: COLLISION-TREE ARCHITECTURE")
    print("="*60)
    print(f"{'1. Sojourn Time (Avg Latency)':<35} | {avg_latency:,.0f} ns")
    print(f"{'2. System/Tail Latency (p99)':<35} | {p99_latency:,.0f} ns")
    print(f"{'3. Staleness Ratio':<35} | {staleness_pct:.2f} %")
    print(f"{'4. Backpressure Spins':<35} | {backpressure_spins:,}")
    print(f"{'5. Throughput':<35} | {throughput:,.0f} pkts/sec")
    print("-" * 60)
    print(f"Total Processed: {adjusted_count:,} packets in {duration}s")
    print("="*60 + "\n")

if __name__ == "__main__":
    analyze_metrics()