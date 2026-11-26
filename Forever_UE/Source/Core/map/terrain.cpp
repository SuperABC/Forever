#include "terrain.h"

#include "common/random.h"
#include "common/error.h"

#include <algorithm>
#include <cmath>
#include <map>


using namespace std;

Terrain::Terrain(TerrainFactory* factory, const string& terrainId) :
	mod(factory->CreateTerrain(terrainId)),
	factory(factory),
	type(),
	name() {
	if (!mod) {
		THROW_EXCEPTION(DataException, "Terrain " + terrainId + " mod is null.\n");
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

TerrainMod* Terrain::GetMod() const {
	return mod;
}

float Terrain::GetPriority() const {
	return mod->GetPriority();
}

void Terrain::SetupTexture() const {
	RandomScope scope(RandomRegistry::GetInstanceStream(mod));
	MOD_CALL(mod, mod->SetupTexture());
}

TerrainMapColor Terrain::GetMapColor() const {
	TerrainMapColor color;
	color.has = mod->hasMapColor;
	color.r = mod->mapColorR;
	color.g = mod->mapColorG;
	color.b = mod->mapColorB;
	return color;
}

TerrainMaterialSpec Terrain::GetMaterialSpec() const {
	return mod->materialSpec;
}

void Terrain::DistributeTerrain(int width, int height,
	const function<const string&(int, int)>& getTerrain, const function<bool(int, int, const string&)>& setTerrain,
	const function<float(int, int)>& getHeight, const function<bool(int, int, float)>& setHeight,
	const function<bool(int, int, bool, float)>& setWater,
	const function<bool(int, int, const float*)>& setFineHeights) const {
	RandomScope scope(RandomRegistry::GetInstanceStream(mod));
	MOD_CALL(mod, mod->DistributeTerrain(width, height, getTerrain, setTerrain, getHeight, setHeight, setWater, setFineHeights));
}
