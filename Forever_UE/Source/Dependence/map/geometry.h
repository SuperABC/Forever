#pragma once

#include "../common/utility.h"
#include "../common/error.h"

#include <vector>
#include <unordered_map>
#include <functional>

// 由长宽计算默认面积时的换算系数
#define ACREAGE_SCALE_FACTOR 100.f


// 边方向
enum FACE_DIRECTION : int {
	FACE_WEST,
	FACE_EAST,
	FACE_NORTH,
	FACE_SOUTH
};

// 矩形角点方向
enum CORNER_DIRECTION : int {
	CORNER_WEST_NORTH = 0,
	CORNER_EAST_NORTH = 1,
	CORNER_EAST_SOUTH = 2,
	CORNER_WEST_SOUTH = 3
};

class Node {
public:
	// 禁止默认构造
	Node() = delete;

	// 显式构造分配新id
	explicit Node(std::string category, float x, float y, float z = 0.f);

	// 拷贝构造使用相同id
	Node(const Node& other);

	// 赋值构造使用相同id
	Node& operator=(const Node& other);

	// 无析构
	~Node();

	// 获取id
	int GetId() const;

	// 获取坐标x
	float GetX() const;

	// 获取坐标y
	float GetY() const;

	// 获取坐标z
	float GetZ() const;

	// 获取类别
	std::string GetCategory() const;

	// 获取计数器
	static int GetCount();

	// 设置计数器
	static void SetCount(int c);

private:
	// 唯一id
	int id;

	// 坐标x
	float posX;

	// 坐标y
	float posY;

	// 坐标z
	float posZ;

	std::string category;

	// id分配计数器
	static int count;
};

class Connection {
public:
	// 禁止默认构造
	Connection() = delete;

	// 构造连接
	explicit Connection(Node n1, Node n2, float begin = 0.0f, float end = 1.0f);

	// 拷贝构造新创建nodes
	Connection(const Connection& other);

	// 赋值构造新创建nodes
	Connection& operator=(const Connection& other);

	// 无析构
	~Connection();

	// 添加控制点
	void AddControls(std::vector<std::pair<Node, float>> controls);

	// 获取控制点及其权重（拷贝）
	std::vector<std::pair<Node, float>> GetControls() const;

	// 根据n1.id, n2.id判断是否相同
	bool operator==(const Connection& other) const;

	// 获取start对应的起点（非实际端点)
	Node GetStart() const;

	// 获取end对应的终点（非实际端点)
	Node GetEnd() const;

	// 获取任一连线点
	Node GetPoint(float f) const;

	// 获取任一连线点处的切线方向（非单位向量，方向及模长由曲线参数化决定）
	void GetTangent(float f, float& dx, float& dy, float& dz) const;

	// 获取begin和end之间的距离（非两端点距离)
	float CalcDistance() const;

	// 获连线任一两点距离
	float CalcDistance(float f1, float f2) const;

protected:
	// 起点
	OBJECT_HOLDER Node* beginVertex;

	// 终点
	OBJECT_HOLDER Node* endVertex;

	// 起点参数
	float begin;

	// 终点参数
	float end;

	// 控制点及其权重
	OBJECT_HOLDER std::vector<std::pair<Node*, float>> controlVertices;

private:
	// 弧长参数表缓存，惰性计算；控制点变化后由AddControls清空
	mutable std::vector<double> arcLengthCache;

	// 按弧长比例f反查对应的原始Bezier多项式参数u，首次调用时惰性建表并缓存到arcLengthCache
	double ResolveArcLengthParam(const std::vector<std::pair<Node*, float>>& allPoints, int m, float f) const;
};

class Intersection : public Node {
public:
	// 禁止默认构造
	Intersection() = delete;

	// 显式构造分配新id
	explicit Intersection(float x, float y, float z = 0.f);

	// 由Node构造，使用相同id
	explicit Intersection(const Node& node);

	// 拷贝构造使用相同id
	Intersection(const Intersection& other);

	// 赋值构造使用相同id
	Intersection& operator=(const Intersection& other);

	// 无析构
	~Intersection();
};

class Road : public Connection {
public:
	// 禁止默认构造
	Road() = delete;

	// 构造连接
	explicit Road(std::string name, Node n1, Node n2, std::string mesh, float unit, float begin = 0.0f, float end = 1.0f);

	// 由Connection构造，附加名称与半径
	explicit Road(const Connection& connection, std::string name, std::string mesh, float unit);

	// 拷贝构造新创建nodes
	Road(const Road& other);

	// 赋值构造新创建nodes
	Road& operator=(const Road& other);

	// 无析构
	~Road();

	// 获取道路名称
	std::string GetName() const;

	// 获取道路3D资产路径
	std::string GetMesh() const;

	// 获取道路mesh基准长度
	float GetUnit() const;

private:
	// 道路名称
	std::string name;

	// 道路3D资产路径
	std::string mesh;

	// 道路mesh基准长度
	float unit;
};

struct QuadBoundary {
	// 四角节点
	Node* corners[4] = { nullptr, nullptr, nullptr, nullptr };

	// 四边连接
	Connection* edges[4] = { nullptr, nullptr, nullptr, nullptr };

	/*
	* 将edges里恰好出现在removed中的边引用置空（角点不会失效，只有被切断的边会失效）
	* @removed: DivideSpace本次划分输出的失效连接列表
	*/
	void Invalidate(const std::vector<Connection*>& removed);
};

class Quad {
public:
	// 构造空矩形
	Quad();

	// 构造矩形
	Quad(float x, float y, float w, float h);

	// 无析构
	virtual ~Quad();

	// 获取中心点坐标x
	float GetPosX() const;

	// 设置中心点坐标x
	void SetPosX(float x);

	// 获取中心点坐标y
	float GetPosY() const;

	// 设置中心点坐标y
	void SetPosY(float y);

	// 获取长宽尺寸x
	float GetSizeX() const;

	// 设置长宽尺寸x
	void SetSizeX(float w);

	// 获取长宽尺寸y
	float GetSizeY() const;

	// 设置长宽尺寸y
	void SetSizeY(float h);

	// 获取左边坐标x
	float GetLeft() const;

	// 获取右边坐标x
	float GetRight() const;

	// 获取下边坐标y
	float GetBottom() const;

	// 获取上边坐标y
	float GetTop() const;

	// 通过四边坐标指定位置
	void SetVertices(float x1, float y1, float x2, float y2);

	// 通过中心和尺寸指定位置
	void SetPosition(float x, float y, float w, float h);

	// 获取面积
	float GetAcreage() const;

	// 设置面积（在指定坐标之前设置期望面积）
	void SetAcreage(float a);

	/*
	* 递归分割内部空间，并记录分割过程中产生/失效的导航节点与连接
	* @elements: 待分割摆放的子矩形元素
	* @boundary: 当前矩形已有的四角四边（非持有引用，不会被回收）
	* @toWorld: 将矩形局部坐标转换为世界坐标的方法，仅在创建新节点时调用
	* @outNewNodes, outNewConnections: 本次调用新创建的节点与连接（追加写入）
	* @outRemovedConnections: 因被切分而失效、需要从导航图移除并释放的旧连接（追加写入）
	* @outElementBoundaries: 每个叶子元素最终所在的四角四边（按元素指针索引）
	*/
	void DivideSpace(std::vector<Quad*>& elements, const QuadBoundary& boundary,
		const std::function<std::pair<float, float>(float, float)>& toWorld, std::string category,
		std::vector<Node*>& outNewNodes, std::vector<Connection*>& outNewConnections,
		std::vector<Connection*>& outRemovedConnections, std::unordered_map<Quad*, QuadBoundary>& outElementBoundaries);

protected:
	// 中心点坐标x
	float posX;

	// 中心点坐标y
	float posY;

	// 长宽尺寸x
	float sizeX;

	// 长宽尺寸y
	float sizeY;

	// 面积
	float acreage;

private:
	/*
	* 记录elem最终落位的边界：若elem是尚待继续分割的Space则写入其boundary字段，否则写入叶子元素的输出表
	* @elem: 待记录边界的元素
	* @boundary: elem最终落位的四角四边
	* @outElementBoundaries: 叶子元素最终所在的四角四边（按元素指针索引）
	*/
	static void RecordBoundary(Quad* elem, const QuadBoundary& boundary,
		std::unordered_map<Quad*, QuadBoundary>& outElementBoundaries);

	/*
	* 将boundary范围内的a、b两个子矩形按面积比沿长边二分定位，记录切分产生的新节点与新旧连接
	* @left, right, bottom, top: 当前待分割区域的边界
	* @boundary: 当前区域已有的四角四边
	* @a, b: 待定位的两个子矩形
	* @toWorld: 将矩形局部坐标转换为世界坐标的方法，仅在创建新节点时调用
	* @outNewNodes, outNewConnections: 本次调用新创建的节点与连接（追加写入）
	* @outRemovedConnections: 因被切分而失效、需要从导航图移除并释放的旧连接（追加写入）
	* @outElementBoundaries: 每个叶子元素最终所在的四角四边（按元素指针索引）
	*/
	static void SplitInto(float left, float right, float bottom, float top, const QuadBoundary& boundary,
		Quad* a, Quad* b, const std::function<std::pair<float, float>(float, float)>& toWorld, std::string category,
		std::vector<Node*>& outNewNodes, std::vector<Connection*>& outNewConnections,
		std::vector<Connection*>& outRemovedConnections,
		std::unordered_map<Quad*, QuadBoundary>& outElementBoundaries);
};

enum AREA_TYPE : int {
	AREA_NONE,
	AREA_RESIDENTIAL_HIGH,
	AREA_RESIDENTIAL_MIDDLE,
	AREA_RESIDENTIAL_LOW,
	AREA_COMMERCIAL_HIGH,
	AREA_COMMERCIAL_MIDDLE,
	AREA_COMMERCIAL_LOW,
	AREA_INDUSTRIAL_HIGH,
	AREA_INDUSTRIAL_MIDDLE,
	AREA_INDUSTRIAL_LOW,
	AREA_OFFICIAL_HIGH,
	AREA_OFFICIAL_MIDDLE,
	AREA_OFFICIAL_LOW,
	AREA_GREEN,
	AREA_END
};

class Lot : public Quad {
public:
	// 构造空地块
	Lot();

	// 根据中心点和长宽构造地块
	Lot(float x, float y, float w, float h, float r);

	// 根据连续三个端点构造地块
	Lot(Node n1, Node n2, Node n3, std::vector<float> margin = std::vector<float>(4, 0.f));

	// 根据连续四个端点构造地块
	Lot(Node n1, Node n2, Node n3, Node n4, std::vector<float> margin = std::vector<float>(4, 0.f));

	// 无析构
	virtual ~Lot();

	// 获取旋转
	float GetRotation() const;

	// 设置旋转
	void SetRotation(float r);

	// 获取地块类型
	AREA_TYPE GetArea() const;

	// 设置地块类型
	void SetArea(AREA_TYPE area);

	// 获取端点坐标（idx 按 CORNER_DIRECTION: 0=TL, 1=TR, 2=BR, 3=BL）
	std::pair<float, float> GetVertex(int idx) const;

	// 获取矩形内任意点坐标
	std::pair<float, float> GetPosition(float x, float y) const;

	// 引入基类的 float 版本，避免名称隐藏
	using Quad::SetPosition;

	// 通过逆时针顺序三个顶点设置矩形
	void SetPosition(Node n1, Node n2, Node n3, const std::vector<float>& margin);

	// 通过顺序无关四个顶点设置矩形
	void SetPosition(Node n1, Node n2, Node n3, Node n4, const std::vector<float>& margin);

protected:
	// 旋转角度
	float rotation;

	// 地块类型
	AREA_TYPE area;
};

