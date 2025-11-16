<div align="center">

# 🌱 **SPOTS: Sprouts Parallel Outcome Tree Search 🌱**

<img src="https://raw.githubusercontent.com/cizektom/spots/main/assets/sprouts.gif"
     width="170" />

[Paper](https://arxiv.org/pdf/2511.10339) •
[Installation](#-installation) •
[Usage](#-console-usage) •
[Databases](#️-databases-of-verified-results) •
[BibTeX](#-citation)
</div>

**SPOTS** is a high-performance solver for the game of *Sprouts*, built on a massively parallel Proof-Number Search **PNS-PDFPN**.
SPOTS is the first Proof-Number Search–based solver that scales efficiently to large CPU clusters. Applied to Sprouts, it achieves:

* ⚡ **332.9× speedup** on 1024 CPU cores
* 📈 **4 orders of magnitude** faster runtime than the previous state-of-the-art solver [GLOP](https://sprouts.tuxfamily.org/wiki/doku.php?id=home)
* 🤯 Proofs **1,000× more complex** than previously reported
* 🏆 Verification of the Sprouts Conjecture for **42 new positions**, nearly doubling known solved outcomes

---

## ✏️ Sprouts

Sprouts is a pencil-and-paper impartial game for two players introduced by **John H. Conway** and **Michael Paterson** in 1967.
The game starts with **n spots**, and players then alternate by following these rules:

1. **Draw a curve** connecting two spots (or looping to one).
2. **No crossings:** curves may not touch or intersect any existing curve except at the endpoints.
3. **Spot limit:** each spot may have at most **3 incident edges** (loops count twice).
4. **Add a new spot** somewhere along the drawn curve.
5. **Normal play:** the first player with no legal move **loses**.

### 🎯 **Challenge**

Solving Sprouts is a major computational benchmark:

* **Designed Difficulty:** It was intentionally created to be a simple-to-play but difficult-to-analyze.
* **Extreme Complexity:** Its game tree complexity grows exponentially, surpassing that of **Chess** and **Go** for even a small number of spots.
* **Sprouts Conjecture:** A long-standing conjecture motivates its study. It states the first player wins if and only if **n mod 6 is 3, 4, or 5**.

## 🧩 **Project Overview**

This project uses specialized algorithms to tackle Sprouts' complexity:

* **Proof-Number Search (PNS):** Sprouts features highly unbalanced game trees with a large branching factor. PNS is a best-first search algorithm that excels in these conditions, focusing on the most promising parts of the game tree.
* **Grundy Numbers (GN, nimbers):** As Sprouts is an **impartial game**, the Sprague-Grundy theorem can be applied. This allows the solver to reduce the complexity of game trees by analyzing independent subpositions separately, which often arise during a game.
* **Massive Parallelization (PNS-PDFPN):** The paper introduces PNS-PDFPN, a new algorithm that scales efficiently on many CPUs. It operates on **two parallel levels**:
  * **First Level (Distributed):** A master process assigns jobs to workers across different cluster nodes.
  * **Second Level (Shared):** Each worker node uses multiple threads (PDFPN) to parallelize its own search, utilizing shared memory.
  * **Shared Results:** Workers share key computed results (like Grundy numbers) to reduce search overhead and improve overall efficiency.

---
## 🛠️ Requirements

SPOTS requires the following system packages:

- `python3-dev`
- `python3-venv`
- `build-essential` (including **GCC 11.4.0** or compatible)
- `cmake` (version ≥ 3.20)

To install these dependencies on Debian/Ubuntu systems:

```bash
sudo apt install python3-dev python3-venv build-essential cmake
````

---

## 📦 **Installation**

Clone the repository and install the project:

```bash
git clone https://github.com/cizektom/spots.git
cd spots

# Recommended: create a virtual environment
python3 -m venv .venv
source .venv/bin/activate

pip3 install .
```

This will automatically build:

* the **C++ extension** (`spots_cpp`) via CMake
* the **Ray** distributed framework

---

## 💻 **Console Usage**

Once installed, use:

```bash
spots-solver <position> --algorithm <algorithm> [options]
```

or:

```bash
python3 -m spots <position> --algorithm <algorithm> [options]
```

---

## 🚀 **Quick Start**

```bash
# Solve a 12-spot game using DFPN
spots-solver "0*12" --algorithm dfpn

# Use 4 threads (shared-memory parallel)
spots-solver "0*12" --algorithm pdfpn --threads 4

# Fully distributed solver (Ray)
spots-solver "0*12" --algorithm pns-pdfpn --workers 4

# Compute the nimber
spots-solver "0*10" --algorithm dfpn --compute_nimber
```

---

## 📋 **Available Algorithms**

| Algorithm   | Type        | Description                        |
| ----------- | ----------- | ---------------------------------- |
| `dfs`       | Sequential  | Depth-First Search                 |
| `pns`       | Sequential  | Proof-Number Search                |
| `dfpn`      | Sequential  | Depth-First Proof-Number Search    |
| `pdfpn`     | Parallel    | Shared-memory PDFPN (Kaneko, 2010) |
| `pns-pdfpn` | Distributed | Distributed PNS-PDFPN using Ray    |

---

## 🎛️ **Command-Line Options**

### General

| Option              | Default      | Description                      |
| ------------------- | ------------ | -------------------------------- |
| `--algorithm`       | *required*   | dfs, pns, dfpn, pdfpn, pns-pdfpn |
| `--compute_nimber`  | false        | Compute Grundy number            |
| `--capacity`        | 100_000     | Transposition table size         |
| `--input_database`  | ""           | Input nimber DB                  |
| `--output_database` | database.spr | Output nimber DB                 |
| `--stats_path`      | stats.csv    | CSV statistics output            |
| `--verbose`         | false        | Verbose mode                     |
| `--seed`            | 0            | RNG seed                         |

### Parallel / Distributed

| Option          | Default | Description                               |
| --------------- | ------- | ----------------------------------------- |
| `--workers`     | 1       | Number of Ray workers                     |
| `--threads`     | 0       | Threads per worker (0 = DFPN, >0 = PDFPN) |
| `--iterations`  | 100     | Iterations per job                        |
| `--updates`     | 100     | Worker update frequency                   |
| `--grouping`    | 1       | Group size for nimber DB sharing          |
| `--no_sharing`  | false   | Disable nimber sharing                    |
| `--state_level` | 0       | Retain: 0 = full, 1 = nimbers, 2 = none   |
| `--address`     | ""      | Connect to existing Ray cluster           |

---

## 🧪 **Programmatic Usage**

### Sequential

```python
from spots.solvers.sequential_solver import SequentialSolver

solver = SequentialSolver(game="Sprouts", algorithm="dfpn")
stats = solver.solve("0*12")
print(stats)
```

### Shared-Memory Parallel

```python
from spots.solvers.parallel_solver import ParallelSolver

solver = ParallelSolver(game="Sprouts", threads=4)
stats = solver.solve("0*12")
print(stats)
```

### Distributed (Ray)

```python
from spots.solvers.parallel_solver import DistributedSolver
import ray

ray.init()
solver = DistributedSolver(game="Sprouts", workers=4, threads=2)
stats = solver.solve("0*12")
print(stats)
```

---

## 🌐 **Distributed Computing with Ray**

### Single Machine

```bash
ray start --head
spots-solver "0*12" --algorithm pns-pdfpn --workers 4 --threads 2
```

### Multi-Node Cluster

**On head node:**

```bash
ray start --head
```

**On each worker node:**

```bash
ray start --address=<head_node_ip>
```

**Run on head node:**

```bash
spots-solver "0*12" --algorithm pns-pdfpn --workers 8 --address <head_node_ip>
```

More details: [https://docs.ray.io/](https://docs.ray.io/)

---

## 📝 **Position Format**

Sprouts positions use the classical string encoding ([Applegate, Jacobson, and Sleator, 1991](https://www.cs.cmu.edu/~sleator/papers/sprouts.pdf); [Lemoine and Viennot, 2015](https://arxiv.org/abs/1008.2320)):

* `"0*12"` — 12-spot starting position
* `"0*18|1a1a"` — a 20-spot position with a connection between two spots

---

## 📊 **Output and Statistics**

SPOTS produces:

* **Console output:** live progress and final results
* **CSV statistics:** via `--stats_path`
* **Nimber database:** via `--output_database`

---

## 🗃️ **Databases of Verified Results**

SPOTS produces *nimber databases* that act as **certificates of correctness** for solved Sprouts positions.
These databases allow the full **reconstruction of reduced proof trees**, following the verification method of [Lemoine & Viennot (GLOP)](https://arxiv.org/abs/1008.2320).
All 32 outcomes computed by our **sequential solver** were fully verified using this procedure.
The positions solved by the parallel solver are substantially larger and therefore could not be fully verified; nevertheless, by verifying several of its results on less complex positions, we confirmed that it produces correct solutions.

Below are the publicly available databases generated by SPOTS:

### 🔗 **Download Databases**

| File                                                                                                        | Size   | Description                                                                        |
| ----------------------------------------------------------------------------------------------------------- | ------ | ---------------------------------------------------------------------------------- |
| [small_database.zip](https://kam.mff.cuni.cz/~cizek/Sprouts/databases/small_database.zip)                   | 136 MB | Nimber database certifying the **32 sequentially solved positions**                                   |
| [large_database.zip](https://kam.mff.cuni.cz/~cizek/Sprouts/databases/large_database.zip)                   | 12 GB  | Nimber database certifying the **remaining 10 parallelly solved positions**                                 |
| [small_verified_database.zip](https://kam.mff.cuni.cz/~cizek/Sprouts/databases/small_verified_database.zip) | 9.5 MB | **Verified nimber database** certifying the 32 sequentially solved positions; contains only results required for proof reconstruction |

These databases, together with the solver and verification procedure, make all reported results **fully reproducible** and **externally verifiable**.

---

## 📚 **Citation**

If you use SPOTS in your research, please cite:

```
@misc{cizek2025pns-pdfpn,
      title={Massively Parallel Proof-Number Search for Impartial Games and Beyond}, 
      author={Cizek, T. and Balko, M.  and Schmid, M.},
      year={2025},
      eprint={2511.10339},
      archivePrefix={arXiv},
}
```
