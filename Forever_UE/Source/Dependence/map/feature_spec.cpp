#include "feature_spec.h"


using namespace std;

void TreeRequests::AddSingle(const PointParams& position, float scale, const string& meshPath) {
	if (meshPath.empty()) return;
	singles.push_back(TreeSingleSpec{ position, scale, meshPath });
}

void TreeRequests::AddLine(const PointParams& from, const PointParams& to, int num, float scale, const string& meshPath) {
	if (num <= 0 || meshPath.empty()) return;
	lines.push_back(TreeLineSpec{ from, to, num, scale, meshPath });
}

void TreeRequests::AddArea(const PointParams& from, const PointParams& to, int num, float scale, const string& meshPath) {
	if (num <= 0 || meshPath.empty()) return;
	areas.push_back(TreeAreaSpec{ from, to, num, scale, meshPath });
}

void LampRequests::AddSingle(const PointParams& position, float rotationDegrees, const string& blueprintPath) {
	if (blueprintPath.empty()) return;
	singles.push_back(LampSingleSpec{ position, rotationDegrees, blueprintPath });
}

void LampRequests::AddLine(const PointParams& from, const PointParams& to, int num, float rotationDegrees, const string& blueprintPath) {
	if (num <= 0 || blueprintPath.empty()) return;
	lines.push_back(LampLineSpec{ from, to, num, rotationDegrees, blueprintPath });
}

void LampRequests::AddArea(const PointParams& from, const PointParams& to, int num, float rotationDegrees, const string& blueprintPath) {
	if (num <= 0 || blueprintPath.empty()) return;
	areas.push_back(LampAreaSpec{ from, to, num, rotationDegrees, blueprintPath });
}
