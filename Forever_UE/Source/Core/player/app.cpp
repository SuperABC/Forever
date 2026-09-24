#include "app.h"

#include "common/error.h"


using namespace std;

App::App(AppFactory* factory, const string& id) :
	factory(factory),
	mod(factory->CreateApp(id)) {
	if (!mod) {
		THROW_EXCEPTION(NullPointerException, "App " + id + " mod is null.\n");
	}
}

App::~App() {
	factory->DestroyApp(mod);
}

string App::GetType() const {
	return mod->GetType();
}

string App::GetName() const {
	return mod->GetName();
}

void App::Init(Canvas* canvas, PostHandle* post) {
	mod->Init(canvas, post);
}

void App::Loop(Canvas* canvas, int ms, PostHandle* post) {
	mod->Loop(canvas, ms, post);
}

void App::Back(Canvas* canvas, PostHandle* post) {
	mod->Back(canvas, post);
}

void App::Refresh(Canvas* canvas, PostHandle* post) {
	mod->Refresh(canvas, post);
}
