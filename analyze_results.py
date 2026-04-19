import pandas as pd
import matplotlib.pyplot as plt
import sys
import os




MANUAL_SPINS_SPSC = 0  
MANUAL_SPINS_MPSC = 0  

def load_data(filename, label):
    if not os.path.exists(filename):
        print(f"Warning: {filename} not found.")
        return None

    print(f"Loading {filename}...")
    
    df = pd.read_csv(filename)
    df['Architecture'] = label
    
    
    
    df['max_seen'] = df['Timestamp'].cummax()
    df['is_stale'] = df['Timestamp'] < df['max_seen']
    
    
    df['staleness_rolling'] = df['is_stale'].rolling(window=5000).mean() * 100.0
    df['latency_rolling'] = df['Latency_ns'].rolling(window=5000).mean()

    return df

def calculate_throughput(df):
    
    
    count = len(df)
    if count < 2: return 0
    
    
    
    
    
    
    
    
    
    return count

def generate_dashboard(dfs, spins_spsc, spins_mpsc):
    plt.style.use('ggplot')
    fig, axes = plt.subplots(2, 2, figsize=(18, 12))
    fig.suptitle('HFT Architecture Battle: SPSC vs MPSC', fontsize=16)

    
    ax1 = axes[0, 0]
    for name, df in dfs.items():
        
        sample = df.iloc[::100]
        ax1.plot(sample.index, sample['Latency_ns'], label=name, alpha=0.7, linewidth=1)
    
    ax1.set_title("1. System Latency Growth (Queue Buildup)")
    ax1.set_ylabel("Latency (nanoseconds)")
    ax1.set_xlabel("Packet Sequence")
    ax1.set_yscale('log') 
    ax1.legend()
    ax1.grid(True, which="both", ls="-", alpha=0.5)

    
    ax2 = axes[0, 1]
    for name, df in dfs.items():
        ax2.plot(df.index, df['staleness_rolling'], label=name, linewidth=2)
    
    ax2.set_title("2. Freshness Collapse (Staleness %)")
    ax2.set_ylabel("Stale Packets (%)")
    ax2.set_xlabel("Packet Sequence")
    ax2.set_ylim(0, 100)
    ax2.legend()

    
    ax3 = axes[1, 0]
    for name, df in dfs.items():
        
        ax3.hist(df['Latency_ns'], bins=100, alpha=0.5, label=name, log=True)
    
    ax3.set_title("3. Latency Distribution (Tail Risks)")
    ax3.set_xlabel("Latency (ns)")
    ax3.set_ylabel("Frequency (Log Scale)")
    ax3.legend()

    
    ax4 = axes[1, 1]
    
    labels = ['Avg Latency (us)', 'Staleness (%)', 'Spins (Millions)']
    
    
    def get_metrics(name):
        if name not in dfs: return [0, 0, 0]
        df = dfs[name]
        lat = df['Latency_ns'].mean() / 1000.0 
        stale = df['is_stale'].mean() * 100.0
        spins = (spins_spsc if name == 'SPSC' else spins_mpsc) / 1_000_000.0
        return [lat, stale, spins]

    spsc_metrics = get_metrics('SPSC')
    mpsc_metrics = get_metrics('MPSC')

    x = range(len(labels))
    width = 0.35

    ax4.bar([i - width/2 for i in x], spsc_metrics, width, label='SPSC')
    ax4.bar([i + width/2 for i in x], mpsc_metrics, width, label='MPSC')

    ax4.set_title("4. Architectural Scorecard")
    ax4.set_xticks(x)
    ax4.set_xticklabels(labels)
    ax4.legend()
    
    
    for i, v in enumerate(spsc_metrics):
        ax4.text(i - width/2, v, f"{v:.1f}", ha='center', va='bottom', fontsize=9)
    for i, v in enumerate(mpsc_metrics):
        ax4.text(i + width/2, v, f"{v:.1f}", ha='center', va='bottom', fontsize=9)

    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.savefig("thesis_dashboard.png")
    print("\nSUCCESS: Generated 'thesis_dashboard.png'")

def print_text_report(dfs, spins_spsc, spins_mpsc):
    print("\n" + "="*65)
    print(f"{'METRIC':<25} | {'SPSC (Sharded)':<18} | {'MPSC (Lock-Free)':<18}")
    print("-" * 65)
    
    spsc = dfs.get('SPSC')
    mpsc = dfs.get('MPSC')
    
    
    l_spsc = spsc['Latency_ns'].mean() if spsc is not None else 0
    l_mpsc = mpsc['Latency_ns'].mean() if mpsc is not None else 0
    
    
    p99_spsc = spsc['Latency_ns'].quantile(0.99) if spsc is not None else 0
    p99_mpsc = mpsc['Latency_ns'].quantile(0.99) if mpsc is not None else 0
    
    
    s_spsc = spsc['is_stale'].mean() * 100 if spsc is not None else 0
    s_mpsc = mpsc['is_stale'].mean() * 100 if mpsc is not None else 0
    
    
    c_spsc = len(spsc) if spsc is not None else 0
    c_mpsc = len(mpsc) if mpsc is not None else 0

    print(f"{'Avg System Latency':<25} | {l_spsc:,.0f} ns{'':<8} | {l_mpsc:,.0f} ns")
    print(f"{'p99 Tail Latency':<25} | {p99_spsc:,.0f} ns{'':<8} | {p99_mpsc:,.0f} ns")
    print(f"{'Staleness Ratio':<25} | {s_spsc:.2f} %{'':<11} | {s_mpsc:.2f} %")
    print(f"{'Total Packets Processed':<25} | {c_spsc:,.0f}{'':<11} | {c_mpsc:,.0f}")
    print(f"{'Backpressure Spins':<25} | {spins_spsc:,.0f}{'':<11} | {spins_mpsc:,.0f}")
    print("="*65 + "\n")

if __name__ == "__main__":
    dfs = {}
    
    
    df_spsc = load_data("./spsc_arch/latency_metrics.csv", "SPSC")
    if df_spsc is not None: dfs['SPSC'] = df_spsc
    
    df_mpsc = load_data("./mpsc_arch/latency_metrics.csv", "MPSC")
    if df_mpsc is not None: dfs['MPSC'] = df_mpsc

    
    s_spsc = int(sys.argv[1]) if len(sys.argv) > 1 else MANUAL_SPINS_SPSC
    s_mpsc = int(sys.argv[2]) if len(sys.argv) > 2 else MANUAL_SPINS_MPSC

    if dfs:
        generate_dashboard(dfs, s_spsc, s_mpsc)
        print_text_report(dfs, s_spsc, s_mpsc)
    else:
        print("Error: No CSV files found. Please run the benchmarks first.")