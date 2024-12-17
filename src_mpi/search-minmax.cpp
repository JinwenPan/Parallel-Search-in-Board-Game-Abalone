/**
 * Very simple example strategy:
 * Search all possible positions reachable via one move,
 * and return the move leading to best position
 *
 * (c) 2006, Josef Weidendorfer
 */

#include "search.h"
#include "board.h"
#include "eval.h"
#include "stdio.h"

/**
 * To create your own search strategy:
 * - copy this file into another one,
 * - change the class name one the name given in constructor,
 * - adjust clone() to return an instance of your class
 * - adjust last line of this file to create a global instance
 *   of your class
 * - adjust the Makefile to include your class in SEARCH_OBJS
 * - implement searchBestMove()
 *
 * Advises for implementation of searchBestMove():
 * - call foundBestMove() when finding a best move since search start
 * - call finishedNode() when finishing evaluation of a tree node
 * - Use _maxDepth for strength level (maximal level searched in tree)
 */
class MinMaxStrategy : public SearchStrategy
{
public:
    // Defines the name of the strategy
    MinMaxStrategy() : SearchStrategy("MinMax") {}

    // Factory method: just return a new instance of this class
    SearchStrategy *clone() { return new MinMaxStrategy(); }

private:
    /**
     * Implementation of the strategy.
     */
    void searchBestMove();
    void searchkthMove(int);
    int minimax_eval(int depth, Move m);
};

void MinMaxStrategy::searchBestMove()
{
    // we try to maximize bestEvaluation
    int bestEval = minEvaluation();
    int eval;

    Move m;
    MoveList list;

    // generate list of allowed moves, put them into <list>
    generateMoves(list);

    // loop over all moves
    while (list.getNext(m))
    {
        // draw move, evalute, and restore position
        eval = minimax_eval(0, m);

        if (eval > bestEval)
        {
            bestEval = eval;
            foundBestMove(0, m, eval);
        }
    }

    finishedNode(0, 0);
}

void MinMaxStrategy::searchkthMove(int k)
{
    Move m;
    MoveList list;

    _board->generateMoves(list);
    int count = 0;
    while (count < k)
    {
        list.getNext(m);
        count++;
    }

    _kthMove = m;
}

int MinMaxStrategy::minimax_eval(int depth, Move m)
{
    if (depth >= _maxDepth)
    {
        playMove(m);
        int eval = evaluate();
        takeBack();
        finishedNode(depth, 0);
        return eval;
    }
    else
    {
        if (depth % 2 == 1) // max
        {
            // printf("depth %d entered max \n", depth);
            int bestEval = minEvaluation();
            int eval;

            Move m_child;
            MoveList list;

            // play move m
            playMove(m);
            // generate list of allowed moves after m, put them into <list>
            generateMoves(list);

            // loop over all moves
            while (list.getNext(m_child))
            {
                eval = minimax_eval(depth + 1, m_child);

                if (eval > bestEval)
                {
                    bestEval = eval;
                }
            }
            // take back move m,
            takeBack();
            finishedNode(depth, 0);
            return bestEval;
        }
        else //  min
        {
            // printf("depth %d entered min \n", depth);
            int bestEval = maxEvaluation();
            int eval;

            Move m_child;
            MoveList list;

            // play move m
            playMove(m);
            // generate list of allowed moves after m, put them into <list>
            generateMoves(list);

            // loop over all moves
            while (list.getNext(m_child))
            {
                eval = minimax_eval(depth + 1, m_child);

                if (eval < bestEval)
                {
                    bestEval = eval;
                }
            }
            // take back move m,
            takeBack();
            finishedNode(depth, 0);
            return bestEval;
        }
    }
}

// register ourselve as a search strategy
MinMaxStrategy minmaxStrategy;
