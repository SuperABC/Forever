#include "puzzle.h"

#include "common/random.h"
#include "common/error.h"


using namespace std;

Puzzle::Puzzle(PuzzleFactory* factory, const string& id) :
	factory(factory),
	mod(factory->CreatePuzzle(id)) {
	if (!mod) {
		THROW_EXCEPTION(DataException, "Puzzle " + id + " mod is null.\n");
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

void Puzzle::SetParam(const string& param) {
	MOD_CALL(mod, mod->SetParam(param));
}

void Puzzle::Init(Canvas* canvas, PostHandle* post) {
	RandomScope scope(RandomRegistry::GetInstanceStream(mod));
	MOD_CALL(mod, mod->Init(canvas, post));
}

JsonValue* Puzzle::Loop(Canvas* canvas, int ms, PostHandle* post) {
	RandomScope scope(RandomRegistry::GetInstanceStream(mod));
	return MOD_CALL(mod, mod->Loop(canvas, ms, post));
}
