#include "puzzle.h"

#include "common/error.h"


using namespace std;

Puzzle::Puzzle(PuzzleFactory* factory, const string& id) :
	factory(factory),
	mod(factory->CreatePuzzle(id)) {
	if (!mod) {
		THROW_EXCEPTION(NullPointerException, "Puzzle " + id + " mod is null.\n");
	}
}

Puzzle::~Puzzle() {
	factory->DestroyPuzzle(mod);
}

string Puzzle::GetType() const {
	return mod->GetType();
}

string Puzzle::GetName() const {
	return mod->GetName();
}

void Puzzle::Init(Canvas* canvas, PostHandle* post) {
	mod->Init(canvas, post);
}

int Puzzle::Loop(Canvas* canvas, int ms, PostHandle* post) {
	return mod->Loop(canvas, ms, post);
}
