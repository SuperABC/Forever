#include "roadnet_mod.h"

#include <cmath>


using namespace std;

void RoadnetMod::AddHatch(const Connection* connection, float t1, float t2, float width) {
	if (!connection) return;

	Node p1 = connection->GetPoint(t1);
	Node p2 = connection->GetPoint(t2);

	float dx = p2.GetX() - p1.GetX();
	float dy = p2.GetY() - p1.GetY();
	float length = connection->CalcDistance(t1, t2);
	if (length <= 0.f) return;

	float rotation = atan2(dy, dx);
	float cx = (p1.GetX() + p2.GetX()) * 0.5f;
	float cy = (p1.GetY() + p2.GetY()) * 0.5f;

	Quad quad(cx, cy, length, width);
	hatches.emplace_back(quad, rotation);
}
