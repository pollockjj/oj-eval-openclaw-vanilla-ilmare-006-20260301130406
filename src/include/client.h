#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <utility>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cmath>

extern int rows;
extern int columns;
extern int total_mines;

char client_map[35][35];
int cell_state[35][35]; // 0=unknown, 1=revealed, 2=marked
int cell_number[35][35];

int total_marked;
int total_revealed;

const int cdx[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
const int cdy[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

bool CInBounds(int r, int c) {
  return r >= 0 && r < rows && c >= 0 && c < columns;
}

void Execute(int r, int c, int type);

void InitGame() {
  total_marked = 0;
  total_revealed = 0;
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
        cell_state[i][j] = 1;
        cell_number[i][j] = line[j] - '0';
        total_revealed++;
      }
    }
  }
}

bool SimpleDeduction() {
  // Mark mines: if remaining == unknown_count
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (cell_state[i][j] != 1) continue;
      int unknown = 0, marked = 0;
      for (int d = 0; d < 8; d++) {
        int ni = i + cdx[d], nj = j + cdy[d];
        if (!CInBounds(ni, nj)) continue;
        if (cell_state[ni][nj] == 0) unknown++;
        else if (cell_state[ni][nj] == 2) marked++;
      }
      if (unknown == 0) continue;
      if (cell_number[i][j] - marked == unknown) {
        for (int d = 0; d < 8; d++) {
          int ni = i + cdx[d], nj = j + cdy[d];
          if (CInBounds(ni, nj) && cell_state[ni][nj] == 0) {
            Execute(ni, nj, 1);
            return true;
          }
        }
      }
    }
  }
  // Auto explore: if marked == number
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (cell_state[i][j] != 1) continue;
      int unknown = 0, marked = 0;
      for (int d = 0; d < 8; d++) {
        int ni = i + cdx[d], nj = j + cdy[d];
        if (!CInBounds(ni, nj)) continue;
        if (cell_state[ni][nj] == 0) unknown++;
        else if (cell_state[ni][nj] == 2) marked++;
      }
      if (unknown == 0) continue;
      if (cell_number[i][j] == marked) {
        Execute(i, j, 2);
        return true;
      }
    }
  }
  return false;
}

// Tank solver
struct TankConstraint {
  int remaining;
  std::vector<int> cells;
};

int frontier_size;
std::pair<int,int> frontier_cells[900];
bool is_frontier[35][35];
int frontier_idx[35][35];

std::vector<TankConstraint> tank_constraints;
std::vector<int> cell_constraints[900];

// Precomputed log of C(n,k) for combinatorial weighting
double logC[901][901];
bool logC_computed = false;

void ComputeLogC() {
  if (logC_computed) return;
  logC_computed = true;
  for (int n = 0; n <= 900; n++) {
    logC[n][0] = 0;
    for (int k = 1; k <= n; k++) {
      logC[n][k] = logC[n-1][k-1] + std::log((double)n / k);
    }
    for (int k = n+1; k <= 900; k++) {
      logC[n][k] = -1e18; // impossible
    }
  }
}

int assignment[900];

bool CheckConstraintsFor(int fi) {
  for (int ci : cell_constraints[fi]) {
    auto& con = tank_constraints[ci];
    int mines_placed = 0, unset = 0;
    for (int idx : con.cells) {
      if (assignment[idx] == 1) mines_placed++;
      else if (assignment[idx] == -1) unset++;
    }
    if (mines_placed > con.remaining) return false;
    if (mines_placed + unset < con.remaining) return false;
  }
  return true;
}

struct Component {
  std::vector<int> cells;
};

std::vector<Component> components;

// Per-component: store valid assignment mine counts and their frequencies
// For each component, store: for each possible mine count k,
//   total_configs[k] = number of valid assignments with k mines
//   cell_mine_configs[cell][k] = number of valid assignments where cell is mine and total is k
struct CompResult {
  int max_mines;
  std::vector<double> total_configs; // indexed by mine count
  std::vector<std::vector<double>> cell_mine_configs; // [local_cell_idx][mine_count]
};

std::vector<CompResult> comp_results;

// Backtrack for a component, tracking mine count
int bt_comp_idx;
void BacktrackComp(int pos, int mines_used, int max_mines) {
  Component& comp = components[bt_comp_idx];
  CompResult& res = comp_results[bt_comp_idx];
  
  if (mines_used > max_mines) return;
  
  if (pos == (int)comp.cells.size()) {
    res.total_configs[mines_used] += 1.0;
    for (int i = 0; i < (int)comp.cells.size(); i++) {
      if (assignment[comp.cells[i]] == 1) {
        res.cell_mine_configs[i][mines_used] += 1.0;
      }
    }
    return;
  }
  
  int fi = comp.cells[pos];
  
  assignment[fi] = 0;
  if (CheckConstraintsFor(fi)) {
    BacktrackComp(pos + 1, mines_used, max_mines);
  }
  
  if (mines_used < max_mines) {
    assignment[fi] = 1;
    if (CheckConstraintsFor(fi)) {
      BacktrackComp(pos + 1, mines_used + 1, max_mines);
    }
  }
  
  assignment[fi] = -1;
}

bool TankSolver() {
  ComputeLogC();
  
  frontier_size = 0;
  memset(is_frontier, 0, sizeof(is_frontier));
  memset(frontier_idx, -1, sizeof(frontier_idx));
  
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (cell_state[i][j] != 0) continue;
      for (int d = 0; d < 8; d++) {
        int ni = i + cdx[d], nj = j + cdy[d];
        if (CInBounds(ni, nj) && cell_state[ni][nj] == 1) {
          is_frontier[i][j] = true;
          frontier_idx[i][j] = frontier_size;
          frontier_cells[frontier_size] = {i, j};
          frontier_size++;
          break;
        }
      }
    }
  }
  
  if (frontier_size == 0) return false;
  
  tank_constraints.clear();
  for (int i = 0; i < frontier_size; i++) cell_constraints[i].clear();
  
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (cell_state[i][j] != 1) continue;
      int marked = 0;
      std::vector<int> fn;
      for (int d = 0; d < 8; d++) {
        int ni = i + cdx[d], nj = j + cdy[d];
        if (!CInBounds(ni, nj)) continue;
        if (cell_state[ni][nj] == 2) marked++;
        else if (cell_state[ni][nj] == 0 && frontier_idx[ni][nj] >= 0)
          fn.push_back(frontier_idx[ni][nj]);
      }
      if (fn.empty()) continue;
      TankConstraint con;
      con.remaining = cell_number[i][j] - marked;
      con.cells = fn;
      int cid = tank_constraints.size();
      for (int fi : fn) cell_constraints[fi].push_back(cid);
      tank_constraints.push_back(con);
    }
  }
  
  int remaining_mines = total_mines - total_marked;
  int total_unknown = rows * columns - total_revealed - total_marked;
  int non_frontier = total_unknown - frontier_size;
  
  // Union-find for components
  std::vector<int> parent(frontier_size);
  for (int i = 0; i < frontier_size; i++) parent[i] = i;
  auto find = [&](int x) -> int {
    while (parent[x] != x) x = parent[x] = parent[parent[x]];
    return x;
  };
  
  for (auto& con : tank_constraints) {
    for (int i = 1; i < (int)con.cells.size(); i++) {
      int a = find(con.cells[0]), b = find(con.cells[i]);
      if (a != b) parent[a] = b;
    }
  }
  
  components.clear();
  comp_results.clear();
  int comp_map_arr[900];
  memset(comp_map_arr, -1, sizeof(comp_map_arr));
  
  for (int i = 0; i < frontier_size; i++) {
    int root = find(i);
    if (comp_map_arr[root] == -1) {
      comp_map_arr[root] = components.size();
      components.push_back({});
    }
    components[comp_map_arr[root]].cells.push_back(i);
  }
  
  // Solve each component
  memset(assignment, -1, sizeof(int) * frontier_size);
  
  bool found_definite = false;
  int def_r = -1, def_c = -1, def_type = -1;
  
  for (int ci = 0; ci < (int)components.size(); ci++) {
    auto& comp = components[ci];
    int sz = comp.cells.size();
    int max_m = std::min(remaining_mines, sz);
    
    CompResult res;
    res.max_mines = max_m;
    res.total_configs.assign(max_m + 1, 0.0);
    res.cell_mine_configs.resize(sz);
    for (int i = 0; i < sz; i++)
      res.cell_mine_configs[i].assign(max_m + 1, 0.0);
    
    comp_results.push_back(res);
    
    bt_comp_idx = ci;
    BacktrackComp(0, 0, max_m);
    
    // Check for definite cells
    for (int li = 0; li < sz; li++) {
      int fi = comp.cells[li];
      double total = 0, mine_total = 0;
      for (int k = 0; k <= max_m; k++) {
        total += comp_results[ci].total_configs[k];
        mine_total += comp_results[ci].cell_mine_configs[li][k];
      }
      if (total == 0) continue;
      if (mine_total == total && !found_definite) {
        found_definite = true;
        def_r = frontier_cells[fi].first;
        def_c = frontier_cells[fi].second;
        def_type = 1;
      } else if (mine_total == 0 && !found_definite) {
        found_definite = true;
        def_r = frontier_cells[fi].first;
        def_c = frontier_cells[fi].second;
        def_type = 0;
      }
    }
  }
  
  if (found_definite) {
    Execute(def_r, def_c, def_type);
    return true;
  }
  
  // Probability-based guessing with combinatorial weighting
  // For each frontier cell, compute P(mine) considering all components jointly
  // 
  // The total probability involves distributing remaining_mines across components and non-frontier.
  // P(cell i is mine) = sum over valid distributions of:
  //   (product of component configs with that many mines) * C(non_frontier, remaining - sum)
  //   where cell i is mine in its component's config
  //
  // This is complex with many components. Use the approach:
  // Combine components via convolution of their mine count distributions.
  
  int ncomp = components.size();
  
  // Combined distribution: prob[k] = weighted number of ways to have k mines total in frontier
  // Start with [1.0] and convolve with each component's total_configs
  std::vector<double> combined(remaining_mines + 1, 0.0);
  combined[0] = 1.0;
  
  for (int ci = 0; ci < ncomp; ci++) {
    int max_m = comp_results[ci].max_mines;
    std::vector<double> new_combined(remaining_mines + 1, 0.0);
    for (int prev = 0; prev <= remaining_mines; prev++) {
      if (combined[prev] == 0) continue;
      for (int k = 0; k <= max_m && prev + k <= remaining_mines; k++) {
        if (comp_results[ci].total_configs[k] == 0) continue;
        new_combined[prev + k] += combined[prev] * comp_results[ci].total_configs[k];
      }
    }
    combined = new_combined;
  }
  
  // Total weighted count: sum over k of combined[k] * C(non_frontier, remaining_mines - k)
  // where 0 <= remaining_mines - k <= non_frontier
  double total_weight = 0;
  for (int k = 0; k <= remaining_mines; k++) {
    int nf_mines = remaining_mines - k;
    if (nf_mines < 0 || nf_mines > non_frontier) continue;
    if (combined[k] == 0) continue;
    double log_w = std::log(combined[k]) + logC[non_frontier][nf_mines];
    total_weight += std::exp(log_w);
  }
  
  if (total_weight == 0) {
    // Fallback
    for (int i = 0; i < rows; i++)
      for (int j = 0; j < columns; j++)
        if (cell_state[i][j] == 0) {
          Execute(i, j, 0);
          return true;
        }
    return false;
  }
  
  // For each frontier cell, compute P(mine)
  // Need to compute for component ci, cell li:
  // P(mine) = sum over k: cell_mine_configs[li][k] * (product of other comps at their counts) * C(nf, rem-total_k)
  //         / total_weight
  // 
  // Efficient: for each component ci, compute "combined without ci"
  // combined_without_ci[k] = combined distribution excluding component ci
  // Then P(cell in ci is mine) = sum_k cell_mine_configs[li][k] * combined_without_ci[total_k - k] * C(nf, rem - total_k)
  
  double best_safe_prob = -1;
  int best_r = -1, best_c = -1;
  
  for (int ci = 0; ci < ncomp; ci++) {
    int max_m = comp_results[ci].max_mines;
    
    // Deconvolve: combined_without = combined / component_ci's distribution
    // combined_without[j] such that convolving with comp_ci gives combined
    // combined_without[j] = (combined[j] - sum_{k=1..max_m} comp[k]*combined_without[j-k]) / comp[0]
    // This works only if comp[0] != 0. If comp[0] == 0, we need different approach.
    
    std::vector<double> cw(remaining_mines + 1, 0.0);
    bool can_deconvolve = (comp_results[ci].total_configs[0] > 0);
    
    if (can_deconvolve) {
      double inv0 = 1.0 / comp_results[ci].total_configs[0];
      for (int j = 0; j <= remaining_mines; j++) {
        double val = combined[j];
        for (int k = 1; k <= max_m && k <= j; k++) {
          val -= comp_results[ci].total_configs[k] * cw[j - k];
        }
        cw[j] = val * inv0;
      }
    } else {
      // Rebuild without this component
      cw[0] = 1.0;
      for (int ci2 = 0; ci2 < ncomp; ci2++) {
        if (ci2 == ci) continue;
        int max_m2 = comp_results[ci2].max_mines;
        std::vector<double> new_cw(remaining_mines + 1, 0.0);
        for (int prev = 0; prev <= remaining_mines; prev++) {
          if (cw[prev] == 0) continue;
          for (int k = 0; k <= max_m2 && prev + k <= remaining_mines; k++) {
            if (comp_results[ci2].total_configs[k] == 0) continue;
            new_cw[prev + k] += cw[prev] * comp_results[ci2].total_configs[k];
          }
        }
        cw = new_cw;
      }
    }
    
    // Now for each cell in component ci
    int sz = components[ci].cells.size();
    for (int li = 0; li < sz; li++) {
      int fi = components[ci].cells[li];
      double mine_weight = 0;
      
      for (int k = 0; k <= max_m; k++) {
        if (comp_results[ci].cell_mine_configs[li][k] == 0) continue;
        // Need to sum over total frontier mines = k + j where j comes from other components
        for (int j = 0; j <= remaining_mines - k; j++) {
          int total_frontier = k + j;
          int nf_mines = remaining_mines - total_frontier;
          if (nf_mines < 0 || nf_mines > non_frontier) continue;
          if (cw[j] == 0) continue;
          double w = comp_results[ci].cell_mine_configs[li][k] * cw[j];
          if (w == 0) continue;
          double log_comb = logC[non_frontier][nf_mines];
          mine_weight += w * std::exp(log_comb);
        }
      }
      
      double safe_prob = 1.0 - mine_weight / total_weight;
      if (safe_prob > best_safe_prob) {
        best_safe_prob = safe_prob;
        best_r = frontier_cells[fi].first;
        best_c = frontier_cells[fi].second;
      }
    }
  }
  
  // Non-frontier probability
  if (non_frontier > 0) {
    // P(specific non-frontier cell is mine) = E[nf_mines] / non_frontier
    // E[nf_mines] = sum_k combined[k] * C(nf, rem-k) * (rem-k)/nf / (sum combined[k]*C(nf,rem-k))
    // Actually: for a specific non-frontier cell,
    // P(mine) = sum_k combined[k] * C(nf-1, rem-k-1) / sum_k combined[k] * C(nf, rem-k)
    double nf_mine_weight = 0;
    for (int k = 0; k <= remaining_mines; k++) {
      int nf_mines = remaining_mines - k;
      if (nf_mines <= 0 || nf_mines > non_frontier) continue;
      if (combined[k] == 0) continue;
      double w = combined[k] * std::exp(logC[non_frontier - 1][nf_mines - 1]);
      nf_mine_weight += w;
    }
    double nf_safe_prob = 1.0 - nf_mine_weight / total_weight;
    
    if (nf_safe_prob > best_safe_prob) {
      // Find a non-frontier cell
      for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
          if (cell_state[i][j] == 0 && !is_frontier[i][j]) {
            best_safe_prob = nf_safe_prob;
            best_r = i;
            best_c = j;
            goto done_nf;
          }
        }
      }
      done_nf:;
    }
  }
  
  if (best_r >= 0) {
    Execute(best_r, best_c, 0);
    return true;
  }
  
  return false;
}

bool FallbackVisit() {
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++)
      if (cell_state[i][j] == 0) {
        Execute(i, j, 0);
        return true;
      }
  return false;
}

void Decide() {
  if (SimpleDeduction()) return;
  if (TankSolver()) return;
  FallbackVisit();
}

#endif
