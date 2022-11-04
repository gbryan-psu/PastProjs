############################################################
# CMPSC 442: Homework 2
############################################################

student_name = "Gabien Bryan"

############################################################
# Imports
############################################################

import itertools
import math
import random
import copy
from collections import deque



############################################################
# Section 1: N-Queens
############################################################

def num_placements_all(n):
    return math.factorial(n*n) / math.factorial(n) / math.factorial(n*n-n)

def num_placements_one_per_row(n):
    return n**n

def n_queens_valid(board):
    # x and y will represent columns
    for i, x in enumerate(board):
        for j, y in enumerate(board):
            if i != j:
                if x == y or (x - y) == (i - j) or (x - y) == (j - i): #check for same col or diagonal
                    return False
    return True

def n_queens_solutions(n, board=()):
     for x in range(n):
        if n_queens_valid(board + (x,)):
            if len(board) == n - 1:
                yield [x]
            if len(board) != n - 1:
                for y in n_queens_solutions(n, board + (x,)):
                    yield [x] + y
  

############################################################
# Section 2: Lights Out
############################################################

class LightsOutPuzzle(object):

    def __init__(self, board):
        self.board = board
        self.rows = len(board)-1
        self.cols = len(board[0])-1

    def get_board(self):
        return self.board

    def perform_move(self, row, col):
        #Error moves that will not effect the board
        if row < 0 or row >= self.rows+1 or col >= self.cols+1:
            return
        #Toggle move
        self.board[row][col] = not self.board[row][col]

        #Toggle above
        if row <= self.rows - 1:
            self.board[row+1][col] = not self.board[row+1][col]
        #Toggle below
        if row >= 1:
            self.board[row-1][col] = not self.board[row-1][col]
        #Toggle right
        if col <= self.cols - 1:
            self.board[row][col+1] = not self.board[row][col+1]
        #Toggle left
        if col >= 1:
            self.board[row][col-1] = not self.board[row][col-1]
            

    def scramble(self):
        for row in range(self.rows+1):
            for col in range(self.cols+1):
                if random.random() < 0.5:
                    self.perform_move(row, col)

    def is_solved(self):
        for row in range(self.rows+1):
            for col in range(self.cols+1):
                if self.board[row][col]:
                    return False
        return True

    def copy(self):
        return LightsOutPuzzle(copy.deepcopy(self.board))

    def successors(self):
        for row in range(self.rows+1):
            for col in range(self.cols+1):
                successor = self.copy()
                successor.perform_move(row, col)
                yield ((row, col), successor)

    def find_solution(self):
        solution = []
        moves = {}

        if self.is_solved():
            return solution

        frontier = [self]
        while True:
            if frontier == []:
                return None
            element = frontier.pop(0) #fifo

            for move, new_ele in element.successors():
                board = tuple(tuple(x) for x in new_ele.board)
                if board in moves:
                    continue
                else:
                    moves[board] = [move]
                if new_ele.is_solved():
                    while new_ele.board != self.board:
                        new_board = tuple(tuple(x) for x in new_ele.board)
                        solution = moves[new_board] + solution
                        new_ele.perform_move(solution[0][0], solution[0][1])
                    return solution
                frontier.append(new_ele)
        return None

def create_puzzle(rows, cols):
    res = LightsOutPuzzle([[False for x in range(cols)] for y in range(rows)])
    return res

############################################################
# Section 3: Linear Disk Movement
############################################################
def make_identical_disks(length, n):
    return [True if i < n else False for i in range(length)]

def make_distinct_disks(length, n):
    return [True if i < n else False for i in range(length)]

def make_identical_sol(length, n):
    return [False if i < length - n else True for i in range(length)]

def make_distinct_sol(length, n):
    return [False if i < length - n else True for i in range(length)]

def successors_identical(length, board, n):
    for x in range(length):
        if not board[x]:
            continue
        prev_board = board[:]
        if x + 1 < length and board[x+1] and x + 2 < length and not board[x+2]:
            board[x], board[x+2] = board[x+2], board[x]
            yield ((x, x+2), board)
            board = prev_board[:]
        if x + 1 < length-1 and not board[x+1]:
            board[x],board[x+1] = board[x+1],board[x]
            yield((x, x+1), board)
            board = prev_board[:]

def successors_distinct(length, board):
    for x in range(length):
        if not board[x]:
            continue
        prev_board = board[:]
        if x + 1 < length and board[x+1] and x + 2 < length and not board[x+2]:
            board[x], board[x+2] = board[x+2], board[x]
            yield ((x, x+2), board)
            board = prev_board[:]
        if x + 1 < length and not board[x+1]:
            board[x], board[x+1] = board[x+1], board[x]
            yield ((x, x+1), board)
            board = prev_board[:]


def solve_identical_disks(length, n):
    board = make_distinct_disks(length, n)
    sol = make_distinct_sol(length, n)
    if length == 0 or board == sol:
        return []

    frontier = deque([([], board)])
    frontier_set = set()
    visited_set = set()
    while frontier:
        move, brd = frontier.pop();
        visited_set.add(tuple(brd))
        for (new_move, new_brd) in successors_identical(length, brd, n):
            result = move + [new_move]
            if (board == sol):
                return result
            current = (result, new_brd)
            if tuple(new_brd) not in frontier_set and tuple(new_brd) not in visited_set:
                frontier.append(current)
                frontier_set.add(tuple(new_brd))
    return result

def solve_distinct_disks(length, n):
    board = make_distinct_disks(length, n)
    sol = make_distinct_sol(length, n)
    if length == 0 or board == sol:
        return []

    frontier = deque([([], board)])
    frontier_set = set()
    visited_set = set()
    while frontier:
        move, brd = frontier.pop();
        for (new_move, new_brd) in successors_distinct(length, brd):
            result = move + [new_move]
            if (new_brd == sol):
                return result
            current = (result, new_brd)
            if tuple(new_brd) not in visited_set and tuple(new_brd) not in frontier_set:
                frontier.append(current)
                frontier_set.add(tuple(new_brd))
    return result
