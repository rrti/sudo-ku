#ifndef _GRID_HDR_
#define _GRID_HDR_

#include <vector>

class Timer;
class Grid {
public:
	Grid();
	~Grid();

	bool Load(const char*);
	void Solve(unsigned int, unsigned int);
	void Print() const;

	bool IsSolved() const { return (M == Z); }
	bool SolveExited() const { return exitSolve; }
	void ExitSolve() { exitSolve = true; }
	double GetTime() const { return solveTime; }
	unsigned int GetIters() const { return I; }

private:
	unsigned int GetCellIndexBFS(unsigned int, unsigned int, bool);
	bool IsLegalCellValue(unsigned int row, unsigned int col, unsigned int val) const;
	bool InsertCellValue(unsigned int row, unsigned int col, unsigned int val);
	bool RemoveCellValue(unsigned int row, unsigned int col, unsigned int val);
	void InsertRawCellValue(unsigned int row, unsigned int col, unsigned int val);
	void RemoveRawCellValue(unsigned int row, unsigned int col, unsigned int val);

	unsigned int M; // number of cell-values filled in so far
	unsigned int K; // puzzle size (K=3: N=3x3 blocks, Z=3x3x3x3 cells)
	unsigned int N; // K * K (number of rows, columns, and blocks)
	unsigned int Z; // N * N (number of cells)
	unsigned int I; // number of iterations so far

	bool exitSolve;
	double solveTime;

	std::vector< std::vector<unsigned char> > legalRowValues;
	std::vector< std::vector<unsigned char> > legalColValues;
	std::vector< std::vector<unsigned char> > legalBlkValues;

	// for each non-given cell <c>, these store all its legal values
	// (calculated for the initial grid configuration) and the index
	// into <legalCellValues> of the most recently tested value
	std::vector< std::vector<unsigned int> > legalCellValues;
	std::vector<             unsigned int  > legalCellIdices;

	std::vector< unsigned  int> cells;
	std::vector< unsigned char> hints;

	// these store the indices of each choicepoint cell
	// encountered and the indices of all cells touched
	// since the first choicepoint
	std::vector<unsigned int> choiceCellIndicesStack;
	std::vector<unsigned int> bufferCellIndicesStack;

	Timer* timer;
};

#endif
