"""
Configuration module for the SPOTS (Sprouts Parallel Outcome Tree Search) solver.

This module provides the mapping between game types and their corresponding
C++ solver implementations, enabling flexible switching between different
solving algorithms and approaches.
"""

import spots._cpp

# Game configuration mapping game names to their corresponding C++ solver classes
games = {
    "Sprouts": {
        "manager": spots._cpp.PnsTreeManager_Sprouts,  # Proof-Number Search Tree Manager
        "worker_group": spots._cpp.PnsWorkersGroup_Sprouts,  # Distributed Worker Group Coordinator
        "dfpn": spots._cpp.DfpnSolver_Sprouts,  # Depth-First Proof-Number Search Solver
        "pdfpn": spots._cpp.ParallelDfpnSolver_Sprouts,  # Parallel Depth-First Proof-Number Search Solver
        "pns": spots._cpp.PnsSolver_Sprouts,  # Basic Proof-Number Search Solver
        "dfs": spots._cpp.DfsSolver_Sprouts,  # Depth-First Search Solver
    },
}
