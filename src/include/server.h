#ifndef SERVER_H
#define SERVER_H

#include <cstdlib>
#include <iostream>
#include <cstring>

int rows;
int columns;
int total_mines;
int game_state;

// Grid data
// mine_map[i][j]: true if mine
// state[i][j]: 0=unvisited, 1=visited, 2=marked
// mine_count[i][j]: number of adjacent mines
bool mine_map[35][35];
int state[35][35];
int mine_count[35][35];

int visit_count;
int marked_mine_count;
int total_non_mines;

const int dx[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
const int dy[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

bool InBounds(int r, int c) {
  return r >= 0 && r < rows && c >= 0 && c < columns;
}

void ComputeMineCounts() {
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      int cnt = 0;
      for (int d = 0; d < 8; d++) {
        int ni = i + dx[d], nj = j + dy[d];
        if (InBounds(ni, nj) && mine_map[ni][nj]) cnt++;
      }
      mine_count[i][j] = cnt;
    }
  }
}

void CheckWin() {
  if (visit_count == total_non_mines) {
    game_state = 1;
  }
}

void DoVisit(int r, int c);

void DoVisit(int r, int c) {
  if (!InBounds(r, c)) return;
  if (state[r][c] != 0) return; // already visited or marked
  if (mine_map[r][c]) {
    // visiting a mine -> loss
    state[r][c] = 1;
    game_state = -1;
    return;
  }
  state[r][c] = 1;
  visit_count++;
  if (mine_count[r][c] == 0) {
    // recursively visit neighbors
    for (int d = 0; d < 8; d++) {
      int ni = r + dx[d], nj = c + dy[d];
      DoVisit(ni, nj);
      if (game_state != 0) return;
    }
  }
}

void InitMap() {
  std::cin >> rows >> columns;
  total_mines = 0;
  visit_count = 0;
  marked_mine_count = 0;
  game_state = 0;
  std::string line;
  for (int i = 0; i < rows; i++) {
    std::cin >> line;
    for (int j = 0; j < columns; j++) {
      mine_map[i][j] = (line[j] == 'X');
      if (mine_map[i][j]) total_mines++;
      state[i][j] = 0;
    }
  }
  total_non_mines = rows * columns - total_mines;
  ComputeMineCounts();
}

void VisitBlock(int r, int c) {
  if (!InBounds(r, c)) return;
  if (state[r][c] != 0) return; // already visited or marked -> no effect
  DoVisit(r, c);
  if (game_state == 0) CheckWin();
}

void MarkMine(int r, int c) {
  if (!InBounds(r, c)) return;
  if (state[r][c] != 0) return; // already visited or marked -> no effect
  state[r][c] = 2; // marked
  if (mine_map[r][c]) {
    marked_mine_count++;
    if (game_state == 0) CheckWin();
  } else {
    // marked a non-mine -> immediate loss
    game_state = -1;
  }
}

void AutoExplore(int r, int c) {
  if (!InBounds(r, c)) return;
  // must be a visited non-mine grid
  if (state[r][c] != 1 || mine_map[r][c]) return;
  // count marked neighbors
  int marked = 0;
  for (int d = 0; d < 8; d++) {
    int ni = r + dx[d], nj = c + dy[d];
    if (InBounds(ni, nj) && state[ni][nj] == 2) marked++;
  }
  if (marked != mine_count[r][c]) return;
  // visit all non-marked, non-visited neighbors
  for (int d = 0; d < 8; d++) {
    int ni = r + dx[d], nj = c + dy[d];
    if (InBounds(ni, nj) && state[ni][nj] == 0) {
      DoVisit(ni, nj);
      if (game_state != 0) return;
    }
  }
  if (game_state == 0) CheckWin();
}

void ExitGame() {
  if (game_state == 1) {
    std::cout << "YOU WIN!" << std::endl;
    std::cout << visit_count << " " << total_mines << std::endl;
  } else {
    std::cout << "GAME OVER!" << std::endl;
    std::cout << visit_count << " " << marked_mine_count << std::endl;
  }
  exit(0);
}

void PrintMap() {
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (state[i][j] == 0) {
        // unvisited
        if (game_state == 1 && mine_map[i][j]) {
          std::cout << '@';
        } else {
          std::cout << '?';
        }
      } else if (state[i][j] == 1) {
        // visited
        if (mine_map[i][j]) {
          std::cout << 'X';
        } else {
          std::cout << mine_count[i][j];
        }
      } else {
        // marked (state == 2)
        if (mine_map[i][j]) {
          std::cout << '@';
        } else {
          std::cout << 'X'; // marked non-mine = loss
        }
      }
    }
    std::cout << std::endl;
  }
}

#endif
