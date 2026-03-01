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

// Client map
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

// Simple deduction
bool SimpleDeduction() {
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
      int remaining = cell_number[i][j] - marked;
      if (remaining == unknown) {
        // All unknowns are mines - mark one
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
        Execute(i, j, 2); // auto explore
        return true;
      }
    }
  }
  return false;
}

// Tank solver with backtracking
// Frontier: unknown cells adjacent to at least one revealed number
// Constraints: from revealed numbers adjacent to frontier cells

struct TankConstraint {
  int remaining; // mines still needed
  std::vector<int> cells; // frontier indices
};

// Global arrays for tank solver
int frontier_size;
std::pair<int,int> frontier_cells[900];
bool is_frontier[35][35];
int frontier_idx[35][35];

std::vector<TankConstraint> tank_constraints;

// Per-cell: which constraints reference it
std::vector<int> cell_constraints[900];

// Solution counting
double mine_prob[900]; // probability of being mine
double total_solutions;
int assignment[900]; // -1=unset, 0=safe, 1=mine

int remaining_global_mines;

// Validate partial assignment against constraints
// Returns: 0=invalid, 1=valid and can continue, 2=fully determined
int ValidateConstraints() {
  for (auto& con : tank_constraints) {
    int mines_placed = 0;
    int unset = 0;
    for (int idx : con.cells) {
      if (assignment[idx] == 1) mines_placed++;
      else if (assignment[idx] == -1) unset++;
    }
    if (mines_placed > con.remaining) return 0;
    if (mines_placed + unset < con.remaining) return 0;
  }
  return 1;
}

// Check constraints involving cell at frontier index fi
bool CheckConstraintsFor(int fi) {
  for (int ci : cell_constraints[fi]) {
    auto& con = tank_constraints[ci];
    int mines_placed = 0;
    int unset = 0;
    for (int idx : con.cells) {
      if (assignment[idx] == 1) mines_placed++;
      else if (assignment[idx] == -1) unset++;
    }
    if (mines_placed > con.remaining) return false;
    if (mines_placed + unset < con.remaining) return false;
  }
  return true;
}

void Backtrack(int pos, int mines_used) {
  if (mines_used > remaining_global_mines) return;
  
  if (pos == frontier_size) {
    // Valid complete assignment
    // Weight by combinations of placing remaining mines in non-frontier cells
    // remaining = remaining_global_mines - mines_used
    // non_frontier_unknown = total_unknown - frontier_size
    // weight = C(non_frontier_unknown, remaining)
    // For probability comparison, we can just count (or use log)
    // Actually for correct probability we need the combinatorial weight
    // But for determining definite mine/safe, weight doesn't matter
    // For probability estimation, we'll just count uniformly for now
    total_solutions += 1.0;
    for (int i = 0; i < frontier_size; i++) {
      if (assignment[i] == 1) mine_prob[i] += 1.0;
    }
    return;
  }
  
  // Try safe (0)
  assignment[pos] = 0;
  if (CheckConstraintsFor(pos)) {
    Backtrack(pos + 1, mines_used);
  }
  
  // Try mine (1)
  if (mines_used < remaining_global_mines) {
    assignment[pos] = 1;
    if (CheckConstraintsFor(pos)) {
      Backtrack(pos + 1, mines_used + 1);
    }
  }
  
  assignment[pos] = -1;
}

// Component-based backtracking
struct Component {
  std::vector<int> cells; // frontier indices
  std::vector<int> constraint_ids;
};

std::vector<Component> components;

double comp_solutions[100]; // total solutions per component
double comp_mine_prob[900]; // mine probability per cell per component

void BacktrackComponent(int comp_idx, int pos, int mines_used, int max_mines) {
  Component& comp = components[comp_idx];
  
  if (mines_used > max_mines) return;
  
  if (pos == (int)comp.cells.size()) {
    comp_solutions[comp_idx] += 1.0;
    for (int i = 0; i < (int)comp.cells.size(); i++) {
      if (assignment[comp.cells[i]] == 1) {
        comp_mine_prob[comp.cells[i]] += 1.0;
      }
    }
    return;
  }
  
  int fi = comp.cells[pos];
  
  // Try safe (0)
  assignment[fi] = 0;
  if (CheckConstraintsFor(fi)) {
    BacktrackComponent(comp_idx, pos + 1, mines_used, max_mines);
  }
  
  // Try mine (1)
  if (mines_used < max_mines) {
    assignment[fi] = 1;
    if (CheckConstraintsFor(fi)) {
      BacktrackComponent(comp_idx, pos + 1, mines_used + 1, max_mines);
    }
  }
  
  assignment[fi] = -1;
}

bool TankSolver() {
  // Build frontier
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
  
  // Build constraints
  tank_constraints.clear();
  for (int i = 0; i < frontier_size; i++) cell_constraints[i].clear();
  
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (cell_state[i][j] != 1) continue;
      int marked = 0, unknown = 0;
      std::vector<int> frontier_neighbors;
      for (int d = 0; d < 8; d++) {
        int ni = i + cdx[d], nj = j + cdy[d];
        if (!CInBounds(ni, nj)) continue;
        if (cell_state[ni][nj] == 2) marked++;
        else if (cell_state[ni][nj] == 0) {
          unknown++;
          if (frontier_idx[ni][nj] >= 0)
            frontier_neighbors.push_back(frontier_idx[ni][nj]);
        }
      }
      if (frontier_neighbors.empty()) continue;
      TankConstraint con;
      con.remaining = cell_number[i][j] - marked;
      con.cells = frontier_neighbors;
      int cid = tank_constraints.size();
      for (int fi : frontier_neighbors) {
        cell_constraints[fi].push_back(cid);
      }
      tank_constraints.push_back(con);
    }
  }
  
  remaining_global_mines = total_mines - total_marked;
  
  // Build connected components using union-find
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
  
  // Group into components
  components.clear();
  int comp_map[900];
  memset(comp_map, -1, sizeof(comp_map));
  
  for (int i = 0; i < frontier_size; i++) {
    int root = find(i);
    if (comp_map[root] == -1) {
      comp_map[root] = components.size();
      components.push_back({});
    }
    components[comp_map[root]].cells.push_back(i);
  }
  
  // Assign constraints to components
  for (int ci = 0; ci < (int)tank_constraints.size(); ci++) {
    int root = find(tank_constraints[ci].cells[0]);
    components[comp_map[root]].constraint_ids.push_back(ci);
  }
  
  // Solve each component
  memset(assignment, -1, sizeof(int) * frontier_size);
  memset(comp_mine_prob, 0, sizeof(double) * frontier_size);
  memset(comp_solutions, 0, sizeof(double) * components.size());
  
  bool found_definite = false;
  int def_r = -1, def_c = -1, def_type = -1;
  
  for (int ci = 0; ci < (int)components.size(); ci++) {
    auto& comp = components[ci];
    
    // For large components, limit max_mines to speed up
    int max_mines = remaining_global_mines;
    // Also limit by sum of constraint remaining values
    // and component size
    if (max_mines > (int)comp.cells.size()) max_mines = comp.cells.size();
    
    BacktrackComponent(ci, 0, 0, max_mines);
    
    if (comp_solutions[ci] == 0) continue;
    
    // Check for definite cells
    for (int fi : comp.cells) {
      if (comp_mine_prob[fi] == comp_solutions[ci]) {
        // Definitely mine
        if (!found_definite) {
          found_definite = true;
          def_r = frontier_cells[fi].first;
          def_c = frontier_cells[fi].second;
          def_type = 1;
        }
      } else if (comp_mine_prob[fi] == 0) {
        // Definitely safe
        if (!found_definite) {
          found_definite = true;
          def_r = frontier_cells[fi].first;
          def_c = frontier_cells[fi].second;
          def_type = 0;
        }
      }
    }
  }
  
  if (found_definite) {
    Execute(def_r, def_c, def_type);
    return true;
  }
  
  // No definite - pick lowest probability mine cell
  int total_unknown = rows * columns - total_revealed - total_marked;
  int non_frontier = total_unknown - frontier_size;
  
  double best_prob = 2.0;
  int best_r = -1, best_c = -1;
  
  for (int ci = 0; ci < (int)components.size(); ci++) {
    if (comp_solutions[ci] == 0) continue;
    for (int fi : components[ci].cells) {
      double p = comp_mine_prob[fi] / comp_solutions[ci];
      if (p < best_prob) {
        best_prob = p;
        best_r = frontier_cells[fi].first;
        best_c = frontier_cells[fi].second;
      }
    }
  }
  
  // Non-frontier probability estimate
  if (non_frontier > 0) {
    // Estimate mines consumed by frontier
    // For simplicity, compute expected mines in each component
    double expected_frontier_mines = 0;
    for (int ci = 0; ci < (int)components.size(); ci++) {
      if (comp_solutions[ci] == 0) continue;
      double comp_expected = 0;
      for (int fi : components[ci].cells) {
        comp_expected += comp_mine_prob[fi] / comp_solutions[ci];
      }
      expected_frontier_mines += comp_expected;
    }
    double remaining_for_nf = remaining_global_mines - expected_frontier_mines;
    if (remaining_for_nf < 0) remaining_for_nf = 0;
    double nf_prob = (non_frontier > 0) ? remaining_for_nf / non_frontier : 1.0;
    
    if (nf_prob < best_prob) {
      // Find a non-frontier unknown cell (prefer corners/edges for safety)
      for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
          if (cell_state[i][j] == 0 && !is_frontier[i][j]) {
            best_prob = nf_prob;
            best_r = i;
            best_c = j;
            goto found_nf;
          }
        }
      }
      found_nf:;
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
