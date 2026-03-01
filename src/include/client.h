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

// Tank solver with backtracking per component
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
double comp_solutions[200];
double comp_mine_prob[900];

int bt_comp_idx;
void BacktrackComponent(int pos, int mines_used, int max_mines) {
  Component& comp = components[bt_comp_idx];
  if (mines_used > max_mines) return;
  if (pos == (int)comp.cells.size()) {
    comp_solutions[bt_comp_idx] += 1.0;
    for (int i = 0; i < (int)comp.cells.size(); i++) {
      if (assignment[comp.cells[i]] == 1) {
        comp_mine_prob[comp.cells[i]] += 1.0;
      }
    }
    return;
  }
  int fi = comp.cells[pos];
  assignment[fi] = 0;
  if (CheckConstraintsFor(fi)) {
    BacktrackComponent(pos + 1, mines_used, max_mines);
  }
  if (mines_used < max_mines) {
    assignment[fi] = 1;
    if (CheckConstraintsFor(fi)) {
      BacktrackComponent(pos + 1, mines_used + 1, max_mines);
    }
  }
  assignment[fi] = -1;
}

bool TankSolver() {
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
  
  // Union-find
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
  
  memset(assignment, -1, sizeof(int) * frontier_size);
  memset(comp_mine_prob, 0, sizeof(double) * frontier_size);
  memset(comp_solutions, 0, sizeof(double) * components.size());
  
  bool found_definite = false;
  int def_r = -1, def_c = -1, def_type = -1;
  
  for (int ci = 0; ci < (int)components.size(); ci++) {
    auto& comp = components[ci];
    int max_m = std::min(remaining_mines, (int)comp.cells.size());
    
    bt_comp_idx = ci;
    BacktrackComponent(0, 0, max_m);
    
    if (comp_solutions[ci] == 0) continue;
    
    for (int fi : comp.cells) {
      if (comp_mine_prob[fi] == comp_solutions[ci] && !found_definite) {
        found_definite = true;
        def_r = frontier_cells[fi].first;
        def_c = frontier_cells[fi].second;
        def_type = 1;
      } else if (comp_mine_prob[fi] == 0 && !found_definite) {
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
  
  // Probability-based guessing
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
    double expected_frontier_mines = 0;
    for (int ci = 0; ci < (int)components.size(); ci++) {
      if (comp_solutions[ci] == 0) continue;
      for (int fi : components[ci].cells) {
        expected_frontier_mines += comp_mine_prob[fi] / comp_solutions[ci];
      }
    }
    double remaining_for_nf = remaining_mines - expected_frontier_mines;
    if (remaining_for_nf < 0) remaining_for_nf = 0;
    double nf_prob = remaining_for_nf / non_frontier;
    
    if (nf_prob < best_prob) {
      for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
          if (cell_state[i][j] == 0 && !is_frontier[i][j]) {
            best_prob = nf_prob;
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
