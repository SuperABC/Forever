#include "terrain.h"

#include "common/error.h"

using namespace std;

Terrain::Terrain(TerrainFactory* factory, const string& terrainId) :
	mod(factory->CreateTerrain(terrainId)),
	factory(factory),
	type(),
	name() {
	if (!mod) {
		THROW_EXCEPTION(NullPointerException, "Terrain " + terrainId + " mod is null.\n");
	}

	type = mod->GetType();
	name = mod->GetName();
}

Terrain::~Terrain() {
	factory->DestroyTerrain(mod);
}

string Terrain::GetType() const {
	return type;
}

string Terrain::GetName() const {
	return name;
}

float Terrain::GetPriority() const {
	return mod->GetPriority();
}

void Terrain::SetupTexture() const {
	mod->SetupTexture();
}

string Terrain::GetTexture() const {
	return mod->diffusePath;
}

pair<bool, float> Terrain::GetWater() const {
	return mod->waterHeight;
}

void Terrain::DistributeTerrain(int width, int height,
	const function<string(int, int)>& getTerrain, const function<bool(int, int, string)>& setTerrain,
	const function<float(int, int)>& getHeight, const function<bool(int, int, float)>& setHeight) const {
	mod->DistributeTerrain(width, height, getTerrain, setTerrain, getHeight, setHeight);
}
