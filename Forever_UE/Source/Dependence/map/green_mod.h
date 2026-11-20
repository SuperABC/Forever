#pragma once

#include "common/mod_test.h"
#include "common/mod_state.h"
#include "map/feature_spec.h"
#include "map/general_geometry.h"

#include <string>
#include <vector>


// Core把一块绿化多边形交给mod的只读输入，设计见green_mod.md。
struct GreenShape {
	std::vector<float> xy;       // 顶点{x0,y0,x1,y1,...}，地图单位(1=10m)，有符号面积>0，首尾不重复，可以是凹多边形
	float minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f; // 包围盒
	bool linked = false;         // 是否有至少一条边连到了真实道路
};

// 市民活动圆(绿化局部=世界坐标，地图单位)。
struct GreenCircle {
	float x = 0.f, y = 0.f, r = 0.f;
};

// 设计见green_mod.md。
class GreenMod {
public:
	GreenMod() = default;
	virtual ~GreenMod() = default;

	virtual const char* GetType() const = 0;

	virtual const char* GetName() = 0;

	// 单元测试：print输出关键信息，返回是否通过。没有深度测试的mod用FOREVER_MOD_GENERIC_UNIT_TEST(见common/mod_test.h)。
	virtual bool UnitTest(TestPrintFn print) = 0;

	// Core在绿化区落地后调用一次：基类实现把shape拷进本mod自己的成员(在mod所在DLL里执行，
	// 容器的分配器不会错位)、清空上一次的输出，再调LayoutGreen()。子类不覆盖这个函数。
	virtual void Layout(const GreenShape& shape);

	// 子类在这里布置：AddCircle/FindLargestCircle指定活动圆，AddDecoration/AddTree*摆放植被和模型。
	// 此时polygon/min*/max*/linked都已就绪。
	virtual void LayoutGreen() {}

	// ---- 输入(Layout时拷入，LayoutGreen里只读) ----
	std::vector<float> polygon;
	float minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f;
	bool linked = false;

	// ---- 输出(值类型，Core读走快照) ----
	// 市民活动圆。Core会按"接路的边+圆心到边的最短连线必须在多边形内"过滤，保留下来的重新编号。
	std::vector<GreenCircle> circles;
	void AddCircle(float x, float y, float r);

	// 自定义模型：position是相对**包围盒**(原点西北角，+X东+Y南，PointParams的ratio相对包围盒宽高)的位置，
	// 不旋转；Core换算成世界坐标后丢弃落在多边形外的。
	void AddDecoration(const DecorationSpec& spec);
	std::vector<DecorationSpec> decorations;

	// 树请求，参考系同上(包围盒)；Core展开时丢弃多边形外的点。
	void AddTreeSingle(const PointParams& position, float scale, const std::string& meshPath);
	void AddTreeLine(const PointParams& from, const PointParams& to, int num, float scale,
		const std::string& meshPath);
	void AddTreeArea(const PointParams& from, const PointParams& to, int num, float scale,
		const std::string& meshPath);
	TreeRequests trees;

	// ---- 工具(非虚，编进各自DLL，内部用GetRandom，Core在mod实例的RandomScope里调Layout) ----
	// 当前多边形内、避开已加入circles的圆之外还能放下的最大圆。找不到(剩余空间小于0.01)返回false，
	// 不会自动加进circles，调用方决定半径(可缩小)后AddCircle。
	bool FindLargestCircle(float& cx, float& cy, float& r);

	// 多边形内均匀随机一点。首次调用耳切三角化并缓存，之后每次O(log t)、没有拒绝采样。
	void RandomPoint(float& x, float& y);

	// 多边形面积(地图单位²，1单位=10m)。
	float GetPolygonArea() const;

	// 子类还必须实现两个static查询(基类不声明)并通过RegisterGreen注册：
	//   static float GetPowerFree();   没连路网的绿化区的权重
	//   static float GetPowerLinked(); 连了路网的绿化区的权重
	// 权重0=这类绿化区不选它。见green_mod.md。

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}

private:
	// RandomPoint的缓存：三角形顶点下标+面积前缀和，Layout时清空。
	std::vector<int> triangleIndices;
	std::vector<float> triangleCumulative;
	bool triangulated = false;
};
