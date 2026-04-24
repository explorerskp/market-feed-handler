# Collision Tree: Lock-Free Market Feed Handler

A simulation of a high-frequency trading (HFT) market data feed handler designed to address the **Micro-Burst Stall** problem in UDP ingress pipelines. The architecture employs a hierarchical, lock-free overwriting mechanism to conflate intermediate ticks in-flight, eliminating consumer backpressure and providing deterministic tail latency under sustained burst conditions (~3,000,000 packets/sec).

Evaluated against lock-free SPSC and MPSC queue baselines, the Collision Tree reduced the data staleness ratio to **10.95%** (vs. ~75% in both baselines) and constrained p99 tail latency to **1.18 ms** (vs. 25.17 ms in SPSC), processing exclusively the live edge of the market.

---

## Performance Benchmarks

Benchmarks were conducted under a simulated high-volume market burst targeting an injection rate of approximately 3,000,000 packets/sec. The system was evaluated against lossless lock-free SPSC (Single-Producer Single-Consumer) and MPSC (Multi-Producer Single-Consumer) architectures.

| Metric | SPSC Baseline | MPSC Baseline | Collision Tree | Improvement (vs. SPSC) |
|---|---|---|---|---|
| Average Sojourn Latency | 2.43 ms | 1.84 ms | 240 µs (0.24 ms) | ~10× faster |
| p99 Tail Latency | 25.17 ms | 19.48 ms | 1.18 ms | ~21× more stable |
| Data Staleness Ratio | ~75% | ~75% | 10.95% | 6.8× fresher |
| Backpressure Spins | >1,090,000 | 0 (bottlenecked) | 0 | Eliminated |
| Packets/sec (Consumer) | Millions (historical) | Millions (historical) | ~87,000 (live edge only) | Maximises goodput |

### Micro-Architectural Trade-offs

- **Instruction Overhead:** The Collision Tree requires approximately 94,600 instructions per successful operation, compared to 17,200 for SPSC — a 5.5× computational overhead that is the direct cost of enforcing strict data freshness.
- **Read Livelock:** The architecture sacrifices memory isolation to allow producers to overwrite active root nodes. This produces ~28,000 dirty reads (aborts and retries) during the burst sequence, which is an accepted trade-off to avoid processing stale market data.

---

## System Architecture

The system models a production HFT ingress pipeline across four stages.

**Ingress Layer and Kernel Bypass:** The production architecture is designed for Kernel Bypass networking (DPDK / Solarflare EFVI), delivering raw UDP multicast packets from the Exchange Gateway directly into user-space memory via DMA, bypassing OS context switches and interrupt overhead entirely. In the current prototype, hardware constraints preclude true kernel bypass; highly optimised POSIX UDP sockets are used instead. The downstream conflation mechanics are otherwise identical. A pool of concurrent network threads spin-polls these ingress channels. Upon intercepting a payload, each thread parses the raw binary data into a structured `MarketUpdate` object (symbol, price, quantity, monotonic sequence timestamp) and records an `arrival_tsc` via the CPU's RDTSC instruction for deterministic internal latency tracking.

**Gateway Routing and Deterministic Hashing:** To prevent cross-asset data corruption, a Symbol ID-based hash maps each parsed update to a dedicated, asset-specific Collision Tree. A secondary deterministic hash then maps the producing thread to a specific leaf node within that tree's Input Layer. Distributing initial concurrent writes across disjoint leaf nodes is central to the design: it breaks the convoy effect, diffuses CPU cache-coherency storms, and avoids the memory bus contention that degrades standard lock-free queues under burst load.

**The Lock-Free Collision Tree (`collision_arch/collision.h`):** The core data structure replaces a linear queue buffer with a shallow, hierarchical tree of atomic nodes, each strictly padded and aligned to a 64-byte cache line boundary to eliminate false sharing. Every node holds an atomic sequence guard (`max_seq_seen`), the market payload, and the arrival timestamp. Rather than appending to a tail pointer, producers engage in a targeted lock-free overwriting protocol. A producer reads the node's sequence guard optimistically: if the guard is newer than the incoming packet, the packet is immediately dropped (conflated) as stale. If the producer holds fresher data, it executes an atomic CAS on the sequence guard to claim exclusive ownership and overwrites the payload. It then promotes the update to the next hierarchical level, repeating the protocol until it is either conflated by a newer packet or successfully captures the root node.

**Execution Layer and Non-Blocking Polling:** The Strategy Processing Thread operates entirely independently of producer burst rates, eliminating the producer-consumer backpressure loop. It executes a time-multiplexed, non-blocking polling routine across the root nodes of all active asset trees. Because producers perform all filtering and conflation upstream, the consumer is relieved of queue-draining operations entirely. Each read executes in strict O(1) time: the strategy thread loads the root's sequence guard with acquire semantics, copies the payload and `arrival_tsc`, and performs a final relaxed consistency check to confirm no partial producer write occurred during the read window.

**Async Logger (`spsc_arch/`):** A lock-free SPSC ring buffer decouples hot-path trading logic from cold-path disk I/O, enabling nanosecond-level latency measurement without inducing observer-effect stalls on the critical path.

---

## Project Structure

```
market_feed/
├── collision_arch/
│   ├── collision.h                  # Core Collision Tree algorithm
│   ├── main_collision_multi.cpp     # Multi-asset engine entry point
│   ├── exchange_simulator_fast.cpp  # Simulator (multi-asset)
│   ├── analyze_collision.py         # Deep-dive analysis and plots
│   └── analyze_collision_metrics.py # Thesis metrics report
├── mpsc_arch/
│   ├── mpsc_queue_lockfree.h        # Lock-free MPSC queue
│   └── main_mpsc_metrics.cpp        # MPSC baseline engine
├── spsc_arch/
│   ├── spsc_queue.h                 # Lock-free SPSC ring buffer
│   ├── main_spsc_metrics.cpp        # SPSC baseline engine
│   └── capture_spsc.cpp             # Capture tool
├── common.h                         # Shared MarketUpdate / RawUpdate structs
├── logger.h                         # Async lock-free ring buffer logger
├── udp_receiver.h                   # Raw socket wrapper (recvfrom, SO_REUSEPORT)
├── tsc_clock.h                      # High-precision TSC timing
├── exchange_simulator_fast.cpp      # Market data replay over UDP
├── exchange_simulator_throttled.cpp # Throttled variant
├── generate_multiset.py             # Multiplexes BTC data into 5 synthetic assets
├── convert_fast.py                  # Converts official CSV trade data to binary
└── analyze_results.py               # Comparative SPSC vs. MPSC dashboard
```

---

## Build & Run

### Prerequisites

- Linux (tested on Fedora)
- `g++` with C++20 support
- `python3` with `pandas`, `matplotlib`, `numpy`

### Step 1 — Prepare Market Data

Download a raw trade CSV (e.g., `BTCUSDT-aggTrades-2025-12.csv`) and convert it to binary format:

```bash
python3 convert_fast.py BTCUSDT-aggTrades-2025-12.csv market_data.bin
```

Generate the multi-asset dataset (5 synthetic symbols from BTC data):

```bash
python3 generate_multiset.py
# Output: multi_data.bin
```

### Step 2 — Compile

**Collision Tree (multi-asset):**
```bash
g++ -std=c++20 -O3 -pthread collision_arch/main_collision_multi.cpp -o collision_arch/collision_multi
```

**Exchange Simulator (for Collision Tree):**
```bash
g++ -std=c++20 -O3 collision_arch/exchange_simulator_fast.cpp -o collision_arch/exchange_multi
```

**SPSC Baseline:**
```bash
g++ -std=c++20 -O3 -pthread spsc_arch/main_spsc_metrics.cpp -o spsc_arch/spsc_metrics
```

**MPSC Baseline:**
```bash
g++ -std=c++20 -O3 -pthread mpsc_arch/main_mpsc_metrics.cpp -o mpsc_arch/mpsc_metrics
```

**General Simulator (for SPSC/MPSC baselines):**
```bash
g++ -std=c++20 -O3 exchange_simulator_fast.cpp -o exchange_fast
```

### Step 3 — Run a Benchmark

Open two terminals. In the first, start the engine:

```bash
# Collision Tree
./collision_arch/collision_multi

# or SPSC baseline
./spsc_arch/spsc_metrics

# or MPSC baseline
./mpsc_arch/mpsc_metrics
```

In the second terminal, start the simulator:

```bash
# For Collision Tree (uses multi_data.bin)
./collision_arch/exchange_multi

# For SPSC/MPSC baselines (uses market_data.bin)
./exchange_fast
```

The simulator runs for 30 seconds then exits cleanly. Press **Enter** in the engine terminal to stop it and flush logs.

### Step 4 — Analyze Results

**Collision Tree deep-dive (latency distribution, load balancing, price feed):**
```bash
cd collision_arch
python3 analyze_collision.py
# Output: collision_deep_dive.png
```

**Collision Tree thesis metrics (sojourn time, p99, staleness, throughput):**
```bash
cd collision_arch
python3 analyze_collision_metrics.py <backpressure_spins> <duration_sec>
# Example: python3 analyze_collision_metrics.py 0 30.0
```

**Comparative SPSC vs. MPSC dashboard:**
```bash
python3 analyze_results.py <spsc_spins> <mpsc_spins>
# Output: thesis_dashboard.png
```

---

## License

See [LICENSE](LICENSE).
