"""
SPOTS (Sprouts Parallel Outcome Tree Search) Python Package.

This package provides Python wrappers and utilities for solving Sprouts game positions
using various algorithms including sequential and parallel Proof-Number Search variants.

The package includes:
- Sequential solvers (DFS, PNS, DFPN)
- Parallel solvers (Parallel PDFPN (by Kaneko 2010), Parallel PNS-PDFPN)
- Statistics logging and analysis tools
- Configuration management for different solving approaches

Main modules:
- spots: Command-line interface and main entry point
- sequential_solver: Single-threaded solving algorithms
- parallel_dfpn: Parallel Depth-First Proof-Number Search (by Kaneko 2010)
- parallel_solver: Parallel PNS-PDFPN using Ray framework
- worker_group: A group of workers for distributed solving using DFPN or PDFPN
- stats_logger: Performance metrics and result logging
- config: Game and solver configuration
"""

__version__ = "1.0.0"
__author__ = "Tomas Cizek"
