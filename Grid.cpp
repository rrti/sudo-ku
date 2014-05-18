#include <cassert>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>

#include "Grid.hpp"
#include "Timer.hpp"

#define SOLVE_BFS 1
#define ROW_BLOCK_INDEX(r) ((r) / K)
#define COL_BLOCK_INDEX(c) ((c) / K)
#define BLOCK_INDEX(r, c) (ROW_BLOCK_INDEX(r) * K + COL_BLOCK_INDEX(c))

Grid::Grid(): M(0), K(0), N(0), Z(0), I(0) {
	exitSolve = false;
	solveTime = 0.0;

	timer = new Timer();
}

Grid::~Grid() {
	for (unsigned int n = 0; n < N; n++) {
		legalRowValues[n].clear();
		legalColValues[n].clear();
		legalBlkValues[n].clear();
	}
	for (unsigned int z = 0; z < Z; z++) {
		legalCellValues[z].clear();
	}

	legalRowValues.clear();
	legalColValues.clear();
	legalBlkValues.clear();

	legalCellValues.clear();
	legalCellIdices.clear();

	cells.clear();
	hints.clear();

	choiceCellIndicesStack.clear();
	bufferCellIndicesStack.clear();

	delete timer;
}


bool Grid::Load(const char* fname) {
	std::fstream f(fname, std::ios::in);

	if (!f.good())
		return false;

	// first character indicates puzzle size
	K = f.get() - 48;
	N = K * K;
	Z = N * N;

	if (K < 1 || K > 9)
		return false;

	legalRowValues.resize(N, std::vector<unsigned char>(N, 1));
	legalColValues.resize(N, std::vector<unsigned char>(N, 1));
	legalBlkValues.resize(N, std::vector<unsigned char>(N, 1));

	legalCellValues.resize(Z, std::vector<unsigned int>());
	legalCellIdices.resize(Z, -1U);

	cells.resize(Z, 0);
	hints.resize(Z, 0);

	choiceCellIndicesStack.reserve(Z);
	bufferCellIndicesStack.reserve(Z);

	unsigned int row = 0, col = 0;
	unsigned int val = 0, ret = 0;

	while (!f.eof()) {
		val = f.get();

		if (col == N) {
			// wrap-around to new row
			row += 1;
			col  = 0;
		}

		if (row == N) {
			// all rows processed
			break;
		}

		if (val == '.') {
			// empty cell, skip
			col += 1;
			continue;
		}

		if (std::isdigit(val)) {
			// assume all characters are from the standard ASCII-128
			// table (ie. '0' maps to 48d, '1' maps to 49d, etcetera)
			val -= 48;
			ret = (val >= 1 && val <= N);
			ret = (ret != 0 && InsertCellValue(row, col, val));

			// guard against unsolvable grids
			if (ret == 0) {
				printf("[%s] illegal value (%u) at <row=%u, col=%u>\n", __FUNCTION__, val, row, col);
				break;
			} else {
				hints[row * N + col] = 1;
			}

			legalCellValues[(row * N) + (col++)].reserve(N);
		}
	}

	f.close();
	return (ret != 0);
}


unsigned int Grid::GetCellIndexBFS(unsigned int threadNum, unsigned int numThreads, bool startSolve) {
	unsigned int minNumLegalCellValues = N;
	unsigned int minLegalCellValuesIdx = 0;

	// for each cell, calculate its degrees of freedom
	// (number of legal values) and start solving the
	// grid at the cell that has minimal freedom
	for (unsigned int row = 0; row < N; row++) {
		for (unsigned int col = 0; col < N; col++) {
			const unsigned int idx = row * N + col;

			if (hints[idx] != 0)
				continue;
			if (cells[idx] != 0)
				continue;

			unsigned int numLegalCellValues = 0;

			if (startSolve) {
				for (unsigned int val = 1; val <= N; val++) {
					if (IsLegalCellValue(row, col, val)) {
						legalCellValues[idx].push_back(val);
					}
				}

				numLegalCellValues = legalCellValues[idx].size();
			} else {
				for (unsigned int valIdx = 0; valIdx < legalCellValues[idx].size(); valIdx++) {
					if (IsLegalCellValue(row, col, legalCellValues[idx][valIdx])) {
						numLegalCellValues += 1;
					}
				}
			}

			if (numLegalCellValues < minNumLegalCellValues) {
				minNumLegalCellValues = numLegalCellValues;
				minLegalCellValuesIdx = idx;
			}
		}
	}

	if (startSolve) {
		assert(minNumLegalCellValues > 0);

		if (numThreads <= minNumLegalCellValues) {
			std::vector<unsigned int> values = legalCellValues[minLegalCellValuesIdx];

			const unsigned int valsPerThread = (minNumLegalCellValues / numThreads);
			const unsigned int valsRemainder = (minNumLegalCellValues % numThreads) * ((threadNum == (numThreads - 1))? 1: 0);
			const unsigned int minValueIndex = threadNum * valsPerThread;
			const unsigned int maxValueIndex = minValueIndex + valsPerThread + valsRemainder;

			legalCellValues[minLegalCellValuesIdx].clear();
			legalCellValues[minLegalCellValuesIdx].reserve(valsPerThread);

			for (unsigned int n = minValueIndex; n < maxValueIndex; n++) {
				legalCellValues[minLegalCellValuesIdx].push_back(values[n]);
			}
		} else {
			if (threadNum < minNumLegalCellValues) {
				std::vector<unsigned int> values = legalCellValues[minLegalCellValuesIdx];

				legalCellValues[minLegalCellValuesIdx].clear();
				legalCellValues[minLegalCellValuesIdx].push_back(values[threadNum]);
			} else {
				// nothing to do for this thread, just trigger an early exit
				// (excess threads could start at cells with higher DOF's?)
				legalCellValues[minLegalCellValuesIdx].clear();
			}
		}
	}

	// note: minNumLegalCellValues can be zero while solving
	// if the search went down a wrong path, in which case we
	// start backtracking (the next iteration of ::Solve will
	// detect that numLegalValues == 0)
	return minLegalCellValuesIdx;
}

void Grid::Solve(unsigned int threadNum, unsigned int numThreads) {
	timer->Tick();

	unsigned int cellIdx = GetCellIndexBFS(threadNum, numThreads, true);
	unsigned int cellRow  = 0;
	unsigned int cellCol  = 0;

	while (!exitSolve) {
		if (M == Z)
			break;
		if (cellIdx == Z)
			cellIdx = 0;

		// skip cells with preloaded values
		// (only necessary if SOLVE_BFS==0)
		if (hints[cellIdx] != 0) {
			cellIdx++;
		} else {
			const unsigned int cellVal = cells[cellIdx];

			unsigned int numLegalValues   = 0;
			unsigned int minLegalValue    = N;
			unsigned int minLegalValueIdx = N - 1;

			cellRow = cellIdx / N;
			cellCol = cellIdx % N;

			// if this cell contains a value, it was a
			// choicepoint during a previous iteration
			if (cellVal != 0) {
				RemoveRawCellValue(cellRow, cellCol, cellVal);
			}

			{
				// determine how many values are still allowed in this cell
				const unsigned int minValIdx = legalCellIdices[cellIdx] + 1;
				const unsigned int maxValIdx = legalCellValues[cellIdx].size();

				for (unsigned int valIdx = minValIdx; valIdx < maxValIdx; valIdx++) {
					const unsigned int val = legalCellValues[cellIdx][valIdx];

					if (IsLegalCellValue(cellRow, cellCol, val)) {
						numLegalValues   += 1;
						minLegalValue     = std::min(minLegalValue,    val   );
						minLegalValueIdx  = std::min(minLegalValueIdx, valIdx);
					}
				}
			}

			if (numLegalValues == 0) {
				// if zero freedom, backtrack to last point of choice
				// (if no such point, then a solution does not exist)
				if (choiceCellIndicesStack.empty())
					break;

				while (bufferCellIndicesStack.back() != choiceCellIndicesStack.back()) {
					// reset to -1U so that next index tested will be 0 again
					legalCellIdices[ bufferCellIndicesStack.back() ] = -1U;

					cellRow = bufferCellIndicesStack.back() / N;
					cellCol = bufferCellIndicesStack.back() % N;

					// restore cells touched since the last choicepoint
					RemoveRawCellValue(cellRow, cellCol, cells[bufferCellIndicesStack.back()]);

					bufferCellIndicesStack.pop_back();
				}

				// move back to the last choicepoint
				cellIdx = choiceCellIndicesStack.back();

				bufferCellIndicesStack.pop_back();
				choiceCellIndicesStack.pop_back();
			} else {
				if (numLegalValues > 1) {
					// if more than one DOF, we have a choice
					// (and hence want to remember this index
					// for backtracking)
					choiceCellIndicesStack.push_back(cellIdx);
				}

				// pick the minimum still-allowed value
				InsertRawCellValue(cellRow, cellCol, minLegalValue);

				// store the index of the value we are about to test
				legalCellIdices[cellIdx] = minLegalValueIdx;

				// save the index of every cell we touch
				// (needed to deal with index wraparound)
				// and proceed to the next
				bufferCellIndicesStack.push_back(cellIdx);

				#if (SOLVE_BFS == 1)
				// "best-first" search
				cellIdx = GetCellIndexBFS(threadNum, numThreads, false);
				#else
				// "brute-force" search
				cellIdx++;
				#endif
			}
		}

		I++;
	}

	timer->Tock();

	solveTime = timer->Time();
	exitSolve = true;
}




bool Grid::IsLegalCellValue(unsigned int row, unsigned int col, unsigned int val) const {
	assert(cells[row * N + col] == 0);

	// value is not allowed in row
	if (legalRowValues[row][val - 1] == 0)
		return false;
	// value is not allowed in colum
	if (legalColValues[col][val - 1] == 0)
		return false;
	// value is not allowed in block
	if (legalBlkValues[BLOCK_INDEX(row, col)][val - 1] == 0)
		return false;

	return true;
}


bool Grid::InsertCellValue(unsigned int row, unsigned int col, unsigned int val) {
	if (!IsLegalCellValue(row, col, val))
		return false;

	InsertRawCellValue(row, col, val);
	return true;
}

bool Grid::RemoveCellValue(unsigned int row, unsigned int col, unsigned int val) {
	if (IsLegalCellValue(row, col, val))
		return false;

	RemoveRawCellValue(row, col, val);
	return true;
}


void Grid::InsertRawCellValue(unsigned int row, unsigned int col, unsigned int val) {
	cells[row * N + col] = val;

	legalRowValues[row][val - 1] = 0;
	legalColValues[col][val - 1] = 0;
	legalBlkValues[BLOCK_INDEX(row, col)][val - 1] = 0;

	M += 1;
}

void Grid::RemoveRawCellValue(unsigned int row, unsigned int col, unsigned int val) {
	cells[row * N + col] = 0;

	legalRowValues[row][val - 1] = 1;
	legalColValues[col][val - 1] = 1;
	legalBlkValues[BLOCK_INDEX(row, col)][val - 1] = 1;

	M -= 1;
}




void Grid::Print() const {
	const unsigned int width = static_cast<unsigned int>(std::log(N) / std::log(10.0)) + 1;

	std::stringstream str; str << "";
	std::stringstream div; div << "+";
	std::ostream& out = std::cout;

	// compose the divider string
	for (unsigned int col = 0; col < N; col++) {
		div << "-";

		div.width(width);
		div.fill('-');
		div << "-";

		div << "-";

		if ((col < N - 1) && ((col + 1) % K) == 0) {
			div << "+";
		}
	}

	div << "+\n";


	// compose the grid string
	for (unsigned int row = 0; row < N; row++) {
		if ((row % K) == 0) {
			str << div.str();
		}

		str << "|";

		for (unsigned int col = 0; col < N; col++) {
			const unsigned int val = cells[row * N + col];

			if (val == 0) {
				str << " ";

				str.width(width);
				str.fill('.');
				str << ".";

				str << " ";
			} else {
				str << " "; str.width(width);
				str << val; str.width(    1);
				str << " ";
			}

			if ((col < N - 1) && ((col + 1) % K) == 0) {
				str << "|";
			}
		}

		str << "|\n";
	}

	str << div.str();
	out << str.str();
}
