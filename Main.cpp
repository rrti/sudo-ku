// simple back-tracking Sudoku puzzle solver
// uses best-first search and multiple threads
//
#include <cstdlib>
#include <cstdio>

#ifdef ENABLE_MULTITHREADING
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#endif

#include "Grid.hpp"

#ifndef ENABLE_MULTITHREADING

int main(int argc, char** argv) {
	if (argc == 1) {
		printf("[%s] usage: %s <sudoku.txt>\n", __FUNCTION__, argv[0]);
		return EXIT_FAILURE;
	}

	Grid g;

	if (!g.Load(argv[1])) {
		printf("[%s] unable to open file \"%s\"\n", __FUNCTION__, argv[1]);
		return EXIT_FAILURE;
	}

	printf("[%s] loaded file \"%s\":\n", __FUNCTION__, argv[1]);

	g.Print();
	g.Solve(0, 1);

	if (!g.IsSolved()) {
		printf("[%s] \"%s\" has no solution\n", __FUNCTION__, argv[1]);
		return EXIT_FAILURE;
	}

	printf("[%s] solved \"%s\" (%g seconds, %u iterations):\n", __FUNCTION__, argv[1], g.GetTime(), g.GetIters());
	g.Print();

	return EXIT_SUCCESS;
}

#else

int main(int argc, const char** argv) {
	using namespace boost;

	enum {
		THREADSTATE_EXITED = 1,
		THREADSTATE_SOLVED = 2,
	};

	if (argc == 1) {
		printf("[%s] usage: %s <sudoku.txt> [numThreads]\n", __FUNCTION__, argv[0]);
		return EXIT_FAILURE;
	}

	const char* gridFile = argv[1];
	const unsigned int numThreads = (argc == 3)? std::max(1, atoi(argv[2])): 2;

	unsigned int exitedThreads = 0;
	unsigned int solvedThreads = 0;

	std::vector<thread*> threads(numThreads, NULL);
	std::vector<Grid*> grids(numThreads, NULL);
	std::vector<unsigned int> states(numThreads, 0);

	{
		Grid grid;

		if (!grid.Load(gridFile)) {
			printf("[%s] unable to open file \"%s\"\n", __FUNCTION__, gridFile);
			return EXIT_FAILURE;
		}

		printf("[%s] loaded file \"%s\":\n", __FUNCTION__, gridFile);
		grid.Print();
	}

	for (unsigned int threadNum = 0; threadNum < numThreads; threadNum++) {
		grids[threadNum] = new Grid();
		grids[threadNum]->Load(gridFile);
		threads[threadNum] = new thread(bind(&Grid::Solve, grids[threadNum], threadNum, numThreads));
	}


	// busy-loop; we cannot wait for an "is solved" condition
	// since the grid might actually be unsolvable (as given)
	// instead, wait until either all threads have exited or
	// one has succesfully solved its grid
	while (exitedThreads != numThreads && solvedThreads == 0) {
		usleep(1000);

		for (unsigned int threadNum = 0; threadNum < numThreads; threadNum++) {
			const Grid* g = grids[threadNum];

			if (!g->SolveExited()) { continue; }
			if ((states[threadNum] & THREADSTATE_EXITED) != 0) { continue; }

			states[threadNum] |= THREADSTATE_EXITED;
			exitedThreads += 1;

			if (!g->IsSolved()) { continue; }
			if ((states[threadNum] & THREADSTATE_SOLVED) != 0) { continue; }

			states[threadNum] |= THREADSTATE_SOLVED;
			solvedThreads += 1;

			printf("[%s][thread %u] solved \"%s\" (%g seconds, %u iterations):\n", __FUNCTION__, threadNum, gridFile, g->GetTime(), g->GetIters());
			g->Print();

			// we only care about the first solution
			break;
		}
	}

	for (unsigned int threadNum = 0; threadNum < numThreads; threadNum++) {
		grids[threadNum]->ExitSolve();
		threads[threadNum]->join();

		delete threads[threadNum];
		delete grids[threadNum];
	}

	if (solvedThreads == 0) {
		printf("[%s] \"%s\" has no solution\n", __FUNCTION__, gridFile);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

#endif
