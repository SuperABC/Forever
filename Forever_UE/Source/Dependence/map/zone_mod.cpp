#include "zone_mod.h"

using namespace std;

void ZoneMod::AddDecoration(const DecorationSpec& spec) {
	if (spec.meshPath.empty()) return;
	decorations.push_back(spec);
}

void ZoneMod::AddTreeSingle(const PointParams& position, float scale, const string& meshPath) {
	trees.AddSingle(position, scale, meshPath);
}

void ZoneMod::AddTreeLine(const PointParams& from, const PointParams& to, int num, float scale,
	const string& meshPath) {
	trees.AddLine(from, to, num, scale, meshPath);
}

void ZoneMod::AddTreeArea(const PointParams& from, const PointParams& to, int num, float scale,
	const string& meshPath) {
	trees.AddArea(from, to, num, scale, meshPath);
}
