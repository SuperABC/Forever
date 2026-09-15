#include "geometry.h"

#include <cmath>
#include <string>
#include <vector>
#include <algorithm>


using namespace std;

int Node::count = 0;

Node::Node(string category, float x, float y, float z) :
	id(count++),
	posX(x),
	posY(y),
	posZ(z),
	category(category) {

}

Node::~Node() {

}

int Node::GetId() const {
	return id;
}

float Node::GetX() const {
	return posX;
}

float Node::GetY() const {
	return posY;
}

float Node::GetZ() const {
	return posZ;
}

string Node::GetCategory() const {
	return category;
}

int Node::GetCount() {
	return count;
}

void Node::SetCount(int c) {
	count = c;
}

Node::Node(const Node& other) :
	id(other.id),
	posX(other.posX),
	posY(other.posY),
	posZ(other.posZ),
	category(other.category) {
	if(count <= other.id) {
		count = other.id + 1;
	}
}

Node& Node::operator=(const Node& other) {
	if (this != &other) {
		posX = other.posX;
		posY = other.posY;
		posZ = other.posZ;
		id = other.id;
		category = other.category;	
		if (count <= other.id) {
			count = other.id + 1;
		}
	}
	return *this;
}

static int Binomial(int n, int k) {
	if (k < 0 || k > n) return 0;
	if (k == 0 || k == n) return 1;
	long long result = 1;
	for (int i = 1; i <= k; i++) {
		result = result * (n - i + 1) / i;
	}
	return static_cast<int>(result);
}

static double Bernstein(int n, int i, double t) {
	return Binomial(n, i) * pow(t, i) * pow(1.0 - t, n - i);
}

static double BernsteinDerivative(int n, int i, double t) {
	double left = (i > 0) ? Bernstein(n - 1, i - 1, t) : 0.0;
	double right = (i < n) ? Bernstein(n - 1, i, t) : 0.0;
	return n * (left - right);
}

// 按原始Bezier多项式参数u（非弧长）计算连线上一点坐标，供弧长参数化内部采样使用
static void EvaluateBezierPoint(const vector<pair<Node*, float>>& allPoints, int m, double u, double& x, double& y, double& z) {
	double sumWeight = 0.0;
	x = 0.0;
	y = 0.0;
	z = 0.0;
	for (int i = 0; i <= m; i++) {
		double B = Bernstein(m, i, u);
		double w = allPoints[i].second;
		sumWeight += B * w;
		x += B * w * allPoints[i].first->GetX();
		y += B * w * allPoints[i].first->GetY();
		z += B * w * allPoints[i].first->GetZ();
	}
	if (sumWeight != 0.0) {
		x /= sumWeight;
		y /= sumWeight;
		z /= sumWeight;
	}
}

Connection::Connection(Node n1, Node n2, float begin, float end) :
	begin(begin),
	end(end),
	beginVertex(new Node(n1)),
	endVertex(new Node(n2)),
	controlVertices(),
	arcLengthCache() {

}

Connection::Connection(const Connection& other) :
	begin(other.begin),
	end(other.end),
	beginVertex(new Node(*other.beginVertex)),
	endVertex(new Node(*other.endVertex)),
	controlVertices(),
	arcLengthCache(other.arcLengthCache) {
	for (auto& [node, weight] : other.controlVertices) {
		controlVertices.emplace_back(new Node(*node), weight);
	}
}

Connection& Connection::operator=(const Connection& other) {
	if (this != &other) {
		delete beginVertex;
		delete endVertex;
		for (auto& [node, _] : controlVertices) {
			delete node;
		}
		controlVertices.clear();

		begin = other.begin;
		end = other.end;
		beginVertex = new Node(*other.beginVertex);
		endVertex = new Node(*other.endVertex);
		for (auto& [node, weight] : other.controlVertices) {
			controlVertices.emplace_back(new Node(*node), weight);
		}
		arcLengthCache = other.arcLengthCache;
	}
	return *this;
}

Connection::~Connection() {
	delete beginVertex;
	delete endVertex;
	for (auto& [node, _] : controlVertices) {
		delete node;
	}
}

void Connection::AddControls(vector<pair<Node, float>> controls) {
	for (auto& [node, weight] : controls) {
		controlVertices.emplace_back(new Node(node), weight);
	}
	arcLengthCache.clear();
}

// 按弧长比例f反查对应的原始Bezier多项式参数u，使GetPoint/GetTangent的参数f按物理距离线性变化
// （即f=0.5时对应的点就是曲线弧长意义上的中点），而不是Bezier多项式意义上的中点；
// 弧长表首次调用时惰性建好并缓存到arcLengthCache，同一条Connection之后的调用不再重新采样
double Connection::ResolveArcLengthParam(const vector<pair<Node*, float>>& allPoints, int m, float f) const {
	constexpr int SAMPLES = 128;
	if (arcLengthCache.empty()) {
		arcLengthCache.resize(SAMPLES + 1);
		double prevX, prevY, prevZ;
		EvaluateBezierPoint(allPoints, m, 0.0, prevX, prevY, prevZ);
		arcLengthCache[0] = 0.0;
		for (int i = 1; i <= SAMPLES; i++) {
			double u = static_cast<double>(i) / SAMPLES;
			double x, y, z;
			EvaluateBezierPoint(allPoints, m, u, x, y, z);
			double dx = x - prevX, dy = y - prevY, dz = z - prevZ;
			arcLengthCache[i] = arcLengthCache[i - 1] + sqrt(dx * dx + dy * dy + dz * dz);
			prevX = x;
			prevY = y;
			prevZ = z;
		}
	}

	double targetLength = f * arcLengthCache[SAMPLES];
	int lo = 0, hi = SAMPLES;
	while (lo < hi - 1) {
		int mid = (lo + hi) / 2;
		if (arcLengthCache[mid] < targetLength) lo = mid;
		else hi = mid;
	}
	double lenLo = arcLengthCache[lo], lenHi = arcLengthCache[hi];
	double ratio = (lenHi > lenLo) ? (targetLength - lenLo) / (lenHi - lenLo) : 0.0;
	return (lo + ratio) / SAMPLES;
}

vector<pair<Node, float>> Connection::GetControls() const {
	vector<pair<Node, float>> result;
	for (auto& [node, weight] : controlVertices) {
		result.emplace_back(*node, weight);
	}
	return result;
}

bool Connection::operator==(const Connection& other) const {
	return beginVertex->GetId() == other.beginVertex->GetId() &&
		endVertex->GetId() == other.endVertex->GetId();
}

Node Connection::GetStart() const {
	return *beginVertex;
}

Node Connection::GetEnd() const {
	return *endVertex;
}

Node Connection::GetPoint(float f) const {
	if (f < 0.f || f > 1.f) {
		debugf("Warning: Bizier query position out of [0, 1].\n");
		if (f < 0.f) f = 0.f;
		if (f > 1.f) f = 1.f;
	}

	float x1 = beginVertex->GetX();
	float y1 = beginVertex->GetY();
	float z1 = beginVertex->GetZ();
	float x2 = endVertex->GetX();
	float y2 = endVertex->GetY();
	float z2 = endVertex->GetZ();

	if (controlVertices.empty()) {
		float x = x1 + f * (x2 - x1);
		float y = y1 + f * (y2 - y1);
		float z = z1 + f * (z2 - z1);
		return Node("", x, y, z);
	}
	else {
		int n = static_cast<int>(controlVertices.size());
		vector<pair<Node*, float>> allPoints;
		allPoints.reserve(n + 2);
		allPoints.emplace_back(beginVertex, 1.0f);
		for (auto& p : controlVertices) {
			allPoints.push_back(p);
		}
		allPoints.emplace_back(endVertex, 1.0f);

		int m = static_cast<int>(allPoints.size()) - 1;
		double u = ResolveArcLengthParam(allPoints, m, f);
		double sumWeight = 0.0;
		double x = 0.0, y = 0.0, z = 0.0;

		for (int i = 0; i <= m; i++) {
			double B = Bernstein(m, i, u);
			double w = allPoints[i].second;
			sumWeight += B * w;
			x += B * w * allPoints[i].first->GetX();
			y += B * w * allPoints[i].first->GetY();
			z += B * w * allPoints[i].first->GetZ();
		}

		if (sumWeight != 0.0) {
			x /= sumWeight;
			y /= sumWeight;
			z /= sumWeight;
		}
		return Node("", static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
	}
}

void Connection::GetTangent(float f, float& dx, float& dy, float& dz) const {
	if (f < 0.f || f > 1.f) {
		debugf("Warning: Bizier query position out of [0, 1].\n");
		if (f < 0.f) f = 0.f;
		if (f > 1.f) f = 1.f;
	}

	float x1 = beginVertex->GetX();
	float y1 = beginVertex->GetY();
	float z1 = beginVertex->GetZ();
	float x2 = endVertex->GetX();
	float y2 = endVertex->GetY();
	float z2 = endVertex->GetZ();

	if (controlVertices.empty()) {
		dx = x2 - x1;
		dy = y2 - y1;
		dz = z2 - z1;
	}
	else {
		int n = static_cast<int>(controlVertices.size());
		vector<pair<Node*, float>> allPoints;
		allPoints.reserve(n + 2);
		allPoints.emplace_back(beginVertex, 1.0f);
		for (auto& p : controlVertices) {
			allPoints.push_back(p);
		}
		allPoints.emplace_back(endVertex, 1.0f);

		int m = static_cast<int>(allPoints.size()) - 1;
		double u = ResolveArcLengthParam(allPoints, m, f);
		double sumWeight = 0.0, sumWeightDeriv = 0.0;
		double x = 0.0, y = 0.0, z = 0.0;
		double xDeriv = 0.0, yDeriv = 0.0, zDeriv = 0.0;

		for (int i = 0; i <= m; i++) {
			double B = Bernstein(m, i, u);
			double Bd = BernsteinDerivative(m, i, u);
			double w = allPoints[i].second;
			sumWeight += B * w;
			sumWeightDeriv += Bd * w;
			x += B * w * allPoints[i].first->GetX();
			y += B * w * allPoints[i].first->GetY();
			z += B * w * allPoints[i].first->GetZ();
			xDeriv += Bd * w * allPoints[i].first->GetX();
			yDeriv += Bd * w * allPoints[i].first->GetY();
			zDeriv += Bd * w * allPoints[i].first->GetZ();
		}

		// 有理Bezier曲线导数由商法则给出：(N'D - ND') / D^2
		if (sumWeight != 0.0) {
			dx = static_cast<float>((xDeriv * sumWeight - x * sumWeightDeriv) / (sumWeight * sumWeight));
			dy = static_cast<float>((yDeriv * sumWeight - y * sumWeightDeriv) / (sumWeight * sumWeight));
			dz = static_cast<float>((zDeriv * sumWeight - z * sumWeightDeriv) / (sumWeight * sumWeight));
		}
		else {
			dx = dy = dz = 0.f;
		}
	}
}

static float ComputeLength(const Connection& connection, float t1, float t2, int segments = 16) {
	if (t1 == t2) return 0.0f;
	if (t1 > t2) swap(t1, t2);
	float step = (t2 - t1) / segments;
	float length = 0.0f;
	Node prev = connection.GetPoint(t1);
	for (int i = 1; i <= segments; i++) {
		float t = t1 + i * step;
		Node curr = connection.GetPoint(t);
		float dx = curr.GetX() - prev.GetX();
		float dy = curr.GetY() - prev.GetY();
		float dz = curr.GetZ() - prev.GetZ();
		length += sqrt(dx * dx + dy * dy + dz * dz);
		prev = curr;
	}
	return length;
}

float Connection::CalcDistance() const {
	return ComputeLength(*this, begin, end);
}

float Connection::CalcDistance(float f1, float f2) const {
	return ComputeLength(*this, f1, f2);
}

Intersection::Intersection(float x, float y, float z) : Node("roadnet", x, y, z) {

}

Intersection::Intersection(const Node& node) : Node(node) {

}

Intersection::Intersection(const Intersection& other) : Node(other) {

}

Intersection& Intersection::operator=(const Intersection& other) {
	Node::operator=(other);
	return *this;
}

Intersection::~Intersection() {

}

Road::Road(string name, Node n1, Node n2, string mesh, float unit, float begin, float end) :
	Connection(n1, n2, begin, end),
	name(name),
	mesh(mesh),
	unit(unit) {

}

Road::Road(const Connection& connection, string name, string mesh, float unit) :
	Connection(connection),
	name(name),
	mesh(mesh),
	unit(unit) {

}

Road::Road(const Road& other) :
	Connection(other),
	name(other.name),
	mesh(other.mesh),
	unit(other.unit),
	vehicleLanes{ other.vehicleLanes[0], other.vehicleLanes[1] },
	parkingLanes{ other.parkingLanes[0], other.parkingLanes[1] },
	pedestrianLanes{ other.pedestrianLanes[0], other.pedestrianLanes[1] },
	openings(other.openings),
	isPathRoad(other.isPathRoad) {

}

Road& Road::operator=(const Road& other) {
	if (this != &other) {
		Connection::operator=(other);
		name = other.name;
		mesh = other.mesh;
		unit = other.unit;
		vehicleLanes[0] = other.vehicleLanes[0];
		vehicleLanes[1] = other.vehicleLanes[1];
		parkingLanes[0] = other.parkingLanes[0];
		parkingLanes[1] = other.parkingLanes[1];
		pedestrianLanes[0] = other.pedestrianLanes[0];
		pedestrianLanes[1] = other.pedestrianLanes[1];
		openings = other.openings;
		isPathRoad = other.isPathRoad;
	}
	return *this;
}

Road::~Road() {

}

string Road::GetName() const {
	return name;
}

string Road::GetMesh() const {
	return mesh;
}

float Road::GetUnit() const {
	return unit;
}

void Road::AddVehicleLane(int side, float width) {
	if (side != 0 && side != 1) return;
	vehicleLanes[side].push_back(width);
}

const vector<float>& Road::GetVehicleLanes(int side) const {
	static const vector<float> empty;
	if (side != 0 && side != 1) return empty;
	return vehicleLanes[side];
}

void Road::AddParkingLane(int side, float width) {
	if (side != 0 && side != 1) return;
	parkingLanes[side].push_back(width);
}

const vector<float>& Road::GetParkingLanes(int side) const {
	static const vector<float> empty;
	if (side != 0 && side != 1) return empty;
	return parkingLanes[side];
}

void Road::AddPedestrianLane(int side, float width) {
	if (side != 0 && side != 1) return;
	pedestrianLanes[side].push_back(width);
}

const vector<float>& Road::GetPedestrianLanes(int side) const {
	static const vector<float> empty;
	if (side != 0 && side != 1) return empty;
	return pedestrianLanes[side];
}

void Road::AddOpening(const RoadOpening& opening) {
	openings.push_back(opening);
}

const vector<RoadOpening>& Road::GetOpenings() const {
	return openings;
}

void Road::SetPathRoad(bool isPath) {
	isPathRoad = isPath;
}

bool Road::IsPathRoad() const {
	return isPathRoad;
}

float Road::GetSideWidth(int side) const {
	if (side != 0 && side != 1) return 0.f;
	float sum = 0.f;
	for (float w : vehicleLanes[side]) sum += w;
	for (float w : parkingLanes[side]) sum += w;
	for (float w : pedestrianLanes[side]) sum += w;
	return sum;
}

float Road::GetTotalWidth() const {
	return GetSideWidth(0) + GetSideWidth(1);
}

Quad::Quad() :
	posX(0.f),
	posY(0.f),
	sizeX(0.f),
	sizeY(0.f),
	acreage(0.f) {

}

Quad::Quad(float x, float y, float w, float h) :
	posX(x),
	posY(y),
	sizeX(w),
	sizeY(h),
	acreage(w * h * ACREAGE_SCALE_FACTOR) {

}

Quad::~Quad() {

}

float Quad::GetPosX() const {
	return posX;
}

void Quad::SetPosX(float x) {
	posX = x;
}

float Quad::GetPosY() const {
	return posY;
}

void Quad::SetPosY(float y) {
	posY = y;
}

float Quad::GetSizeX() const {
	return sizeX;
}

void Quad::SetSizeX(float w) {
	sizeX = w;
}

float Quad::GetSizeY() const {
	return sizeY;
}

void Quad::SetSizeY(float h) {
	sizeY = h;
}

float Quad::GetLeft() const {
	return posX - sizeX / 2.f;
}

float Quad::GetRight() const {
	return posX + sizeX / 2.f;
}

float Quad::GetBottom() const {
	return posY - sizeY / 2.f;
}

float Quad::GetTop() const {
	return posY + sizeY / 2.f;
}

void Quad::SetVertices(float x1, float y1, float x2, float y2) {
	if (x1 > x2) {
		swap(x1, x2);
	}
	if (y1 > y2) {
		swap(y1, y2);
	}

	posX = (x1 + x2) / 2.f;
	posY = (y1 + y2) / 2.f;
	sizeX = x2 - x1;
	sizeY = y2 - y1;
	acreage = sizeX * sizeY * ACREAGE_SCALE_FACTOR;
}

void Quad::SetPosition(float x, float y, float w, float h) {
	posX = x;
	posY = y;
	sizeX = w;
	sizeY = h;
	acreage = sizeX * sizeY * ACREAGE_SCALE_FACTOR;
}

float Quad::GetAcreage() const {
	return acreage;
}

void Quad::SetAcreage(float a) {
	acreage = a;
}

Lot::Lot() :
	Quad(),
	rotation(0.f),
	area(AREA_NONE) {

}

Lot::Lot(float x, float y, float w, float h, float r) :
	Quad(x, y, w, h),
	rotation(r),
	area(AREA_NONE) {

}

Lot::Lot(Node n1, Node n2, Node n3, vector<float> margin, const unordered_map<int, Road*>& boundary) :
	Quad(),
	rotation(0.f),
	area(AREA_NONE) {
	SetPosition(n1, n2, n3, margin);
	for (const auto& [direction, road] : boundary) {
		SetBoundaryRoad(direction, road);
	}
}

Lot::Lot(Node n1, Node n2, Node n3, Node n4, vector<float> margin, const unordered_map<int, Road*>& boundary) :
	Quad(),
	rotation(0.f),
	area(AREA_NONE) {
	SetPosition(n1, n2, n3, n4, margin);
	for (const auto& [direction, road] : boundary) {
		SetBoundaryRoad(direction, road);
	}
}

Lot::~Lot() {
	for (Lot* free : freeLots) {
		delete free;
	}
	for (const PathRoadLink& link : pathRoadLinks) {
		delete link.road;
	}
}

float Lot::GetRotation() const {
	return rotation;
}

void Lot::SetRotation(float r) {
	rotation = r;
}

AREA_TYPE Lot::GetArea() const {
	return area;
}

void Lot::SetArea(AREA_TYPE a) {
	area = a;
}

pair<float, float> Lot::GetVertex(int idx) const {
	if (idx < 0 || idx > 3) {
		THROW_EXCEPTION(OutOfRangeException, "Block vertex out of range [0, 3].\n");
	}

	float hx = sizeX / 2.0f;
	float hy = sizeY / 2.0f;
	float c = cos(rotation);
	float s = sin(rotation);

	switch (idx) {
	case 0: // 左上
		return { posX - hx * c + hy * s, posY - hx * s - hy * c };
	case 1: // 右上
		return { posX + hx * c + hy * s, posY + hx * s - hy * c };
	case 2: // 右下
		return { posX + hx * c - hy * s, posY + hx * s + hy * c };
	case 3: // 左下
		return { posX - hx * c - hy * s, posY - hx * s + hy * c };
	default:
		return { 0.f, 0.f };
	}
}

pair<float, float> Lot::GetPosition(float x, float y) const {
	float cosR = cos(rotation);
	float sinR = sin(rotation);

	float relativeX = x - sizeX / 2.0f;
	float relativeY = y - sizeY / 2.0f;

	float rotatedX = relativeX * cosR - relativeY * sinR;
	float rotatedY = relativeX * sinR + relativeY * cosR;

	return { posX + rotatedX, posY + rotatedY };
}

void Lot::SetPosition(Node n1, Node n2, Node n3, const vector<float>& margin) {
	if (margin.size() != 4) {
		THROW_EXCEPTION(InvalidArgumentException, "Block must have 4 margins.\n");
	}

	float x1 = n1.GetX(), y1 = n1.GetY();
	float x2 = n2.GetX(), y2 = n2.GetY();
	float x3 = n3.GetX(), y3 = n3.GetY();

	// 向量 u = p2 - p1, v = p3 - p2
	float ux = x2 - x1, uy = y2 - y1;
	float vx = x3 - x2, vy = y3 - y2;

	// 检查垂直
	float dot = ux * vx + uy * vy;
	const float eps = 1e-5f;
	if (abs(dot) > eps) {
		THROW_EXCEPTION(InvalidArgumentException, "Block edges are not perpendicular.\n");
	}

	// 计算尺寸
	float sx = sqrt(ux * ux + uy * uy);
	float sy = sqrt(vx * vx + vy * vy);

	// 应用边距
	sx -= margin[1] + margin[3];
	sy -= margin[0] + margin[2];

	// 计算中心点：p1 和 p3 的对角线中心，加上边距偏移
	float cx = (x1 + x3) / 2.0f
		+ (margin[3] - margin[1]) * ux / (2.0f * sx)
		+ (margin[2] - margin[0]) * vx / (2.0f * sy);
	float cy = (y1 + y3) / 2.0f
		+ (margin[3] - margin[1]) * uy / (2.0f * sx)
		+ (margin[2] - margin[0]) * vy / (2.0f * sy);

	// 计算旋转角度（p1p2 与 x 轴夹角）
	float rot = atan2(uy, ux);

	// 更新成员变量
	posX = cx;
	posY = cy;
	sizeX = sx;
	sizeY = sy;
	rotation = rot;
	acreage = sx * sy * ACREAGE_SCALE_FACTOR;
}

void Lot::AddAddress(const string& road, int index) {
	addresses.emplace_back(road, index);
}

const vector<pair<string, int>>& Lot::GetAddresses() const {
	return addresses;
}

string Lot::GetAddress() const {
	if (addresses.empty()) return "";
	return addresses[0].first + " " + to_string(addresses[0].second);
}

void Lot::SetBoundaryRoad(int direction, Road* road) {
	boundaryRoads[direction] = road;
}

Road* Lot::GetBoundaryRoad(int direction) const {
	auto it = boundaryRoads.find(direction);
	return it != boundaryRoads.end() ? it->second : nullptr;
}

const unordered_map<int, Road*>& Lot::GetBoundaryRoads() const {
	return boundaryRoads;
}

void Lot::SetPosition(Node n1, Node n2, Node n3, Node n4, const vector<float>& margin) {
	if (margin.size() != 4) {
		THROW_EXCEPTION(InvalidArgumentException, "Block must have 4 margins.\n");
	}

	vector<Node> nodes = { n1, n2, n3, n4 };

	// 计算中心点
	float cx = 0.0f, cy = 0.0f;
	for (const auto& node : nodes) {
		cx += node.GetX();
		cy += node.GetY();
	}
	cx /= 4.0f;
	cy /= 4.0f;

	// 按顺时针排序
	sort(nodes.begin(), nodes.end(),
		[cx, cy](const Node& a, const Node& b) {
			return atan2(a.GetY() - cy, a.GetX() - cx) >
				atan2(b.GetY() - cy, b.GetX() - cx);
		});

	// 检查矩形条件
	const float eps = 1e-2f;

	// 计算四个边向量
	float u1x = nodes[1].GetX() - nodes[0].GetX();
	float u1y = nodes[1].GetY() - nodes[0].GetY();
	float u2x = nodes[2].GetX() - nodes[1].GetX();
	float u2y = nodes[2].GetY() - nodes[1].GetY();
	float u3x = nodes[3].GetX() - nodes[2].GetX();
	float u3y = nodes[3].GetY() - nodes[2].GetY();
	float u4x = nodes[0].GetX() - nodes[3].GetX();
	float u4y = nodes[0].GetY() - nodes[3].GetY();

	// 检查邻边垂直（点积为0）
	float dot1 = u1x * u2x + u1y * u2y;
	float dot2 = u2x * u3x + u2y * u3y;
	float dot3 = u3x * u4x + u3y * u4y;
	float dot4 = u4x * u1x + u4y * u1y;

	if (abs(dot1) > eps || abs(dot2) > eps ||
		abs(dot3) > eps || abs(dot4) > eps) {
		THROW_EXCEPTION(InvalidArgumentException, "Block edges are not perpendicular.\n");
	}

	// 检查对边长度相等
	float len1 = sqrt(u1x * u1x + u1y * u1y);
	float len2 = sqrt(u2x * u2x + u2y * u2y);
	float len3 = sqrt(u3x * u3x + u3y * u3y);
	float len4 = sqrt(u4x * u4x + u4y * u4y);

	if (abs(len1 - len3) > eps || abs(len2 - len4) > eps) {
		THROW_EXCEPTION(InvalidArgumentException, "Block opposite edges are not equal.\n");
	}

	// 计算尺寸（取相邻两边长度）
	float sx = len1;
	float sy = len2;

	// 计算旋转角度（使用第一条边 nodes[0]->nodes[1]）
	float rot = atan2(u1y, u1x);

	// 应用边距
	sx -= margin[1] + margin[0];
	sy -= margin[2] + margin[3];
	cx += (margin[0] - margin[1]) * u1x / (2.0f * sx)
		+ (margin[3] - margin[2]) * u2x / (2.0f * sy);
	cy += (margin[0] - margin[1]) * u1y / (2.0f * sx)
		+ (margin[3] - margin[2]) * u2y / (2.0f * sy);

	// 更新成员变量
	posX = cx;
	posY = cy;
	sizeX = sx;
	sizeY = sy;
	rotation = rot;
	acreage = sx * sy * ACREAGE_SCALE_FACTOR;
}

namespace {
	// 把一个和reference共享同一个旋转角的世界坐标点，换算成reference本地坐标系下的(x,y)
	// （原点在WEST-NORTH角，和Lot::GetPosition/GetVertex的既有约定一致：局部x=0是WEST、
	// x=sizeX是EAST，局部y=0是NORTH、y=sizeY是SOUTH）。这是GetPosition的逆变换。
	pair<float, float> ToLocal(const Quad& reference, float rotation, float worldX, float worldY) {
		float dx = worldX - reference.GetPosX();
		float dy = worldY - reference.GetPosY();
		float c = cosf(rotation);
		float s = sinf(rotation);
		float relX = dx * c + dy * s;
		float relY = -dx * s + dy * c;
		return { relX + reference.GetSizeX() / 2.f, relY + reference.GetSizeY() / 2.f };
	}

	// 一个子块"可达"当且仅当它至少有一边的边界Road非空——真正的切分总会在分割线上生成一条
	// Road（Lot::SplitWithPath），唯一天生没有Road的边是从顶层Lot继承下来、老工程就没有路的
	// 内部分界边。
	bool HasAnyBoundaryRoad(const Lot* lot) {
		for (int dir = 0; dir < 4; dir++) {
			if (lot->GetBoundaryRoad(dir)) return true;
		}
		return false;
	}

	constexpr float MIN_LOT_EXTENT = 2.f;

	// 把世界坐标点(px,py)反投影到road的Start->End直线上，算出近似弧长比例t——按直线而不是
	// 真正的曲线弧长反查，只覆盖小路一般连接的基本走直线的frontage场景，弯道中间开口这次不
	// 精确处理(和这次范围内其余投影计算一致)。
	float ProjectT(const Road* road, float px, float py) {
		Node start = road->GetStart();
		Node end = road->GetEnd();
		float dx = end.GetX() - start.GetX();
		float dy = end.GetY() - start.GetY();
		float lenSq = dx * dx + dy * dy;
		if (lenSq < 1e-9f) return 0.f;
		float t = ((px - start.GetX()) * dx + (py - start.GetY()) * dy) / lenSq;
		return max(0.f, min(1.f, t));
	}
}

Lot::SplitResult Lot::SplitWithPath(bool splitAlongX, float splitCoordinate, const PathLaneSpec& spec) {
	SplitResult failResult;
	float pathWidth = spec.vehicleWidth * 2.f + spec.pedestrianWidth * 2.f;
	float half = pathWidth / 2.f;
	float extent = splitAlongX ? sizeX : sizeY;
	if (splitCoordinate - half < 0.f || splitCoordinate + half > extent) {
		return failResult;
	}

	// 局部坐标系：X轴0=WEST、sizeX=EAST；Y轴0=NORTH、sizeY=SOUTH。splitAlongX时，切割线
	// （一条垂直于X轴、贯穿Y范围的线）两端分别落在NORTH(y=0)和SOUTH(y=sizeY)边上；否则
	// （一条垂直于Y轴、贯穿X范围的线）两端分别落在WEST(x=0)和EAST(x=sizeX)边上。这两个方向
	// 也是切完之后两段都直接继承、不受这次切割影响的边界。
	int endFace1 = splitAlongX ? FACE_NORTH : FACE_WEST;
	int endFace2 = splitAlongX ? FACE_SOUTH : FACE_EAST;
	Road* endRoad1 = GetBoundaryRoad(endFace1);
	Road* endRoad2 = GetBoundaryRoad(endFace2);
	if (!endRoad1 && !endRoad2) {
		return failResult; // 关键设计决策8：两端都没有已有路可连，拒绝产生孤岛小路
	}

	float lx1 = splitAlongX ? splitCoordinate : 0.f;
	float ly1 = splitAlongX ? 0.f : splitCoordinate;
	float lx2 = splitAlongX ? splitCoordinate : sizeX;
	float ly2 = splitAlongX ? sizeY : splitCoordinate;
	auto [wx1, wy1] = GetPosition(lx1, ly1);
	auto [wx2, wy2] = GetPosition(lx2, ly2);

	// 受这次切割影响的一对方向：splitAlongX时是WEST/EAST，否则是NORTH/SOUTH。lower段（局部
	// 坐标较小的一侧）保留原WEST(或NORTH)、新的一侧指向小路；upper段反过来。
	int lowerKeepFace = splitAlongX ? FACE_WEST : FACE_NORTH;
	int upperKeepFace = splitAlongX ? FACE_EAST : FACE_SOUTH;
	int lowerPathFace = splitAlongX ? FACE_EAST : FACE_SOUTH;
	int upperPathFace = splitAlongX ? FACE_WEST : FACE_NORTH;

	float lowerW, lowerH, upperW, upperH, lowerLocalCX, lowerLocalCY, upperLocalCX, upperLocalCY;
	if (splitAlongX) {
		lowerW = splitCoordinate - half;
		lowerH = sizeY;
		lowerLocalCX = lowerW / 2.f;
		lowerLocalCY = sizeY / 2.f;

		upperW = sizeX - (splitCoordinate + half);
		upperH = sizeY;
		upperLocalCX = splitCoordinate + half + upperW / 2.f;
		upperLocalCY = sizeY / 2.f;
	}
	else {
		lowerW = sizeX;
		lowerH = splitCoordinate - half;
		lowerLocalCX = sizeX / 2.f;
		lowerLocalCY = lowerH / 2.f;

		upperW = sizeX;
		upperH = sizeY - (splitCoordinate + half);
		upperLocalCX = sizeX / 2.f;
		upperLocalCY = splitCoordinate + half + upperH / 2.f;
	}

	if (lowerW < MIN_LOT_EXTENT || lowerH < MIN_LOT_EXTENT || upperW < MIN_LOT_EXTENT || upperH < MIN_LOT_EXTENT) {
		return failResult;
	}

	Road* pathRoad = new Road("path", Node("path", wx1, wy1), Node("path", wx2, wy2), "", 0.f);
	pathRoad->SetPathRoad(true);
	pathRoad->AddVehicleLane(0, spec.vehicleWidth);
	pathRoad->AddVehicleLane(1, spec.vehicleWidth);
	pathRoad->AddPedestrianLane(0, spec.pedestrianWidth);
	pathRoad->AddPedestrianLane(1, spec.pedestrianWidth);

	// 小路两端各自落在的Road上的弧长比例(直线投影近似)，不管endRoad是不是小路都要算——大路
	// 开口标记只在endRoad不是小路时才打，但这个t值本身是Core层Map::ConnectPathRoad接导航图
	// 时唯一需要的定位信息，两种情况(大路/小路)都要提供，所以从"只在大路分支里算"挪成"只要
	// endRoad非空就算"。
	float endT1 = endRoad1 ? ProjectT(endRoad1, wx1, wy1) : 0.f;
	float endT2 = endRoad2 ? ProjectT(endRoad2, wx2, wy2) : 0.f;

	// 小路接到一条"大路"(非小路)上的那一端，给大路标一个开口——只是几何/渲染意义上的路面
	// 缺口标记(RoadOpening)，不触碰导航图(那部分交给Core层Map::ConnectPathRoad，见map.md)。
	// 两条小路互相连接的路口不算"大路被开口"，不在这里标记。forwardSide/isVehicle这两个字段
	// 目前没有消费方会读（只有Map::AddRoadAccessNode自己产出的开口才用得到，用来选车道），
	// 随便填一个值即可。
	if (endRoad1 && !endRoad1->IsPathRoad()) {
		RoadOpening opening;
		opening.t = endT1;
		opening.width = pathWidth;
		opening.forwardSide = true;
		opening.isVehicle = true;
		endRoad1->AddOpening(opening);
	}
	if (endRoad2 && !endRoad2->IsPathRoad()) {
		RoadOpening opening;
		opening.t = endT2;
		opening.width = pathWidth;
		opening.forwardSide = true;
		opening.isVehicle = true;
		endRoad2->AddOpening(opening);
	}

	auto [lowerWorldX, lowerWorldY] = GetPosition(lowerLocalCX, lowerLocalCY);
	auto [upperWorldX, upperWorldY] = GetPosition(upperLocalCX, upperLocalCY);

	Lot* lowerLot = new Lot(lowerWorldX, lowerWorldY, lowerW, lowerH, rotation);
	Lot* upperLot = new Lot(upperWorldX, upperWorldY, upperW, upperH, rotation);

	if (endRoad1) { lowerLot->SetBoundaryRoad(endFace1, endRoad1); upperLot->SetBoundaryRoad(endFace1, endRoad1); }
	if (endRoad2) { lowerLot->SetBoundaryRoad(endFace2, endRoad2); upperLot->SetBoundaryRoad(endFace2, endRoad2); }

	Road* keepRoadLower = GetBoundaryRoad(lowerKeepFace);
	Road* keepRoadUpper = GetBoundaryRoad(upperKeepFace);
	if (keepRoadLower) lowerLot->SetBoundaryRoad(lowerKeepFace, keepRoadLower);
	if (keepRoadUpper) upperLot->SetBoundaryRoad(upperKeepFace, keepRoadUpper);
	lowerLot->SetBoundaryRoad(lowerPathFace, pathRoad);
	upperLot->SetBoundaryRoad(upperPathFace, pathRoad);

	SplitResult result;
	result.lowerLot = lowerLot;
	result.upperLot = upperLot;
	result.pathRoad = pathRoad;
	result.endRoad1 = endRoad1;
	result.endT1 = endT1;
	result.endRoad2 = endRoad2;
	result.endT2 = endT2;
	return result;
}

vector<Lot*>& Lot::GetFreeLots() {
	if (!freeLotsInitialized) {
		freeLotsInitialized = true;
		Lot* self = new Lot(posX, posY, sizeX, sizeY, rotation);
		for (auto& [dir, road] : boundaryRoads) {
			self->SetBoundaryRoad(dir, road);
		}
		freeLots.push_back(self);
	}
	return freeLots;
}

float Lot::GetFreeAcreage() {
	float sum = 0.f;
	for (Lot* free : GetFreeLots()) {
		sum += free->GetAcreage();
	}
	return sum;
}

bool Lot::RequestPlacement(int direction, float marginStart, float marginEnd, float depth,
	const PathLaneSpec& spec, Quad* outPlaced, unordered_map<int, Road*>* outBoundaryRoads) {

	if (!GetBoundaryRoad(direction)) {
		return false; // 关键设计决策3：方向没有路，直接拒绝
	}

	float targetXMin, targetXMax, targetYMin, targetYMax;
	switch (direction) {
	case FACE_WEST:
		targetXMin = 0.f; targetXMax = depth;
		targetYMin = marginStart; targetYMax = sizeY - marginEnd;
		break;
	case FACE_EAST:
		targetXMin = sizeX - depth; targetXMax = sizeX;
		targetYMin = marginStart; targetYMax = sizeY - marginEnd;
		break;
	case FACE_NORTH:
		targetYMin = 0.f; targetYMax = depth;
		targetXMin = marginStart; targetXMax = sizeX - marginEnd;
		break;
	default: // FACE_SOUTH
		targetYMin = sizeY - depth; targetYMax = sizeY;
		targetXMin = marginStart; targetXMax = sizeX - marginEnd;
		break;
	}
	if (targetXMax - targetXMin < MIN_LOT_EXTENT || targetYMax - targetYMin < MIN_LOT_EXTENT) {
		return false;
	}

	auto [twx1, twy1] = GetPosition(targetXMin, targetYMin);
	auto [twx2, twy2] = GetPosition(targetXMax, targetYMax);

	auto& pool = GetFreeLots();
	for (size_t i = 0; i < pool.size(); i++) {
		Lot* candidate = pool[i];
		auto [lx1, ly1] = ToLocal(*candidate, rotation, twx1, twy1);
		auto [lx2, ly2] = ToLocal(*candidate, rotation, twx2, twy2);
		float lxmin = min(lx1, lx2), lxmax = max(lx1, lx2);
		float lymin = min(ly1, ly2), lymax = max(ly1, ly2);

		const float eps = 1e-2f;
		if (lxmin < -eps || lymin < -eps || lxmax > candidate->GetSizeX() + eps || lymax > candidate->GetSizeY() + eps) {
			continue; // 目标矩形没有完全落在这块自由地里，换下一块
		}

		Lot* working = candidate;
		vector<Lot*> survivors;
		bool ok = true;
		float pathHalf = (spec.vehicleWidth * 2.f + spec.pedestrianWidth * 2.f) / 2.f;

		auto reproject = [&]() {
			auto [ax1, ay1] = ToLocal(*working, rotation, twx1, twy1);
			auto [ax2, ay2] = ToLocal(*working, rotation, twx2, twy2);
			return make_tuple(min(ax1, ax2), max(ax1, ax2), min(ay1, ay2), max(ay1, ay2));
		};

		auto cutKeepLower = [&](bool alongXAxis, float cutCoord) -> bool {
			auto res = working->SplitWithPath(alongXAxis, cutCoord, spec);
			if (!res.pathRoad) return false;
			pathRoadLinks.push_back({ res.pathRoad, res.endRoad1, res.endT1, res.endRoad2, res.endT2 });
			if (res.upperLot->GetSizeX() >= MIN_LOT_EXTENT && res.upperLot->GetSizeY() >= MIN_LOT_EXTENT && HasAnyBoundaryRoad(res.upperLot)) {
				survivors.push_back(res.upperLot);
			}
			else {
				delete res.upperLot;
			}
			delete working;
			working = res.lowerLot;
			return true;
		};
		auto cutKeepUpper = [&](bool alongXAxis, float cutCoord) -> bool {
			auto res = working->SplitWithPath(alongXAxis, cutCoord, spec);
			if (!res.pathRoad) return false;
			pathRoadLinks.push_back({ res.pathRoad, res.endRoad1, res.endT1, res.endRoad2, res.endT2 });
			if (res.lowerLot->GetSizeX() >= MIN_LOT_EXTENT && res.lowerLot->GetSizeY() >= MIN_LOT_EXTENT && HasAnyBoundaryRoad(res.lowerLot)) {
				survivors.push_back(res.lowerLot);
			}
			else {
				delete res.lowerLot;
			}
			delete working;
			working = res.upperLot;
			return true;
		};

		bool depthAlongX = (direction == FACE_WEST || direction == FACE_EAST);
		bool touchesLowEnd = (direction == FACE_WEST || direction == FACE_NORTH);

		// ①深度方向：裁掉目标矩形远离道路一侧的多余部分（如果目标已经顶到这块自由地的最远端
		// 就跳过）
		{
			auto [wxmin, wxmax, wymin, wymax] = reproject();
			float depthMin = depthAlongX ? wxmin : wymin;
			float depthMax = depthAlongX ? wxmax : wymax;
			float depthExtent = depthAlongX ? working->GetSizeX() : working->GetSizeY();
			if (touchesLowEnd) {
				if (depthExtent - depthMax > 1e-2f) ok = cutKeepLower(depthAlongX, depthMax + pathHalf);
			}
			else {
				if (depthMin > 1e-2f) ok = cutKeepUpper(depthAlongX, depthMin - pathHalf);
			}
		}

		// ②③沿道路方向(frontage轴)：依次裁掉marginStart端、marginEnd端的多余部分
		if (ok) {
			auto [wxmin, wxmax, wymin, wymax] = reproject();
			float frontMin = depthAlongX ? wymin : wxmin;
			if (frontMin > 1e-2f) ok = cutKeepUpper(!depthAlongX, frontMin - pathHalf);
		}
		if (ok) {
			auto [wxmin, wxmax, wymin, wymax] = reproject();
			float frontMax = depthAlongX ? wymax : wxmax;
			float frontExtent = depthAlongX ? working->GetSizeY() : working->GetSizeX();
			if (frontExtent - frontMax > 1e-2f) ok = cutKeepLower(!depthAlongX, frontMax + pathHalf);
		}

		if (!ok) {
			// working是最后一次失败的裁剪之前的状态(仍然有效，没有被delete)，survivors是
			// 期间已经成功剥离出去的存活子块——如果candidate本身已经被中途的裁剪delete掉
			// (working != candidate)，pool[i]这个槽位就是悬空指针，必须换成working，不能
			// 直接留着candidate的旧值。
			pool[i] = working;
			for (Lot* s : survivors) pool.push_back(s);
			return false;
		}

		outPlaced->SetPosition(working->GetPosX(), working->GetPosY(), working->GetSizeX(), working->GetSizeY());
		if (outBoundaryRoads) *outBoundaryRoads = working->GetBoundaryRoads();

		pool.erase(pool.begin() + i);
		delete working;
		for (Lot* s : survivors) {
			pool.push_back(s);
		}
		return true;
	}

	return false;
}

namespace {
	// FillRemainder这次改用老工程的思路：先按权重CDF采样出一串(type,acreage)一直到填满某个
	// freeLots条目的总面积（对应老工程Map::GenerateBuildings的采样循环，
	// E:\Projects\Forever_UE\Source\Core\map\map.cpp第687-734行），再两两合并成一棵二叉树
	// （对应老工程Quad::DivideSpace的合并阶段，E:\Projects\Forever_UE\Source\Dependence\map\
	// geometry.cpp第706-730行），最后递归二分真实的Lot几何、把每个叶子安置到自己最终的矩形里
	// （对应老工程Quad::SplitInto+DivideSpace第732-751行）。只在这个文件内部使用，不进头文件。
	struct FillMergeNode {
		float acreage = 0.f;
		string type; // 只有叶子有意义；FILL_EMPTY_TYPE表示这段面积主动空置，不生成FillResult
		FillMergeNode* left = nullptr;
		FillMergeNode* right = nullptr;
		bool IsLeaf() const { return !left && !right; }
	};

	const string FILL_EMPTY_TYPE = "__EMPTY__";

	void DeleteFillMergeTree(FillMergeNode* node) {
		if (!node) return;
		DeleteFillMergeTree(node->left);
		DeleteFillMergeTree(node->right);
		delete node;
	}

	// 老工程采样循环搬过来：按candidates权重CDF反复抽类型，直到累计面积达到acreageBlock——
	// 只有"抽到但剩余空间连这个类型下限都放不下"才计入attempts(沿用老工程
	// MAX_ALLOCATION_ATTEMPTS=16的值)，抽到能放的类型永远不计入尝试次数，保证"填满"优先于
	// "尝试次数"，不会像原来贪心版本那样把成功的迭代也一起消耗进尝试预算。最后如果还差一点凑
	// 不满(通常是attempts耗尽)，补一个FILL_EMPTY_TYPE叶子把总面积账目补齐，这样递归二分时
	// 不会有游离在任何叶子之外的面积。
	vector<pair<string, float>> SampleFillElements(float acreageBlock,
		const vector<pair<string, float>>& candidates,
		const function<float(const string&)>& randomAcreage,
		const function<pair<float, float>(const string&)>& acreageMinMax) {

		vector<pair<string, float>> elements;
		float totalWeight = 0.f;
		for (auto& [type, w] : candidates) totalWeight += w;
		if (totalWeight <= 0.f) return elements;

		constexpr int MAX_SAMPLE_ATTEMPTS = 16;
		float acreageTmp = 0.f;
		int attempts = 0;
		while (acreageTmp < acreageBlock && attempts < MAX_SAMPLE_ATTEMPTS) {
			float r = GetRandom(10000) / 10000.f * totalWeight;
			string chosenType = candidates.back().first;
			float acc = 0.f;
			for (auto& [type, w] : candidates) {
				acc += w;
				if (r <= acc) { chosenType = type; break; }
			}

			float acreage = randomAcreage(chosenType);
			float minA = acreageMinMax(chosenType).first;
			if (acreageBlock - acreageTmp < minA) {
				attempts++;
				continue;
			}
			if (acreageBlock - acreageTmp < acreage) {
				acreage = acreageBlock - acreageTmp;
			}
			elements.emplace_back(chosenType, acreage);
			acreageTmp += acreage;
		}

		if (acreageBlock - acreageTmp > 1e-3f) {
			elements.emplace_back(FILL_EMPTY_TYPE, acreageBlock - acreageTmp);
		}
		return elements;
	}

	// 老工程Quad::DivideSpace合并阶段搬过来：按面积降序排序，反复把数组末尾(最小)两个元素
	// 合并成一个新内部节点(面积=两者之和)，插入排序塞回数组维持降序，直到只剩1或2个顶层节点
	// (只采样出一个元素时不需要合并，直接返回那一个叶子)。
	FillMergeNode* BuildFillMergeTree(vector<pair<string, float>>& elements) {
		vector<FillMergeNode*> nodes;
		nodes.reserve(elements.size());
		for (auto& [type, acreage] : elements) {
			FillMergeNode* leaf = new FillMergeNode();
			leaf->acreage = acreage;
			leaf->type = type;
			nodes.push_back(leaf);
		}
		sort(nodes.begin(), nodes.end(), [](FillMergeNode* a, FillMergeNode* b) {
			return a->acreage > b->acreage;
			});

		while (nodes.size() > 2) {
			FillMergeNode* merged = new FillMergeNode();
			merged->left = nodes[nodes.size() - 1];
			merged->right = nodes[nodes.size() - 2];
			merged->acreage = merged->left->acreage + merged->right->acreage;
			nodes.pop_back();

			int i = static_cast<int>(nodes.size()) - 2;
			for (; i >= 0; i--) {
				if (merged->acreage > nodes[i]->acreage) {
					nodes[i + 1] = nodes[i];
				}
				else {
					nodes[i + 1] = merged;
					break;
				}
			}
			if (i < 0) nodes[0] = merged;
		}

		if (nodes.empty()) return nullptr;
		if (nodes.size() == 1) return nodes[0];

		FillMergeNode* root = new FillMergeNode();
		root->left = nodes[0];
		root->right = nodes[1];
		root->acreage = root->left->acreage + root->right->acreage;
		return root;
	}

	// 老工程Quad::SplitInto+DivideSpace安置阶段搬过来：把region按node->left/right的面积比例
	// 递归二分，安置到最终矩形。和老工程零宽度切割线不同，这里每一刀都是真实的
	// Lot::SplitWithPath(带宽度的小路)，可能因为切割线两端都没有边界Road或分出来的某段小于
	// MIN_LOT_EXTENT而失败——优先按较长边选轴，切不动就换另一条轴再试一次，两条轴都切不动就
	// 说明这个节点没法再细分，把region整个让给面积更大的子树(另一支连同它的采样结果一起
	// 作废，不产生FillResult，DeleteFillMergeTree释放)，这是唯一的退化点，只在几何确实分不出
	// 两段合法矩形时才触发，不像原来贪心版本那样纯粹因为面积门槛就整块丢弃。
	void PlaceFillMergeNode(Lot* region, FillMergeNode* node, const PathLaneSpec& spec,
		vector<Lot::FillResult>& results, vector<PathRoadLink>& outLinks) {

		if (node->IsLeaf()) {
			if (node->type != FILL_EMPTY_TYPE) {
				results.push_back({ node->type, Quad(region->GetPosX(), region->GetPosY(),
					region->GetSizeX(), region->GetSizeY()), region->GetBoundaryRoads() });
			}
			delete region;
			delete node;
			return;
		}

		bool preferAlongX = region->GetSizeX() > region->GetSizeY();
		bool aIsLower = GetRandom(2) != 0;
		FillMergeNode* lowerChild = aIsLower ? node->left : node->right;
		FillMergeNode* upperChild = aIsLower ? node->right : node->left;
		float lowerRatio = lowerChild->acreage / node->acreage;

		auto tryAxis = [&](bool alongX) -> Lot::SplitResult {
			float depthExtent = alongX ? region->GetSizeX() : region->GetSizeY();
			return region->SplitWithPath(alongX, depthExtent * lowerRatio, spec);
			};

		Lot::SplitResult res = tryAxis(preferAlongX);
		if (!res.pathRoad) res = tryAxis(!preferAlongX);

		if (!res.pathRoad) {
			FillMergeNode* survivor = (node->left->acreage >= node->right->acreage) ? node->left : node->right;
			FillMergeNode* dropped = (survivor == node->left) ? node->right : node->left;
			DeleteFillMergeTree(dropped);
			delete node;
			PlaceFillMergeNode(region, survivor, spec, results, outLinks);
			return;
		}

		outLinks.push_back({ res.pathRoad, res.endRoad1, res.endT1, res.endRoad2, res.endT2 });
		delete region;
		delete node;
		PlaceFillMergeNode(res.lowerLot, lowerChild, spec, results, outLinks);
		PlaceFillMergeNode(res.upperLot, upperChild, spec, results, outLinks);
	}
}

vector<Lot::FillResult> Lot::FillRemainder(const PathLaneSpec& spec,
	const function<float(const string&)>& randomAcreage,
	const function<pair<float, float>(const string&)>& acreageMinMax) {

	vector<FillResult> results;
	if (candidates.empty()) return results;

	auto& pool = GetFreeLots();

	// 每个满足MIN_LOT_EXTENT的freeLots条目当成一个独立容器，各自跑一遍"采样填满->合并->
	// 递归二分"，互不共享采样进度(对应"通过权重填满所有子lot")；不满足MIN_LOT_EXTENT的条目
	// (某边小于2，纯几何意义上切不出东西)保留在pool原地不动。
	vector<Lot*> toProcess;
	for (size_t i = 0; i < pool.size(); ) {
		if (pool[i]->GetSizeX() >= MIN_LOT_EXTENT && pool[i]->GetSizeY() >= MIN_LOT_EXTENT) {
			toProcess.push_back(pool[i]);
			pool.erase(pool.begin() + i);
		}
		else {
			i++;
		}
	}

	vector<PathRoadLink> newLinks;
	for (Lot* target : toProcess) {
		vector<pair<string, float>> elements = SampleFillElements(target->GetAcreage(), candidates, randomAcreage, acreageMinMax);
		if (elements.empty()) {
			delete target;
			continue;
		}
		FillMergeNode* root = BuildFillMergeTree(elements);
		PlaceFillMergeNode(target, root, spec, results, newLinks);
	}

	pathRoadLinks.insert(pathRoadLinks.end(), newLinks.begin(), newLinks.end());
	return results;
}

const vector<PathRoadLink>& Lot::GetPathRoadLinks() const {
	return pathRoadLinks;
}

vector<Road*> Lot::GetPathRoads() const {
	vector<Road*> result;
	result.reserve(pathRoadLinks.size());
	for (const PathRoadLink& link : pathRoadLinks) {
		result.push_back(link.road);
	}
	return result;
}

void Lot::AddCandidate(const string& type, float weight) {
	candidates.emplace_back(type, weight);
}

const vector<pair<string, float>>& Lot::GetCandidates() const {
	return candidates;
}

void Lot::ClearCandidates() {
	candidates.clear();
}
