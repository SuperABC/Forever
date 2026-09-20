#include "puzzle_basic.h"

using namespace std;

int PuzzleBasic::count = 0;

PuzzleBasic::PuzzleBasic() : id(count++) {
}

const char* PuzzleBasic::GetName() {
	name = "PuzzleBasic" + to_string(id);
	return name.data();
}
