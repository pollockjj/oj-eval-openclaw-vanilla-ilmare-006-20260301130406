#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <utility>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cstdlib>

extern int rows;
extern int columns;
extern int total_mines;

// Client map: what we see
// '?' = unknown, '0'-'8' = number, '@' = marked mine
char client_map[35][35];

// Derived info
// cell_state: 0=unknown, 1=revealed number, 2=marked mine
int cell_state[35][35];
int cell_number[35][35]; // the number shown (only valid if revealed)

int total_marked;
int total_revealed;
bool first_call;

const int cdx[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
const int cdy[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

bool CInBounds(int r, int c) {
  return r >= 0 && r < rows && c >= 0 && c < columns;
}

void Execute(int r, int c, int type);

void InitGame() {
  // Reset all globals
  total_marked = 0;
  total_revealed = 0;
  first_call = true;
  for (int i = 0; i < 35; i++)
    for (int j = 0; j < 35; j++) {
      client_map[i][j] = '?';
      cell_state[i][j] = 0;
      cell_number[i][j] = 0;
    }

  int first_row, first_column;
  std::cin >> first_row >> first_column;
  Execute(first_row, first_column, 0);
}

void ReadMap() {
  total_marked = 0;
  total_revealed = 0;
  std::string line;
  for (int i = 0; i < rows; i++) {
    std::cin >> line;
    for (int j = 0; j < columns; j++) {
      client_map[i][j] = line[j];
      if (line[j] == '?') {
        cell_state[i][j] = 0;
      } else if (line[j] == '@') {
        cell_state[i][j] = 2;
        total_marked++;
      } else {
        // digit 0-8
        cell_state[i][j] = 1;
        cell_number[i][j] = line[j] - '0';
        total_revealed++;
      }
    }
  }
}

// Get neighbors info for a revealed cell
struct NeighborInfo {
  int unknown_count;
  int marked_count;
  int number;
  std::vector<std::pair<int,int>> unknowns;
};

NeighborInfo GetNeighborInfo(int r, int c) {
  NeighborInfo info;
  info.unknown_count = 0;
  info.marked_count = 0;
  info.number = cell_number[r][c];
  for (int d = 0; d < 8; d++) {
    int ni = r + cdx[d], nj = c + cdy[d];
    if (!CInBounds(ni, nj)) continue;
    if (cell_state[ni][nj] == 0) {
      info.unknown_count++;
      info.unknowns.push_back({ni, nj});
    } else if (cell_state[ni][nj] == 2) {
      info.marked_count++;
    }
  }
  return info;
}

// Simple deduction: mark obvious mines, reveal obvious safe cells
// Returns true if progress was made
bool SimpleDeduction() {
  bool progress = false;
  
  // Pass 1: If number - marked == unknown_count, all unknowns are mines
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (cell_state[i][j] != 1) continue;
      NeighborInfo info = GetNeighborInfo(i, j);
      if (info.unknown_count == 0) continue;
      int remaining_mines = info.number - info.marked_count;
      if (remaining_mines == info.unknown_count) {
        // All unknowns are mines
        for (auto& [r, c] : info.unknowns) {
          Execute(r, c, 1); // mark mine
          progress = true;
          return true; // one Execute per Decide
        }
      }
    }
  }
  
  // Pass 2: If number == marked, all unknowns are safe -> auto explore
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (cell_state[i][j] != 1) continue;
      NeighborInfo info = GetNeighborInfo(i, j);
      if (info.unknown_count == 0) continue;
      if (info.number == info.marked_count) {
        // All unknowns are safe, auto explore
        Execute(i, j, 2);
        progress = true;
        return true;
      }
    }
  }
  
  return false;
}

// Constraint-based (tank) solver for coupled cells
// Collect all unknown cells adjacent to at least one revealed number (frontier)
// Try all valid mine assignments and find cells that are always mine or always safe

// For efficiency, limit frontier size
struct Constraint {
  int r, c; // the numbered cell
  int remaining_mines; // number - marked_count
  std::vector<int> frontier_indices; // indices into frontier array
};

bool TankSolver() {
  // Collect frontier: unknown cells adjacent to at least one number
  std::vector<std::pair<int,int>> frontier;
  bool is_frontier[35][35];
  memset(is_frontier, 0, sizeof(is_frontier));
  
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (cell_state[i][j] != 0) continue;
      bool adj_to_number = false;
      for (int d = 0; d < 8; d++) {
        int ni = i + cdx[d], nj = j + cdy[d];
        if (CInBounds(ni, nj) && cell_state[ni][nj] == 1) {
          adj_to_number = true;
          break;
        }
      }
      if (adj_to_number) {
        is_frontier[i][j] = true;
        frontier.push_back({i, j});
      }
    }
  }
  
  if (frontier.empty()) return false;
  
  int n = frontier.size();
  
  // If frontier too large, split into connected components
  // Build adjacency: two frontier cells are connected if they share a constraint
  
  // Build constraints
  std::vector<Constraint> constraints;
  // Map frontier cells to indices
  int findex[35][35];
  memset(findex, -1, sizeof(findex));
  for (int i = 0; i < n; i++) {
    findex[frontier[i].first][frontier[i].second] = i;
  }
  
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (cell_state[i][j] != 1) continue;
      NeighborInfo info = GetNeighborInfo(i, j);
      if (info.unknown_count == 0) continue;
      Constraint con;
      con.r = i;
      con.c = j;
      con.remaining_mines = info.number - info.marked_count;
      for (auto& [r, c] : info.unknowns) {
        int idx = findex[r][c];
        if (idx >= 0) con.frontier_indices.push_back(idx);
      }
      if (!con.frontier_indices.empty()) {
        constraints.push_back(con);
      }
    }
  }
  
  // Connected components via union-find
  std::vector<int> parent(n);
  for (int i = 0; i < n; i++) parent[i] = i;
  auto find = [&](int x) -> int {
    while (parent[x] != x) x = parent[x] = parent[parent[x]];
    return x;
  };
  auto unite = [&](int a, int b) {
    a = find(a); b = find(b);
    if (a != b) parent[a] = b;
  };
  
  for (auto& con : constraints) {
    for (int i = 1; i < (int)con.frontier_indices.size(); i++) {
      unite(con.frontier_indices[0], con.frontier_indices[i]);
    }
  }
  
  // Group by component
  std::vector<std::vector<int>> components;
  int comp_id[35*35];
  memset(comp_id, -1, sizeof(comp_id));
  for (int i = 0; i < n; i++) {
    int root = find(i);
    if (comp_id[root] == -1) {
      comp_id[root] = components.size();
      components.push_back({});
    }
    components[comp_id[root]].push_back(i);
  }
  
  // Process each component
  // For components small enough (<=25), enumerate all valid assignments
  // Track: mine_count[i] = number of valid assignments where cell i is a mine
  //        total_valid = total valid assignments
  
  std::vector<long long> mine_freq(n, 0);
  std::vector<long long> total_valid_per_comp(components.size(), 0);
  
  bool found_definite = false;
  int definite_r = -1, definite_c = -1, definite_type = -1;
  
  for (int ci = 0; ci < (int)components.size(); ci++) {
    auto& comp = components[ci];
    int sz = comp.size();
    
    if (sz > 25) continue; // skip too large components
    
    // Get constraints relevant to this component
    int comp_root = find(comp[0]);
    std::vector<Constraint*> rel_cons;
    for (auto& con : constraints) {
      if (!con.frontier_indices.empty() && find(con.frontier_indices[0]) == comp_root) {
        rel_cons.push_back(&con);
      }
    }
    
    // Local index mapping
    int local_idx[35*35];
    memset(local_idx, -1, sizeof(local_idx));
    for (int i = 0; i < sz; i++) {
      local_idx[comp[i]] = i;
    }
    
    // Enumerate 2^sz assignments
    long long valid_count = 0;
    std::vector<long long> local_mine_freq(sz, 0);
    
    // Remaining mines globally: total_mines - total_marked
    int remaining_global = total_mines - total_marked;
    // Non-frontier unknowns
    int total_unknown = rows * columns - total_revealed - total_marked;
    int non_frontier_unknown = total_unknown - n;
    
    for (long long mask = 0; mask < (1LL << sz); mask++) {
      int mines_in_mask = __builtin_popcountll(mask);
      
      // Check: mines_in_mask <= remaining_global
      // Also: remaining_global - mines_in_mask <= non_frontier_unknown + (n - sz) [mines in other components]
      // This is hard to check exactly across components, so we just check constraints
      
      bool valid = true;
      for (auto* con : rel_cons) {
        int mine_count_here = 0;
        for (int fi : con->frontier_indices) {
          int li = local_idx[fi];
          if (li >= 0 && (mask & (1LL << li))) mine_count_here++;
        }
        if (mine_count_here != con->remaining_mines) {
          valid = false;
          break;
        }
      }
      
      if (valid) {
        valid_count++;
        for (int i = 0; i < sz; i++) {
          if (mask & (1LL << i)) local_mine_freq[i]++;
        }
      }
    }
    
    total_valid_per_comp[ci] = valid_count;
    
    if (valid_count == 0) continue;
    
    for (int i = 0; i < sz; i++) {
      mine_freq[comp[i]] = local_mine_freq[i];
      
      // Check for definite mine or definite safe
      if (local_mine_freq[i] == valid_count && !found_definite) {
        // Definitely a mine
        found_definite = true;
        definite_r = frontier[comp[i]].first;
        definite_c = frontier[comp[i]].second;
        definite_type = 1; // mark
      } else if (local_mine_freq[i] == 0 && !found_definite) {
        // Definitely safe
        found_definite = true;
        definite_r = frontier[comp[i]].first;
        definite_c = frontier[comp[i]].second;
        definite_type = 0; // visit
      }
    }
    
    // If we found something definite, act immediately
    if (found_definite) {
      Execute(definite_r, definite_c, definite_type);
      return true;
    }
  }
  
  // No definite deduction found. Pick the cell with lowest mine probability.
  // Also consider non-frontier cells probability.
  int remaining_global = total_mines - total_marked;
  int total_unknown = rows * columns - total_revealed - total_marked;
  int non_frontier_unknown = total_unknown - n;
  
  double best_prob = 2.0;
  int best_r = -1, best_c = -1;
  
  // Check frontier cells
  for (int ci = 0; ci < (int)components.size(); ci++) {
    auto& comp = components[ci];
    if (total_valid_per_comp[ci] == 0) continue;
    for (int i = 0; i < (int)comp.size(); i++) {
      double prob = (double)mine_freq[comp[i]] / total_valid_per_comp[ci];
      if (prob < best_prob) {
        best_prob = prob;
        best_r = frontier[comp[i]].first;
        best_c = frontier[comp[i]].second;
      }
    }
  }
  
  // Check non-frontier unknown cells probability
  // Approximate: mines not in frontier / non-frontier unknowns
  // This is rough but serviceable
  if (non_frontier_unknown > 0) {
    // Estimate mines in frontier (average across valid assignments)
    // For simplicity, use total_mines - total_marked as remaining
    // and estimate frontier mines from frequencies
    // Actually for a proper estimate we'd need cross-component analysis
    // Simple heuristic: remaining_global / total_unknown for non-frontier
    double non_frontier_prob = (double)remaining_global / total_unknown;
    if (non_frontier_prob < best_prob) {
      // Pick a random non-frontier unknown cell
      for (int i = 0; i < rows && best_prob > non_frontier_prob; i++) {
        for (int j = 0; j < columns; j++) {
          if (cell_state[i][j] == 0 && !is_frontier[i][j]) {
            best_prob = non_frontier_prob;
            best_r = i;
            best_c = j;
            break;
          }
        }
      }
    }
  }
  
  if (best_r >= 0) {
    Execute(best_r, best_c, 0); // visit the safest cell
    return true;
  }
  
  return false;
}

// Fallback: visit any unknown cell
bool FallbackVisit() {
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (cell_state[i][j] == 0) {
        Execute(i, j, 0);
        return true;
      }
    }
  }
  return false;
}

void Decide() {
  // Try simple deduction first
  if (SimpleDeduction()) return;
  
  // Try tank solver
  if (TankSolver()) return;
  
  // Fallback
  FallbackVisit();
}

#endif
