# Collision Tree: High-Frequency Trading Feed Handler

The Collision Tree architecture is a simulation of a high-frequency trading (HFT) market data feed handler designed to solve the "Micro-Burst Stall" problem. It uses a hierarchical, lock-free overwriting mechanism to actively conflate intermediate ticks in-flight, mathematically eliminating consumer backpressure. This ensures deterministic tail latency and data freshness even under extreme network load (e.g., bursts of 3,000,000 packets per second).

**Key Achievement:** Evaluated against baseline lock-free SPSC and MPSC queue architectures under severe market bursts, the Collision Tree constrained the data staleness ratio to just 10.95% (compared to ~75% in baselines) and reduced p99 tail latency to 1.18 ms (down from 25.17 ms), exclusively evaluating the absolute live edge of the market.

## 🚀 Performance Benchmarks

Benchmarks were conducted using a simulated high-volume market burst targeting an injection rate of approximately 3,000,000 packets per second. The system was evaluated against lossless lock-free SPSC (Single-Producer Single-Consumer) and MPSC (Multi-Producer Single-Consumer) architectures.

| Metric | SPSC Baseline | MPSC Baseline | Collision Tree (Proposed) | Improvement (vs SPSC) |
| --- | --- | --- | --- | --- |
| Average Sojourn Latency | 2.43 ms | 1.84 ms | 240 µs (0.24 ms) | ~10x Faster |
| p99 Tail Latency | 25.17 ms | 19.48 ms | 1.18 ms | ~21x More Stable |
| Data Staleness Ratio | ~75% | ~75% | 10.95% | 6.8x Fresher Data |
| Backpressure Spins | >1.09 million | 0 (bottlenecked) | 0 | Eliminated |
| Packets/sec (Consumer) | Millions (historical) | Millions (historical) | ~87,000 (live edge only)| Maximizes Goodput |

### Micro-Architectural Trade-offs
- **Instruction Overhead:** The Collision Tree requires ~94,600 instructions per successful operation (compared to 17,200 for SPSC). This 5.5x computational penalty is the physical cost of guaranteeing strict data freshness.
- **Read Livelock:** The architecture sacrifices memory isolation to allow producers to overwrite active root nodes, resulting in ~28,000 dirty reads (aborts and retries) during the burst sequence. However, this intermittent livelock is a justified trade-off to avoid processing obsolete market data.

## 🏗 System Architecture

The system mimics a production HFT ingress pipeline:

**Network Ingress (UDP):**
Uses `SO_REUSEPORT` to allow multiple consumer threads to bind to the same multicast group, simulating hardware-based Receive Side Scaling (RSS).

**The Collision Tree (The Core):**
A probabilistic, lock-free data structure (located in `collision_arch/`).
- Concurrent producers perform the computational heavy lifting of filtering, merging, and dropping stale data.
- Incoming threads conflate intermediate ticks in-flight via a lock-free overwriting protocol.
- The consumer strategy thread operates in strict $O(1)$ time by loading the root's sequence guard, copying the payload, and performing a final relaxed consistency check.
- Result: Only actionable, live market data is exclusively evaluated.

**Async Logging (The Observer):**
A Lock-Free SPSC Ring Buffer (`spsc_arch/`).
- Decouples the hot-path trading logic from the cold-path disk I/O.
- Measures nanosecond-level latency without inducing "Observer Effect" stalls.

## 🛠️ Build & Run

### Prerequisites
- Linux Environment
- `g++` (Supporting C++20)
- `python3` (For traffic simulation and analysis)

### 1. Compilation
The project contains several components that can be compiled individually.
```bash
# Example: compiling the collision architecture
g++ -std=c++20 -O3 -pthread collision_arch/main_collision_multi.cpp -o hft_engine
```

### 2. Run the Engine
Run the compiled binary:
```bash
./hft_engine
```

### 3. Start the Traffic Simulator
In a second terminal, blast the market data using one of the simulators:
```bash
g++ -std=c++20 -O3 exchange_simulator_fast.cpp -o simulator
./simulator
```

### 4. Analyze Results
After running the tests, compare the generated CSV logs:
```bash
python analyze_results.py
```

## 📂 Project Structure

- `collision_arch/` - Core Logic: The lock-free Collision Tree algorithm (`collision.h`) and tests.
- `mpsc_arch/` - Multi-Producer Single-Consumer lock-free queue architecture (Baseline).
- `spsc_arch/` - Single-Producer Single-Consumer lock-free ring buffer architecture (Baseline).
- `logger.h` - Async lock-free Ring Buffer logger.
- `udp_receiver.h` - Raw socket wrapper handling `recvfrom` and `SO_REUSEPORT`.
- `tsc_clock.h` - High precision timing using TSC.
- `exchange_simulator_*.cpp` - Market data replays over UDP.
- `analyze_results.py` - Generates performance stats and graphs from CSV logs.
