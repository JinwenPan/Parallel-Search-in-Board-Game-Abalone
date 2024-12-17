/**
 * Computer player
 *
 * (1) Connects to a game communication channel,
 * (2) Waits for a game position requiring to draw a move,
 * (3) Does a best move search, and broadcasts the resulting position,
 *     Jump to (2)
 *
 * (C) 2005-2015, Josef Weidendorfer, GPLv2+
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "board.h"
#include "search.h"
#include "eval.h"
#include "network.h"

#include <mpi.h>

/* Global, static vars */
NetworkLoop l;
Board myBoard;
Evaluator ev;

/* Which color to play? */
int myColor = Board::color1;

/* Which search strategy to use? */
int strategyNo = 0;

/* Max search depth */
int maxDepth = 0;

/* Maximal number of moves before terminating (negative for infinity) */
int maxMoves = -1;

/* to set verbosity of NetworkLoop implementation */
extern int verbose;

/* remote channel */
char *host = 0; /* not used on default */
int rport = 23412;

/* local channel */
int lport = 23412;

/* change evaluation after move? */
bool changeEval = true;

MPI_Datatype mpiBoard;
int rank, size;

/*** Define the opening moves ***/
bool opening = true;
int count_moves = 0;
// For X:
Move move1X = Move(108, Move::LeftUp, Move::move3); // I9/LeftUp
Move move2X = Move(106, Move::LeftUp, Move::move3); // I7/LeftUp
Move move3X = Move(107, Move::LeftUp, Move::move3); // I8/LeftUP
Move move4X = Move(105, Move::LeftUp, Move::move2); // I6/LeftUP
Move move5X = Move(95, Move::LeftUp, Move::push2);	// H7/LeftUp/Push

Move openings_X[5] = {move1X, move2X, move3X, move4X, move5X};

// For O:
Move move1O = Move(12, Move::RightDown, Move::move3);  // A1/RightDown
Move move2O = Move(13, Move::RightDown, Move::move3);  // A2/RightDown
Move move3O = Move(14, Move::RightDown, Move::move3);  // A3/RightDown
Move move4O = Move(27, Move::LeftDown, Move::move3);   // B5/LeftDown
Move move5O = Move(27, Move::RightDown, Move::right2); //

Move openings_O[5] = {move1O, move2O, move3O, move4O, move5O};

/**
 * MyDomain
 *
 * Class for communication handling for player:
 * - start search for best move if a position is received
 *   in which this player is about to draw
 */
class MyDomain : public NetworkDomain
{
public:
	MyDomain(int p) : NetworkDomain(p) { sent = 0; }

	void sendBoard(Board *);

protected:
	void received(char *str);
	void newConnection(Connection *);

private:
	Board *sent;
};

void MyDomain::sendBoard(Board *b)
{
	if (b)
	{
		static char tmp[500];
		sprintf(tmp, "pos %s\n", b->getState());
		if (verbose)
			printf("%s", tmp + 4);
		broadcast(tmp);
	}
	sent = b;
}

void MyDomain::received(char *str)
{
	if (strncmp(str, "quit", 4) == 0)
	{
		l.exit();
		return;
	}

	if (strncmp(str, "pos ", 4) != 0)
		return;

	// on receiving remote position, do not broadcast own board any longer
	sent = 0;

	myBoard.setState(str + 4);

	if (verbose)
	{
		printf("\n\n==========================================\n%s", str + 4);
	}

	int state = myBoard.validState();
	if ((state != Board::valid1) && (state != Board::valid2))
	{
		printf("%s\n", Board::stateDescription(state));
		switch (state)
		{
		case Board::timeout1:
		case Board::timeout2:
		case Board::win1:
		case Board::win2:
			l.exit();
		default:
			break;
		}
		return;
	}
	// printf("***********************************************\n");
	// printf("current board: \n");
	// printf("%s\n", myBoard.getState());
	// printf("***********************************************\n");

	if (myBoard.actColor() & myColor)
	{
		struct timeval t1, t2;

		gettimeofday(&t1, 0);

		// This is where we actually want to start the parallelism!
		Move m;
		if (size > 1)
		{

			MoveList list = MoveList();
			myBoard.generateMoves(list);
			int listLength = list.getLength();
			Move FirstLayerMoves[listLength]; // here we store the first layer moves
			Move DummyMove;					  // to iterate the list
			int eval_vector[listLength];	  // here we store the evals of the first layer moves
			int eval;
			int k = 0;
			char buffer[size - 1][sizeof(Board)];
			MPI_Request request;

			if (opening)
			{
				if (myColor == Board::color1)
				{ // O
					m = openings_O[count_moves];
				}
				else if (myColor == Board::color2)
				{ // X
					m = openings_X[count_moves];
				}

				if (!list.isElement(m, 0, false) || count_moves >= 5)
				{
					opening = false;
				}
				count_moves++;
			}

			if (opening)
			{
			}
			else
			{
				while (k < size - 1 && k < listLength)
				{
					list.getNext(DummyMove);
					FirstLayerMoves[k] = DummyMove;
					myBoard.playMove(DummyMove, 0);
					myBoard.pack(buffer[k]);
					myBoard.takeBack();

					MPI_Isend(&buffer[k], myBoard.size(), MPI_BYTE, k + 1, k, MPI_COMM_WORLD, &request);
					k++;
				}
				// at this point we have sent size-1 boards to workers, or all boards if there are less than size-1 boards

				int recieved_evals = 0; // counter for recieved evals

				MPI_Status status; // in status we will recive the source and the value of k

				// Distribute the remaining boards dynamically, whoever finishes first will get the next board
				// this only happens if there are more boards than workers
				while (list.getNext(DummyMove))
				{
					// recive eval from any source
					MPI_Recv(&eval, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
					recieved_evals++;
					int sender_k = status.MPI_TAG;		 // tells us where to store the eval
					int sender_rank = status.MPI_SOURCE; // next board will be sent to this rank
					eval_vector[sender_k] = eval;

					// we will use buffer[sender_rank-1] since getting the recieve means that the first sent completed
					// Send the next board to the worker that just finished
					FirstLayerMoves[k] = DummyMove;
					myBoard.playMove(DummyMove, 0);
					myBoard.pack(buffer[sender_rank - 1]);
					myBoard.takeBack();
					MPI_Isend(&buffer[sender_rank - 1], myBoard.size(), MPI_BYTE, sender_rank, k, MPI_COMM_WORLD, &request);
					k++;
				}

				// recive the remaining evals
				while (recieved_evals < listLength)
				{
					MPI_Recv(&eval, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
					// status.MPI_TAG is the first layer index (k) tells us where to store the eval
					eval_vector[status.MPI_TAG] = eval;
					recieved_evals++;
				}

				// terminate workers
				for (int i = 1; i < size; i++)
				{
					// tag 1000 tells the worker to terminate
					MPI_Send(&buffer[0], 1, MPI_BYTE, i, 1000, MPI_COMM_WORLD); //, &requests2[i - 1]);
				}

				// check for best eval
				int bestEval = -9999999;
				int bestj = 0;
				for (int j = 0; j < listLength; j++)
				{
					if (eval_vector[j] > bestEval)
					{
						bestEval = eval_vector[j];
						bestj = j;
					}
				}
				m = FirstLayerMoves[bestj];

				// MPI_Waitall(size - 1, requests2, MPI_STATUSES_IGNORE);
			}
		}
		else // single process version
		{
			m = myBoard.bestMove();
		}

		gettimeofday(&t2, 0);

		int msecsPassed =
			(1000 * t2.tv_sec + t2.tv_usec / 1000) -
			(1000 * t1.tv_sec + t1.tv_usec / 1000);

		printf("%s ", (myColor == Board::color1) ? "O" : "X");
		if (m.type == Move::none)
		{
			printf(" can not draw any move ?! Sorry.\n");
			return;
		}
		printf("draws '%s' (after %d.%03d secs)...\n", m.name(), msecsPassed / 1000, msecsPassed % 1000);

		myBoard.playMove(m, msecsPassed);

		sendBoard(&myBoard);

		if (changeEval)
			ev.changeEvaluation();

		/* stop player at win position */
		int state = myBoard.validState();
		if ((state != Board::valid1) &&
			(state != Board::valid2))
		{
			printf("%s\n", Board::stateDescription(state));
			switch (state)
			{
			case Board::timeout1:
			case Board::timeout2:
			case Board::win1:
			case Board::win2:
				l.exit();
			default:
				break;
			}
		}

		maxMoves--;
		if (maxMoves == 0)
		{
			printf("Terminating because given number of moves drawn.\n");
			broadcast("quit\n");
			l.exit();
		}
	}
}

void processWork() // only called if size > 1
{

	while (1) // infinite loop
	{
		int eval;
		char buffer[sizeof(Board)];
		Board b;
		Move m;
		MPI_Status status;
		MPI_Recv(&buffer, b.size(), MPI_BYTE, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		// tag holds the index of the move in the first layer, or a terminate signal
		if (status.MPI_TAG == 1000) // tag 1000 tells the worker to terminate
		{
			return;
		}
		else
		{
			b.unpack(buffer);

			if (!b.isValid()) // win condition, no need to search
			{
				eval = 14999;
				MPI_Send(&eval, 1, MPI_INT, 0, status.MPI_TAG, MPI_COMM_WORLD);
			}
			else
			{
				m = b.bestMove();
				eval = -1 * b._BOARD_bestEval;

				MPI_Send(&eval, 1, MPI_INT, 0, status.MPI_TAG, MPI_COMM_WORLD);
			}
		}
	}
}

void MyDomain::newConnection(Connection *c)
{
	NetworkDomain::newConnection(c);

	if (sent)
	{
		static char tmp[500];
		int len = sprintf(tmp, "pos %s\n", sent->getState());
		c->sendString(tmp, len);
	}
}

/*
 * Main program
 */

void printHelp(char *prg, bool printHeader)
{
	if (printHeader)
		printf("Computer player V 0.2\n"
			   "Search for a move on receiving a position in which we are expected to draw.\n\n");

	printf("Usage: %s [options] [X|O] [<strength>]\n\n"
		   "  X                Play side X\n"
		   "  O                Play side O (default)\n"
		   "  <strength>       Playing strength, depending on strategy\n"
		   "                   A time limit can reduce this\n\n",
		   prg);
	printf(" Options:\n"
		   "  -h / --help      Print this help text\n"
		   "  -v / -vv         Be verbose / more verbose\n"
		   "  -s <strategy>    Number of strategy to use for computer (see below)\n"
		   "  -n               Do not change evaluation function after own moves\n"
		   "  -<integer>       Maximal number of moves before terminating\n"
		   "  -p [host:][port] Connection to broadcast channel\n"
		   "                   (default: 23412)\n\n");

	printf(" Available search strategies for option '-s':\n");

	const char **strs = SearchStrategy::strategies();
	for (int i = 0; strs[i]; i++)
		printf("  %2d : Strategy '%s'%s\n", i, strs[i],
			   (i == strategyNo) ? " (default)" : "");
	printf("\n");

	exit(1);
}

void parseArgs(int argc, char *argv[])
{
	int arg = 0;
	while (arg + 1 < argc)
	{
		arg++;
		if (strcmp(argv[arg], "-h") == 0 ||
			strcmp(argv[arg], "--help") == 0)
			printHelp(argv[0], true);
		if (strncmp(argv[arg], "-v", 2) == 0)
		{
			verbose = 1;
			while (argv[arg][verbose + 1] == 'v')
				verbose++;
			continue;
		}
		if (strcmp(argv[arg], "-n") == 0)
		{
			changeEval = false;
			continue;
		}
		if ((strcmp(argv[arg], "-s") == 0) && (arg + 1 < argc))
		{
			arg++;
			if (argv[arg][0] >= '0' && argv[arg][0] <= '9')
				strategyNo = argv[arg][0] - '0';
			continue;
		}

		if ((argv[arg][0] == '-') &&
			(argv[arg][1] >= '0') &&
			(argv[arg][1] <= '9'))
		{
			int pos = 2;

			maxMoves = argv[arg][1] - '0';
			while ((argv[arg][pos] >= '0') &&
				   (argv[arg][pos] <= '9'))
			{
				maxMoves = maxMoves * 10 + argv[arg][pos] - '0';
				pos++;
			}
			continue;
		}

		if ((strcmp(argv[arg], "-p") == 0) && (arg + 1 < argc))
		{
			arg++;
			if (argv[arg][0] > '0' && argv[arg][0] <= '9')
			{
				lport = atoi(argv[arg]);
				continue;
			}
			char *c = strrchr(argv[arg], ':');
			int p = 0;
			if (c != 0)
			{
				*c = 0;
				p = atoi(c + 1);
			}
			host = argv[arg];
			if (p)
				rport = p;
			continue;
		}

		if (argv[arg][0] == 'X')
		{
			myColor = Board::color2;
			continue;
		}
		if (argv[arg][0] == 'O')
		{
			myColor = Board::color1;
			continue;
		}

		int strength = atoi(argv[arg]);
		if (strength == 0)
		{
			printf("ERROR - Unknown option %s\n", argv[arg]);
			printHelp(argv[0], false);
		}

		maxDepth = strength;
	}
}

int main(int argc, char *argv[])
{
	MPI_Init(&argc, &argv);

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	MPI_Type_contiguous(Board::size(), MPI_BYTE, &mpiBoard);
	MPI_Type_commit(&mpiBoard);

	parseArgs(argc, argv);

	SearchStrategy *ss = SearchStrategy::create(strategyNo);
	ss->setMaxDepth(maxDepth);

	myBoard.setSearchStrategy(ss);
	ss->setEvaluator(&ev);
	ss->registerCallbacks(new SearchCallbacks(verbose));

	if (rank == 0)
	{
		printf("Using strategy '%s' (depth %d) ...\n", ss->name(), maxDepth);

		MyDomain d(lport);
		l.install(&d);

		if (host)
			d.addConnection(host, rport);

		l.run();
	}
	else
	{
		while (maxMoves != 0)
		{
			processWork();
		}
		// printf("Rank %d finished working \n", rank);
	}
	MPI_Finalize();
}
