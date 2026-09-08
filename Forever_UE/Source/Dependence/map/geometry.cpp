#include "geometry.h"

#include <cmath>
#include <string>
#include <vector>
#include <algorithm>


using namespace std;

// DivideSpace递归过程中代表"尚未定位的合并子树"的临时矩形
class Space : public Quad {
public:
	// 由两个待合并子矩形构造，面积为两者之和
	Space(Quad* r1, Quad* r2);

	// 第一个子矩形
	Quad* r1;

	// 第二个子矩形
	Quad* r2;

	// 落位后的四角四边
	QuadBoundary boundary;
};

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
	unit(other.unit) {

}

Road& Road::operator=(const Road& other) {
	if (this != &other) {
		Connection::operator=(other);
		name = other.name;
		mesh = other.mesh;
		unit = other.unit;
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

void QuadBoundary::Invalidate(const vector<Connection*>& removed) {
	for (int i = 0; i < 4; i++) {
		if (find(removed.begin(), removed.end(), edges[i]) != removed.end()) {
			edges[i] = nullptr;
		}
	}
}

Space::Space(Quad* r1, Quad* r2) :
	r1(r1),
	r2(r2),
	boundary() {
	acreage = r1->GetAcreage() + r2->GetAcreage();
}

void Quad::RecordBoundary(Quad* elem, const QuadBoundary& boundary, unordered_map<Quad*, QuadBoundary>& outElementBoundaries) {
	if (auto space = dynamic_cast<Space*>(elem)) {
		space->boundary = boundary;
	}
	else {
		outElementBoundaries[elem] = boundary;
	}
}

void Quad::SplitInto(float left, float right, float bottom, float top, const QuadBoundary& boundary,
	Quad* a, Quad* b, const function<pair<float, float>(float, float)>& toWorld, string category,
	vector<Node*>& outNewNodes, vector<Connection*>& outNewConnections, vector<Connection*>& outRemovedConnections,
	unordered_map<Quad*, QuadBoundary>& outElementBoundaries) {

	bool aIsLower = GetRandom(2) != 0;
	Quad* lowerElem = aIsLower ? a : b;
	Quad* upperElem = aIsLower ? b : a;
	float lowerAcreage = lowerElem->GetAcreage();
	float totalAcreage = a->GetAcreage() + b->GetAcreage();

	QuadBoundary lowerBoundary, upperBoundary;

	// 沿X分割为左右两部分，否则沿Y分割为上下两部分
	if (right - left > top - bottom) {
		float divX = left + (right - left) * lowerAcreage / totalAcreage;
		if (abs(divX - left) < 3.f) divX = left;
		if (abs(divX - right) < 3.f) divX = right;
		lowerElem->SetVertices(left, bottom, divX, top);
		upperElem->SetVertices(divX, bottom, right, top);

		auto [bx, by] = toWorld(divX, bottom);
		auto [tx, ty] = toWorld(divX, top);
		Node* splitBottom = new Node(category, bx, by);
		Node* splitTop = new Node(category, tx, ty);
		outNewNodes.push_back(splitBottom);
		outNewNodes.push_back(splitTop);

		Connection* topLeftEdge = new Connection(*boundary.corners[3], *splitTop);
		Connection* topRightEdge = new Connection(*splitTop, *boundary.corners[2]);
		Connection* bottomLeftEdge = new Connection(*splitBottom, *boundary.corners[0]);
		Connection* bottomRightEdge = new Connection(*boundary.corners[1], *splitBottom);
		Connection* dividerEdge = new Connection(*splitBottom, *splitTop);
		outNewConnections.push_back(topLeftEdge);
		outNewConnections.push_back(topRightEdge);
		outNewConnections.push_back(bottomLeftEdge);
		outNewConnections.push_back(bottomRightEdge);
		outNewConnections.push_back(dividerEdge);
		if (boundary.edges[3]) outRemovedConnections.push_back(boundary.edges[3]);
		if (boundary.edges[2]) outRemovedConnections.push_back(boundary.edges[2]);

		lowerBoundary.corners[0] = boundary.corners[0];
		lowerBoundary.corners[1] = splitBottom;
		lowerBoundary.corners[2] = splitTop;
		lowerBoundary.corners[3] = boundary.corners[3];
		lowerBoundary.edges[0] = boundary.edges[0];
		lowerBoundary.edges[1] = dividerEdge;
		lowerBoundary.edges[2] = bottomLeftEdge;
		lowerBoundary.edges[3] = topLeftEdge;

		upperBoundary.corners[0] = splitBottom;
		upperBoundary.corners[1] = boundary.corners[1];
		upperBoundary.corners[2] = boundary.corners[2];
		upperBoundary.corners[3] = splitTop;
		upperBoundary.edges[0] = dividerEdge;
		upperBoundary.edges[1] = boundary.edges[1];
		upperBoundary.edges[2] = bottomRightEdge;
		upperBoundary.edges[3] = topRightEdge;
	}
	else {
		float divY = bottom + (top - bottom) * lowerAcreage / totalAcreage;
		if (abs(divY - bottom) < 3.f) divY = bottom;
		if (abs(divY - top) < 3.f) divY = top;
		lowerElem->SetVertices(left, divY, right, top);
		upperElem->SetVertices(left, bottom, right, divY);

		auto [lx, ly] = toWorld(left, divY);
		auto [rx, ry] = toWorld(right, divY);
		Node* splitLeft = new Node(category, lx, ly);
		Node* splitRight = new Node(category, rx, ry);
		outNewNodes.push_back(splitLeft);
		outNewNodes.push_back(splitRight);

		Connection* leftTopEdge = new Connection(*boundary.corners[3], *splitLeft);
		Connection* leftBottomEdge = new Connection(*splitLeft, *boundary.corners[0]);
		Connection* rightTopEdge = new Connection(*boundary.corners[2], *splitRight);
		Connection* rightBottomEdge = new Connection(*splitRight, *boundary.corners[1]);
		Connection* dividerEdge = new Connection(*splitLeft, *splitRight);
		outNewConnections.push_back(leftTopEdge);
		outNewConnections.push_back(leftBottomEdge);
		outNewConnections.push_back(rightTopEdge);
		outNewConnections.push_back(rightBottomEdge);
		outNewConnections.push_back(dividerEdge);
		if (boundary.edges[1]) outRemovedConnections.push_back(boundary.edges[1]);
		if (boundary.edges[0]) outRemovedConnections.push_back(boundary.edges[0]);

		lowerBoundary.corners[0] = splitLeft;
		lowerBoundary.corners[1] = splitRight;
		lowerBoundary.corners[2] = boundary.corners[2];
		lowerBoundary.corners[3] = boundary.corners[3];
		lowerBoundary.edges[0] = leftTopEdge;
		lowerBoundary.edges[1] = rightTopEdge;
		lowerBoundary.edges[2] = dividerEdge;
		lowerBoundary.edges[3] = boundary.edges[3];

		upperBoundary.corners[0] = boundary.corners[0];
		upperBoundary.corners[1] = boundary.corners[1];
		upperBoundary.corners[2] = splitRight;
		upperBoundary.corners[3] = splitLeft;
		upperBoundary.edges[0] = leftBottomEdge;
		upperBoundary.edges[1] = rightBottomEdge;
		upperBoundary.edges[2] = boundary.edges[2];
		upperBoundary.edges[3] = dividerEdge;
	}

	RecordBoundary(lowerElem, lowerBoundary, outElementBoundaries);
	RecordBoundary(upperElem, upperBoundary, outElementBoundaries);
}

void Quad::DivideSpace(vector<Quad*>& elements, const QuadBoundary& boundary,
	const function<pair<float, float>(float, float)>& toWorld, string category,
	vector<Node*>& outNewNodes, vector<Connection*>& outNewConnections,
	vector<Connection*>& outRemovedConnections, unordered_map<Quad*, QuadBoundary>& outElementBoundaries) {
	if (elements.empty()) return;

	for (int i = 0; i < 4; i++) {
		if (!boundary.corners[i] || !boundary.edges[i]) {
			THROW_EXCEPTION(NullPointerException, "DivideSpace boundary must have 4 valid corners and edges.\n");
		}
	}

	sort(elements.begin(), elements.end(), [](Quad* a, Quad* b) {
		return a->GetAcreage() > b->GetAcreage();
	});

	if (elements.size() == 1) {
		elements[0]->SetPosition(posX, posY, sizeX, sizeY);
		outElementBoundaries[elements[0]] = boundary;
		return;
	}

	while (elements.size() > 2) {
		Space* tmp = new Space(elements[elements.size() - 1], elements[elements.size() - 2]);
		elements.pop_back();
		int i = static_cast<int>(elements.size()) - 2;
		for (; i >= 0; i--) {
			if (tmp->GetAcreage() > elements[i]->GetAcreage()) {
				elements[i + 1] = elements[i];
			}
			else {
				elements[i + 1] = tmp;
				break;
			}
		}
		if (i < 0) elements[0] = tmp;
	}

	SplitInto(GetLeft(), GetRight(), GetBottom(), GetTop(), boundary, elements[0], elements[1], toWorld,
		category, outNewNodes, outNewConnections, outRemovedConnections, outElementBoundaries);

	while (!elements.empty()) {
		auto tmp = elements.back();
		elements.pop_back();
		if (auto chunk = dynamic_cast<Space*>(tmp)) {
			Quad* rect1 = chunk->r1;
			Quad* rect2 = chunk->r2;

			if (tmp->GetAcreage() > 0) {
				SplitInto(tmp->GetLeft(), tmp->GetRight(), tmp->GetBottom(), tmp->GetTop(), chunk->boundary, rect1, rect2, toWorld,
					category, outNewNodes, outNewConnections, outRemovedConnections, outElementBoundaries);

				if (dynamic_cast<Space*>(rect1)) elements.push_back(rect1);
				if (dynamic_cast<Space*>(rect2)) elements.push_back(rect2);
			}
			delete chunk;
		}
	}
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

Lot::Lot(Node n1, Node n2, Node n3, vector<float> margin) :
	Quad(),
	rotation(0.f),
	area(AREA_NONE) {
	SetPosition(n1, n2, n3, margin);
}

Lot::Lot(Node n1, Node n2, Node n3, Node n4, vector<float> margin) :
	Quad(),
	rotation(0.f),
	area(AREA_NONE) {
	SetPosition(n1, n2, n3, n4, margin);
}

Lot::~Lot() {

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
