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
