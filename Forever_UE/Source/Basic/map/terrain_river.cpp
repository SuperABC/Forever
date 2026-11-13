#include "terrain_river.h"

#include "common/utility.h"

#include <vector>
#include <cmath>
#include <algorithm>
#include <tuple>
#include <unordered_set>
#include <string>

// 海平面水位(地图元素单位)：Ocean/River共用一个常量，分开写会让河口出现河水/海水的落差。
#define SEA_LEVEL -0.1f

using namespace std;

// 八方向邻居偏移量——Ocean/Mountain/River三个地形都要用，同样放在文件顶部。
static const int neighborOffsets[8][2] = { {1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1} };

// ============================================================================
// RiverTerrain：山体内D8单流向流量累积确定起点，流出山体后按角度/角速度模型继续蔓延，
// 全有或全无(只有流到地图边界/海洋/并入已有河才保留)。详见terrain_river.md
// "RiverTerrain"一节。
// ============================================================================

// 累积流量超过这个值的山体格子才有资格成为河流起点(起始值，待按效果调)。
#define RIVER_FLOW_THRESHOLD 30.0f

// 新河流起点与所有已有起点的最小距离(element单位，1 element=10m，200=2000m)——起点挤得太近会长出
// 一堆紧挨着的平行短河。
#define RIVER_ORIGIN_MIN_SPACING 200.0f

// 流出山体之后，流向用角度(弧度)表示，偏向速度是角速度(标量，不是向量)——只保留偏向
// 速度这一层，不再叠加偏向加速度(导数阶数太高，行为难控制)。每步直接给角速度加一个
// 带正负号的随机小量(弧度/步)，角度按角速度积分更新。先给个起始值，看完效果再调。
#define RIVER_ANGULAR_VEL_JITTER 0.015f

// 角速度本身夹的上限(弧度/步)，防止扰动累积出转得太快、路径碎成一团——角速度是没有
// 均值回归的纯随机游走，夹了上限不代表只是偶尔碰一下，实际会在边界附近停留很长时间。
#define RIVER_ANGULAR_VEL_MAX 0.02f

// 沿流向每次前进的距离(element单位)，按这个步长采样落在哪个格子上。
#define RIVER_SAMPLE_STEP 0.5f

// 第三阶段：河流格子的河床比原地形低多少(地图元素单位)。水位不用单独的偏移常量——
// 直接用"原地形高度+SEA_LEVEL"，和海洋同一个公式，天然在河口处对上海平面。
#define RIVER_HEIGHT_DROP 0.5f

// 出山口附近坡度检查：山体内路径里连续两格(更靠源头-更靠山脚)的高度差超过这个值就截断，
// 河流起点改到截断点，不让河流从很陡的地方发源。
#define RIVER_MAX_STEP_DROP 0.2f

// 河岸平滑范围(格数)——非河流格子距河流越近，高度下降得越多(线性过渡)，超过这个距离
// 完全不下降，先给个起始值。
#define RIVER_BLUR_RADIUS 2

// 到河道距离的搜索半径(格数)——比RIVER_BLUR_RADIUS+1再多1格，河岸细化高度对距离场做4x4
// Catmull-Rom插值时，模板最外一圈也要拿到真实距离；更远的格子统一记成这个值(早已降为0)。
#define RIVER_BANK_SEARCH_RADIUS (RIVER_BLUR_RADIUS + 2)

// 河道加宽起始半径——1.5刚好能把3x3内的8个邻居(对角距离sqrt(2)≈1.414)全部纳入，
// 对应原来固定宽度3的效果，作为"流程=0"时的基准半径。
#define RIVER_BASE_WIDEN_RADIUS 1.5f

// 河道每流经这么多格(riverDistance)，加宽半径增加0.5——对称膨胀一次增加的是
// 直径(两侧各加一点)，半径+0.5相当于宽度+1，所以是"每200格宽度+1"。
#define RIVER_WIDTH_GROW_PER_CELLS 200.0f

// 碰到海之后顺着原流向继续往海里推这么多格(element单位)，强制多接几格真海洋格子进
// 河流——海洋贴着陆地那一圈其实是"沙滩"(高度固定0，只比固定水位高一点)，只接触到的
// 这一两格可能还没真正越过沙滩，往深一点的海里多推几格，确保河床高度公式切过沙滩。
#define RIVER_MOUTH_EXTEND_CELLS 4.0f

// 河流中心线离海岸线(横竖四个方向任一方向上)不到这么多格(element单位，10=100m)就开始往海的
// 方向拐，不让河擦着海岸线走；每一步转"当前流向与海方向夹角×RIVER_COAST_STEER_COEF"。
#define RIVER_COAST_AVOID_DISTANCE 10.0f

// 别的河(不属于自己这条链路)同理，但检测距离只有4格(40m)——往那条河拐，最终靠近到8邻居范围内
// 走原来的"并入别的河"收尾。拐的比例沿用RIVER_COAST_STEER_COEF。
#define RIVER_NEIGHBOR_AVOID_DISTANCE 4.0f
#define RIVER_COAST_STEER_COEF 0.0625f

// 地图正中心留给井字路网中心lot(AREA_TEST，见roadnet_jing.cpp)用——河流只要
// 流经地图中心这片区域(宽高方向各3/8~5/8那一块)，不管是山体内还是流出山体之后，整条河
// (两段一起)直接作废，不让河流占掉地图中心那块地。
#define RIVER_CENTER_MIN_FRAC (3.0f / 8.0f)
#define RIVER_CENTER_MAX_FRAC (5.0f / 8.0f)

// 一条链路(从起点到并入别的河/入海/出地图)自己标记的中心线格子数少于这个值，就整条撤销——
// 在加宽之前按中心线判断，避免地图上到处是很短的小河段。
#define RIVER_MIN_LENGTH 100

int RiverTerrain::count = 0;

RiverTerrain::RiverTerrain() : id(count++) {

}

RiverTerrain::~RiverTerrain() {

}

const char* RiverTerrain::GetId() {
	return "terrain_river";
}

const char* RiverTerrain::GetType() const {
	return "terrain_river";
}

const char* RiverTerrain::GetName() {
	name = "河流地形" + to_string(id);
	return name.data();
}

float RiverTerrain::GetPriority() const {
	// 必须低于Mountain(0.95)和Ocean(0.9)——DistributeTerrain跑到这里时要能读到两者
	// 写完之后的最终地形分类。
	return 0.5f;
}

void RiverTerrain::SetupTexture() {
	// 和平地同一张贴图：河道靠地形凹陷+水面mesh表现，不需要地表材质额外强调。
	diffusePath = "/Game/Asset/Terrain/Plain/PlainDiffuse.PlainDiffuse";
	// normalPath/roughnessPath留空——现有fallback逻辑会填平的法线+中性粗糙度
}

void RiverTerrain::DistributeTerrain(int width, int height,
	const function<const string&(int, int)>& getTerrain, const function<bool(int, int, const string&)>& setTerrain,
	const function<float(int, int)>& getHeight, const function<bool(int, int, float)>& setHeight,
	const function<bool(int, int, bool, float)>& setWater,
	const function<bool(int, int, const float*)>& setFineHeights) const {

	int cellCount = width * height;
	vector<float> heights(cellCount);
	vector<bool> isMountain(cellCount);
	vector<int> mountainIndices;
	mountainIndices.reserve(cellCount);

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int idx = y * width + x;
			heights[idx] = getHeight(x, y);
			isMountain[idx] = (getTerrain(x, y) == "terrain_mountain");
			if (isMountain[idx]) mountainIndices.push_back(idx);
		}
	}

	// 按高度降序结算D8:轮到每个山体格子时,它从更高邻居那里该收到的流量已经全部收到了
	// (更高的格子必然排在它前面处理)。
	sort(mountainIndices.begin(), mountainIndices.end(),
		[&heights](int a, int b) { return heights[a] > heights[b]; });

	vector<float> accum(cellCount, 0.0f);
	for (int idx : mountainIndices) accum[idx] = 1.0f;
	// 每个山体格子自己的D8下坡目标(8邻居里最低的那个山体格子)，流到非山体就停的格子记-1——
	// 和accum同一个条件判断，流量累积"要不要往外送"和河流"要不要往下蔓延"走的是同一条路径。
	vector<int> flowTarget(cellCount, -1);
	// 8邻居里绝对最低的那个,不管是不是山体都记下来——流到非山体就停的格子,flowTarget是-1,
	// 但exitTarget还留着"原本想流去哪个非山体格子",供流出山体之后的第二阶段续上。
	vector<int> exitTarget(cellCount, -1);

	for (int idx : mountainIndices) {
		int x = idx % width;
		int y = idx / width;
		// 比较的是坡度(高度差/距离)不是原始高度——对角邻居比轴向邻居远sqrt(2)倍,直接比
		// 原始高度会让对角方向系统性"看起来更低"(同样的坡度,对角方向覆盖的水平距离更长、
		// 高度差自然更大)，结果所有流向都挤成45度斜线，转弯也只能在4个对角方向之间跳、
		// 全是90度直角。按坡度比才能让轴向/对角公平竞争。
		float bestSlope = 0.f; // 必须严格>0才算"更低"，和原来heights[nIdx]<heights[idx]同一个门槛
		int lowestIdx = -1;
		for (auto& neighborOffset : neighborOffsets) {
			int nx = x + neighborOffset[0];
			int ny = y + neighborOffset[1];
			if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
			int nIdx = ny * width + nx;
			float dist = (neighborOffset[0] != 0 && neighborOffset[1] != 0) ? 1.41421356f : 1.0f;
			float slope = (heights[idx] - heights[nIdx]) / dist;
			if (slope > bestSlope) {
				bestSlope = slope;
				lowestIdx = nIdx;
			}
		}
		if (lowestIdx >= 0) exitTarget[idx] = lowestIdx;
		// 没有比自己更低的邻居(局部坑),或者最低的邻居不是山体:流到这里为止,不往外送
		// (这就是"流到非山体就停止")。
		if (lowestIdx < 0 || !isMountain[lowestIdx]) continue;
		accum[lowestIdx] += accum[idx];
		flowTarget[idx] = lowestIdx;
	}

	float maxAccum = 0.0f;
	for (int idx : mountainIndices) {
		if (accum[idx] > maxAccum) maxAccum = accum[idx];
	}

	vector<bool> isRiver(cellCount, false);
	// 每个河流格子相对自己所在这条链路起点的"流程"(经过的格子数，不是实际距离)——用来做
	// "越往下游越宽"：每条链路自己从0开始数，山体内和流出山体之后两段接续计数，不是从
	// 全图某个固定点算。
	vector<int> riverDistance(cellCount, 0);
	// 每个河流中心线格子的流向(不要求归一化，全0=没记录)——并入别的河时拿来和当前河的流向比夹角：
	// 山体内是下坡目标方向，流出山体之后是标记那一步的流向角。
	vector<float> riverFlowX(cellCount, 0.f), riverFlowY(cellCount, 0.f);
	vector<pair<int, int>> riverOrigins; // 已确认的河流起点坐标，供间距检查用

	auto distanceToAllOriginsOk = [&](int x, int y) {
		for (auto& origin : riverOrigins) {
			float dx = float(x - origin.first), dy = float(y - origin.second);
			if (sqrtf(dx * dx + dy * dy) <= RIVER_ORIGIN_MIN_SPACING) return false;
		}
		return true;
		};

	auto hasRiverNeighbor = [&](int x, int y) {
		for (auto& off : neighborOffsets) {
			int nx = x + off[0], ny = y + off[1];
			if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
			if (isRiver[ny * width + nx]) return true;
		}
		return false;
		};

	// 地图正中心(宽高方向各3/8~5/8那一块，留给井字路网中心lot用)——河流只要流经这片区域
	// 就整条作废，不管是山体内还是流出山体之后，判定一视同仁。
	auto inCenterRegion = [&](int x, int y) {
		return x >= (int)(width * RIVER_CENTER_MIN_FRAC) && x < (int)(width * RIVER_CENTER_MAX_FRAC) &&
			y >= (int)(height * RIVER_CENTER_MIN_FRAC) && y < (int)(height * RIVER_CENTER_MAX_FRAC);
		};

	// flowTarget链路上流量只会越来越大(下坡目标的accum已经把起点的accum加进去了)，起点本身
	// 过线，整条下坡链路自然都过线，不用每一步再查一次accum，一路标记到流出山体/局部坑，
	// 或者撞上已经标记过的河流(提前停止，避免重复工作)为止。返回链路真正的终点下标+这条
	// 链路新标记的格子列表(只记"之前不是河流、这次改成true"的那些，供失败时整体撤销用)+
	// 完整路径(按经过顺序，供算出山时的初始流向用——只看最后两格算方向，方向必然是D8的
	// 8个离散方向之一，归一化之后角度还是只能是0/45/90/135这几个值，所以要往回多看几格)。
	auto propagateDownhill = [&](int startIdx) {
		vector<int> touched;
		vector<int> path;
		int cur = startIdx;
		while (true) {
			path.push_back(cur);
			if (!isRiver[cur]) {
				isRiver[cur] = true;
				riverDistance[cur] = (int)touched.size(); // 这条链路自己从0开始数
				touched.push_back(cur);
				int towards = flowTarget[cur] >= 0 ? flowTarget[cur] : exitTarget[cur];
				if (towards >= 0) {
					riverFlowX[cur] = float(towards % width - cur % width);
					riverFlowY[cur] = float(towards / width - cur / width);
				}
			}
			int next = flowTarget[cur];
			if (next < 0 || isRiver[next]) break;
			cur = next;
		}
		return make_tuple(cur, touched, path);
		};

	// 流出山体之后：连续坐标按流向每次前进RIVER_SAMPLE_STEP(0.5)距离，采样点落到哪个
	// element就把那个element标记成河流；流向用角度(标量)表示，偏向速度是角速度(标量，
	// 不是向量)，每步直接给角速度加随机扰动再积分进角度，没有"向量模长"这种概念，也
	// 不存在向量退化成0的问题。采样到的element比上一个高(说明偏向速度把路径带上坡了)，
	// 就在它的8邻居里找高度最低的那个(不是坡度最大的，这里单纯是脱困，不用再按距离
	// 归一化)，强制把角度刷新成指向那个邻居。
	// 结束原因：碰到mountain、真正的局部坑(连刷新都找不到更低的邻居)都算失败，连同
	// 这条链路山体内的那一段一起整体撤销；碰到地图边界/ocean才算成功保留；碰到别的河——
	// 和"起点+下坡蔓延"规则里的既有机制一致，算并入，不算失败也不算新起点。
	// (只装新标记的格子，同propagateDownhill的touched，失败时和山体内那段一起撤销)
	// ObtuseMerge：汇入别的河时两条河的流向夹角是钝角(>90°)，按失败处理整条撤销。
	enum class RiverEnd { MapEdge, Ocean, OtherRiver, Mountain, Pit, StepLimit, CenterBlocked, ObtuseMerge };
	auto randf = [](float lo, float hi) { return lo + GetRandom(100000) / 99999.0f * (hi - lo); };
	auto continueRiverOutsideMountain = [&](int startX, int startY, float initDirX, float initDirY,
		const vector<int>& ownChainSeed, int startDistance) {
		vector<int> touched;
		// 当前这一步的流向，markRiver给新标记的格子记下来(见riverFlowX/Y)。
		float curDirX = initDirX, curDirY = initDirY;
		// "自己这条链路已经标记过的格子"不算"别的河"——山体内propagateDownhill那段
		// (ownChainSeed)和这个函数自己往下走标记的格子都要排除,否则刚走一步,上一个格子
		// (或者山体内最后那几格)就会被当成"碰到别的河"立刻中止,河流根本流不出去。
		unordered_set<int> ownChainSet(ownChainSeed.begin(), ownChainSeed.end());
		// riverDistance接着山体内那段(startDistance=mountainTouched.size())继续往下数,
		// 不从0重开——下游变宽是看整条链路的流程，不是只看流出山体之后这一段。
		auto markRiver = [&](int ex, int ey) {
			int idx = ey * width + ex;
			if (!isRiver[idx]) {
				isRiver[idx] = true;
				riverDistance[idx] = startDistance + (int)touched.size();
				riverFlowX[idx] = curDirX;
				riverFlowY[idx] = curDirY;
				touched.push_back(idx);
			}
			ownChainSet.insert(idx);
			};
		// "碰到别的河"按8邻居判定,不是非得正好采样到河流格子本身才算——挨着河流(哪怕还没
		// 踩上去)就应该停,不然两条河会贴在一起平行流一段才真正合并，缝隙处很突兀。
		auto hasNearbyRiver = [&](int cx, int cy) {
			int selfIdx = cy * width + cx;
			if (isRiver[selfIdx] && !ownChainSet.count(selfIdx)) return true;
			for (auto& off : neighborOffsets) {
				int nx = cx + off[0], ny = cy + off[1];
				if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
				int nIdx = ny * width + nx;
				if (isRiver[nIdx] && !ownChainSet.count(nIdx)) return true;
			}
			return false;
			};
		// 汇入别的河时的夹角判定：把自己和8邻居里"别的河"格子的流向相加，再和当前流向(dirX,dirY)求点积，
		// 小于0就是夹角大于90°(钝角)，这条河就不并入、整条作废。别的河没有流向记录(全0)时点积为0，不算钝角。
		auto mergesObtusely = [&](int cx, int cy, float dirX, float dirY) {
			float sumX = 0.f, sumY = 0.f;
			auto add = [&](int nIdx) {
				if (isRiver[nIdx] && !ownChainSet.count(nIdx)) { sumX += riverFlowX[nIdx]; sumY += riverFlowY[nIdx]; }
				};
			add(cy * width + cx);
			for (auto& off : neighborOffsets) {
				int nx = cx + off[0], ny = cy + off[1];
				if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
				add(ny * width + nx);
			}
			return sumX * dirX + sumY * dirY < 0.f;
			};
		// 海洋也按8邻居判定——挨着海(哪怕采样点本身还没真的落在海洋格子上)就算到了,不用
		// 非得真的踩进海里一格才算成功,和"碰到别的河"同一个道理。
		auto hasNearbyOcean = [&](int cx, int cy) {
			if (getTerrain(cx, cy) == "terrain_ocean") return true;
			for (auto& off : neighborOffsets) {
				int nx = cx + off[0], ny = cy + off[1];
				if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
				if (getTerrain(nx, ny) == "terrain_ocean") return true;
			}
			return false;
			};
		// 规则1：判定"到海了"用的是8邻居(挨着海就算到，不用真的踩进海里一格)，但河流的
		// 格子本身到此为止——如果不把那个触发判定的海洋格子也并进河流，中间会缺一段，
		// 看起来像河流在岸边几米外凭空断掉。把检测到的那个/那些挨着的海洋格子直接标记成
		// 河流，接上这段缺口。
		auto convertTouchingOceanToRiver = [&](int cx, int cy) {
			if (getTerrain(cx, cy) == "terrain_ocean") markRiver(cx, cy);
			for (auto& off : neighborOffsets) {
				int nx = cx + off[0], ny = cy + off[1];
				if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
				if (getTerrain(nx, ny) == "terrain_ocean") markRiver(nx, ny);
			}
			};
		// 海洋最外面一圈其实是"沙滩"(贴着陆地那圈海洋格子,高度固定是0，只比固定水位
		// SEA_LEVEL高一点点，参见terrain_river.md"入海口"一节)——只接到刚碰到的这一两格
		// 海洋，河床高度公式可能还没真正算到沙滩后面更深的海里，沙滩这一截挡在中间看起来
		// 还是断的。碰到海之后继续顺着原来的流向往海里再推RIVER_MOUTH_EXTEND_CELLS格，
		// 强制多标记这几格海洋格子为河流，让河床高度公式(原高度-下降量)在沙滩之外的更深
		// 海域也执行一遍，确保真正切过沙滩连到深一点的海里。
		auto extendIntoOcean = [&](float fromX, float fromY, float dirX, float dirY) {
			float len = sqrtf(dirX * dirX + dirY * dirY);
			if (len < 1e-6f) return; // 方向退化(理论上不会发生),不推
			float ux = dirX / len, uy = dirY / len;
			float px = fromX, py = fromY;
			int lastExtEx = (int)roundf(fromX), lastExtEy = (int)roundf(fromY);
			int steps = (int)ceilf(RIVER_MOUTH_EXTEND_CELLS / RIVER_SAMPLE_STEP);
			for (int s = 0; s < steps; s++) {
				px += ux * RIVER_SAMPLE_STEP;
				py += uy * RIVER_SAMPLE_STEP;
				if (px < 0.f || px >= float(width) || py < 0.f || py >= float(height)) break;
				int ex = (int)roundf(px), ey = (int)roundf(py);
				ex = max(0, min(width - 1, ex));
				ey = max(0, min(height - 1, ey));
				if (ex == lastExtEx && ey == lastExtEy) continue;
				markRiver(ex, ey);
				lastExtEx = ex; lastExtEy = ey;
			}
			};

		string startTerrain = getTerrain(startX, startY);
		if (inCenterRegion(startX, startY)) return make_pair(RiverEnd::CenterBlocked, touched);
		if (hasNearbyOcean(startX, startY)) {
			convertTouchingOceanToRiver(startX, startY);
			extendIntoOcean(float(startX), float(startY), initDirX, initDirY);
			return make_pair(RiverEnd::Ocean, touched);
		}
		if (startTerrain == "terrain_mountain") return make_pair(RiverEnd::Mountain, touched);
		if (hasNearbyRiver(startX, startY)) {
			return make_pair(mergesObtusely(startX, startY, initDirX, initDirY) ? RiverEnd::ObtuseMerge : RiverEnd::OtherRiver, touched);
		}
		markRiver(startX, startY);

		float posX = float(startX), posY = float(startY);
		int lastEx = startX, lastEy = startY;
		float prevHeight = getHeight(startX, startY);

		float theta = atan2f(initDirY, initDirX); // 流向角度(弧度)，初始值继承自山体出口方向
		float angularVel = 0.f; // 角速度(标量，弧度/步)

		int maxSteps = (int)((width + height) * 2 / RIVER_SAMPLE_STEP); // 步距变小,步数要相应放大

		for (int step = 0; step < maxSteps; step++) {
			// 规则3：每步直接给角速度加一个带正负号的随机小量，夹到绝对值上限；角度按
			// 角速度积分更新——只有这一层积分，没有"加速度"。
			angularVel = clamp(angularVel + randf(-RIVER_ANGULAR_VEL_JITTER, RIVER_ANGULAR_VEL_JITTER), -RIVER_ANGULAR_VEL_MAX, RIVER_ANGULAR_VEL_MAX);
			theta += angularVel;

			// 规则：离海岸线/别的河太近就按比例往它的方向拐，别擦着走。横竖(东南西北)四个方向各
			// 往外探测：海洋看RIVER_COAST_AVOID_DISTANCE以内、别的河(isRiver里不属于自己这条链路的
			// 格子)看RIVER_NEIGHBOR_AVOID_DISTANCE以内，取所有命中里最近的一处；当前流向和那个方向的
			// 夹角(带符号，归一化到(-π,π])乘RIVER_COAST_STEER_COEF就是这一步要转的角度——下一步夹角
			// 已经缩小，转的量自然跟着缩小(比如夹角80°、系数1/16，这步转5°，下步转75°×1/16…)。
			// 背离目标流的时候(夹角超过90°)同样照常拐。
			{
				int cx = max(0, min(width - 1, (int)roundf(posX)));
				int cy = max(0, min(height - 1, (int)roundf(posY)));
				static const int axes[4][2] = { {1, 0}, {0, 1}, {-1, 0}, {0, -1} };
				float nearest = max(RIVER_COAST_AVOID_DISTANCE, RIVER_NEIGHBOR_AVOID_DISTANCE), target = 0.f;
				bool attractorNear = false;
				for (auto& axis : axes) {
					for (int d = 1; float(d) < nearest; d++) {
						int nx = cx + axis[0] * d, ny = cy + axis[1] * d;
						if (nx < 0 || nx >= width || ny < 0 || ny >= height) break;
						bool hit = (float(d) < RIVER_COAST_AVOID_DISTANCE && getTerrain(nx, ny) == "terrain_ocean");
						if (!hit && float(d) < RIVER_NEIGHBOR_AVOID_DISTANCE) {
							int nIdx = ny * width + nx;
							hit = isRiver[nIdx] && !ownChainSet.count(nIdx);
						}
						if (hit) {
							nearest = float(d);
							target = atan2f(float(axis[1]), float(axis[0]));
							attractorNear = true;
							break;
						}
					}
				}
				if (attractorNear) {
					float diff = target - theta;
					while (diff > 3.14159265f) diff -= 6.28318531f;
					while (diff <= -3.14159265f) diff += 6.28318531f;
					theta += diff * RIVER_COAST_STEER_COEF;
				}
			}

			float dirX = cosf(theta), dirY = sinf(theta);
			curDirX = dirX;
			curDirY = dirY;

			posX += dirX * RIVER_SAMPLE_STEP;
			posY += dirY * RIVER_SAMPLE_STEP;
			if (posX < 0.f || posX >= float(width) || posY < 0.f || posY >= float(height))
				return make_pair(RiverEnd::MapEdge, touched); // 规则：流到地图边界算成功

			int ex = (int)roundf(posX), ey = (int)roundf(posY);
			ex = max(0, min(width - 1, ex));
			ey = max(0, min(height - 1, ey));
			if (ex == lastEx && ey == lastEy) continue; // 还在同一个element里,这次采样不触发任何判定

			// 规则：流经地图中心(留给路网中心lot的那片区域)就整条河作废——这里直接返回
			// CenterBlocked(不在kept的MapEdge/Ocean/OtherRiver白名单里)，外层调用方会连同
			// 山体内那段一起整体撤销，和Mountain/Pit同一个失败语义。
			if (inCenterRegion(ex, ey)) return make_pair(RiverEnd::CenterBlocked, touched);

			// 这里查isRiver(内存里的临时状态)而不是getTerrain——真实地图这次要等全部链路都
			// 决定完才一次性提交，不然先成功的链路还没写进真实地图，后面的链路就看不到它。
			if (hasNearbyRiver(ex, ey)) { // 碰到别的河(8邻居范围内),算并入,不是失败——但夹角是钝角就不并入,整条作废
				return make_pair(mergesObtusely(ex, ey, dirX, dirY) ? RiverEnd::ObtuseMerge : RiverEnd::OtherRiver, touched);
			}
			if (hasNearbyOcean(ex, ey)) { // 挨着海(8邻居范围内),算流到海,成功
				markRiver(ex, ey); // 这一格本身先接上(走到这一步说明还没真正setTerrain过)
				convertTouchingOceanToRiver(ex, ey); // 把触发判定的海洋格子也并进河流,接上缺口
				extendIntoOcean(posX, posY, dirX, dirY); // 顺着原流向继续往海里推几格,切过沙滩
				return make_pair(RiverEnd::Ocean, touched);
			}
			string t = getTerrain(ex, ey);
			if (t == "terrain_mountain") return make_pair(RiverEnd::Mountain, touched); // 规则：碰到山体,整条河作废

			float h = getHeight(ex, ey);
			if (h > prevHeight + 1e-4f) {
				// 采样到的格子比上一格高——角速度把路径带上坡了，在这个格子的8邻居里
				// 找高度最低的那个，强制把角度刷新成指向它，角速度清零(不带着刚才那个
				// 走错方向的转速继续转)；如果连这个"最低的"也不比当前更低，说明真的是
				// 一个局部坑，没有任何方向能逃出去——规则：流到洼地中止,整条河作废。
				int bestDx = 0, bestDy = 0; float bestHeight = 1e30f; bool found = false;
				for (auto& off : neighborOffsets) {
					int nx = ex + off[0], ny = ey + off[1];
					if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
					float nh = getHeight(nx, ny);
					if (nh < bestHeight) { bestHeight = nh; bestDx = off[0]; bestDy = off[1]; found = true; }
				}
				if (!found || bestHeight >= h - 1e-4f) return make_pair(RiverEnd::Pit, touched);
				theta = atan2f(float(bestDy), float(bestDx));
				angularVel = 0.f;
			}

			markRiver(ex, ey);
			prevHeight = h;
			lastEx = ex; lastEy = ey;
		}
		return make_pair(RiverEnd::StepLimit, touched); // 步数耗尽也没到边界/海,按失败处理
		};

	int originCount = 0, keptCount = 0, shortCount = 0;
	for (int idx : mountainIndices) { // 同一个降序顺序，"先到达阈值的"就是先遍历到的
		if (isRiver[idx] || accum[idx] < RIVER_FLOW_THRESHOLD) continue;
		int x = idx % width, y = idx / width;
		// 能变成河流只有一条路：下坡蔓延(只有已有河流格子自己的最低邻居会被标记)。这里只
		// 负责判断"要不要批准成一个新起点"——挨着已有河流但不是它的下坡目标，不算，跳过；
		// 离所有已有起点都不够远，也跳过；两条都躲过了才能当新起点。
		if (hasRiverNeighbor(x, y)) continue;
		if (!distanceToAllOriginsOk(x, y)) continue;
		riverOrigins.push_back({ x, y });
		originCount++;

		auto [finalIdx, mountainTouched, mountainPath] = propagateDownhill(idx);
		// 规则：山体内这一段只要碰到地图中心区域，整条河也作废——跟流出山体之后那段用
		// 同一条inCenterRegion判定，这里没有"kept白名单"这种机制(propagateDownhill本身
		// 不区分成功/失败)，所以直接在这里扫一遍path，碰到就撤销mountainTouched、跳过
		// 这个起点，不往下走cutoff/continueRiverOutsideMountain那一套。
		bool mountainHitsCenter = false;
		for (int p : mountainPath) {
			if (inCenterRegion(p % width, p / width)) { mountainHitsCenter = true; break; }
		}
		if (mountainHitsCenter) {
			for (int t : mountainTouched) isRiver[t] = false;
			riverOrigins.pop_back();
			continue;
		}
		int exit = exitTarget[finalIdx];
		// 终点flowTarget仍然有效,说明是并入了既有河流,不是独立链路的终点,不用往下判定——
		// 但长度要求同样适用：这条支流自己在山体内走的格子数不够就整条撤销。
		if (flowTarget[finalIdx] >= 0) {
			// 并入既有河的夹角判定：当前河在终点的流向(终点->被并入的格子)和被并入那条河的流向点积小于0(钝角)就整条作废。
			int merged = flowTarget[finalIdx];
			float curX = float(merged % width - finalIdx % width), curY = float(merged / width - finalIdx / width);
			bool obtuse = curX * riverFlowX[merged] + curY * riverFlowY[merged] < 0.f;
			if (obtuse || (int)mountainPath.size() < RIVER_MIN_LENGTH) {
				for (int t : mountainTouched) isRiver[t] = false;
				riverOrigins.pop_back();
				if (!obtuse) shortCount++;
			}
			continue;
		}
		if (exit < 0) {
			// 山体内部的局部坑(exitTarget也是-1,连非山体邻居都没有)——规则：流到洼地中止,
			// 整条河作废，把这条链路刚标记的山体部分撤销。
			for (int t : mountainTouched) isRiver[t] = false;
			riverOrigins.pop_back();
			continue;
		}

		// 防止河流过陡：从山脚(mountainPath末尾)往山上(下标减小方向)逆推，只要连续两格之间
		// (更靠源头的那个减去更靠山脚的那个)高度差超过RIVER_MAX_STEP_DROP，就在那里截断——
		// 靠源头那一段(含原来的起点)整体撤销，河流起点改成截断点，只保留靠山脚、坡度更缓
		// 的那一段。
		int cutoffPos = 0; // mountainPath里真正保留下来的起始下标
		for (int p = (int)mountainPath.size() - 1; p >= 1; p--) {
			if (heights[mountainPath[p - 1]] - heights[mountainPath[p]] > RIVER_MAX_STEP_DROP) {
				cutoffPos = p;
				break;
			}
		}
		if (cutoffPos > 0) {
			for (int p = 0; p < cutoffPos; p++) isRiver[mountainPath[p]] = false;
			int newOriginIdx = mountainPath[cutoffPos];
			riverOrigins.back() = { newOriginIdx % width, newOriginIdx / width };
			// 保留下来的山体段重新从0起算riverDistance(原值是从被撤销的原起点数的)。
			for (int p = cutoffPos; p < (int)mountainPath.size(); p++) riverDistance[mountainPath[p]] = p - cutoffPos;
		}

		int ex = exit % width, ey = exit / width;
		// 初始流向不能只看出山前最后一格到出山第一格这一步——那永远是D8的8个离散方向之一，
		// 归一化后角度只能是0/45/90/135这几个值。往回倒最多4格(链路不够长就用截断之后剩下
		// 那段的起点)，从那一格直接指向出山格子，算出来的向量分量通常不再是±1，角度自然
		// 不再是离散值。
		int histLen = min(4, (int)mountainPath.size() - cutoffPos);
		int baseIdx = mountainPath[mountainPath.size() - histLen];
		int bx = baseIdx % width, by = baseIdx / width;
		float initDirX = float(ex - bx), initDirY = float(ey - by);
		// 流程计数接着"保留下来的那段山体路径"长度继续数(不是整条mountainTouched的原始
		// 长度)——过陡截断之后前面那段已经撤销，不算在"从源头流了多远"里面。
		int mountainKeptLen = (int)mountainPath.size() - cutoffPos;
		vector<int> keptMountainPath(mountainPath.begin() + cutoffPos, mountainPath.end()); // 不含已撤销的格子
		auto [reason, plainTouched] = continueRiverOutsideMountain(ex, ey, initDirX, initDirY, keptMountainPath, mountainKeptLen);
		bool kept = (reason == RiverEnd::MapEdge || reason == RiverEnd::Ocean || reason == RiverEnd::OtherRiver);
		// 长度=过陡截断之后保留的山体段+流出山体之后新标记的格子(含入海口接驳/延伸的格子)，
		// 不够RIVER_MIN_LENGTH就和其他失败原因一样整条撤销。
		if (kept && mountainKeptLen + (int)plainTouched.size() < RIVER_MIN_LENGTH) {
			kept = false;
			shortCount++;
		}
		if (!kept) {
			for (int t : mountainTouched) isRiver[t] = false;
			for (int t : plainTouched) isRiver[t] = false;
			riverOrigins.pop_back();
		} else {
			keptCount++;
		}
	}

	// 加宽河流：所有起点/碰撞/截断判定都是基于宽度1的中心线做的(hasNearbyRiver这类检测
	// 要是提前用加宽之后的格子去判断，中心线还没走开多远就会被自己的宽度碰到，逻辑会乱)，
	// 所以加宽放在最后。越往下游越宽——每个中心线格子按自己的riverDistance(这条链路从
	// 源头数起、流经了多少格)算一个专属半径，不再是所有格子统一膨胀一圈的固定宽度3。
	// 半径用欧几里得距离判断(跟河岸blur那次改法一样)，不用8邻居格数，避免同样的"方块/
	// 八边形"棱角问题；先试每200格宽度+1(对称膨胀一次两侧各加，所以是半径+0.5)。
	{
		vector<bool> widened = isRiver;
		for (int idx = 0; idx < cellCount; idx++) {
			if (!isRiver[idx]) continue;
			int x = idx % width, y = idx / width;
			float radius = RIVER_BASE_WIDEN_RADIUS + riverDistance[idx] / RIVER_WIDTH_GROW_PER_CELLS * 0.5f;
			int radiusCeil = (int)ceilf(radius);
			for (int dy = -radiusCeil; dy <= radiusCeil; dy++) {
				for (int dx = -radiusCeil; dx <= radiusCeil; dx++) {
					if (dx == 0 && dy == 0) continue;
					if (sqrtf(float(dx * dx + dy * dy)) > radius) continue;
					int nx = x + dx, ny = y + dy;
					if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
					widened[ny * width + nx] = true;
				}
			}
		}
		isRiver = move(widened);
	}

	// 河岸平滑范围也并进河流，不止是改个高度：算每个格子到(加宽+入海口兜底加宽之后)
	// 河流的距离，只算到RIVER_BLUR_RADIUS为止，这个范围内的格子直接并入isRiver——连地形类型、水面
	// 都按河流处理，不是只单独调一下高度。只调高度不改地形/不加水面的话，水面mesh的
	// 覆盖范围还是停在加宽后的河道边缘，边缘往外一格地形已经被削平滑了一截，水面却没有
	// 跟着铺过去，侧边界只是往外挪了一格，没有真正消失。falloff随距离线性减小(核心河道
	// 为1，到RIVER_BLUR_RADIUS+1格之外为0)，只用来决定高度降多少；水面统一用"原地形
	// 高度+SEA_LEVEL"这个公式，不受falloff影响，跟河道本身的水位计算方式一致。
	// 原来用BFS按"格数"算距离，8邻居每一步距离都算1，等距离线其实是个八边形/方块
	// (切比雪夫距离的那种棱角形状)，这才是"圆角锯齿"的真正来源——半径不够只会让这圈
	// 更窄，棱角本身不会消失。改成直接算每个格子到最近河流格子的欧几里得距离(真实的
	// 直线距离，不是走了几步)，等距离线才会是真正的圆弧。只在每个河流格子周围
	// (2*RIVER_BLUR_RADIUS+1)见方的局部范围内比较，不是整张地图两两比较距离，河流格子
	// 数量通常远小于全图格子数，复杂度仍然可控。
	// 线性衰减基础上再过一次smoothstep缓动(3t^2-2t^3)，让下降速度本身也在半径边界顺滑地
	// 起止，不是一进圈就突然开始按固定速率下降。到d=RIVER_BLUR_RADIUS+1才降到0。
	auto bankFalloff = [](float d) {
		float t = clamp(1.0f - d / float(RIVER_BLUR_RADIUS + 1), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
		};
	vector<float> dropFalloff(cellCount, 1.0f); // 核心河道(加宽后)默认按满额下降
	// 到核心河道(加宽后、并入平滑范围之前)的欧几里得距离，搜到RIVER_BANK_SEARCH_RADIUS为止，
	// 更远的统一记成这个值——commit之后生成细化高度数组时还要对它做插值，所以留在块外面。
	vector<float> distToRiver(cellCount, float(RIVER_BANK_SEARCH_RADIUS));
	// 只降高度、不并入河流的那一圈(RIVER_BLUR_RADIUS<d<RIVER_BLUR_RADIUS+1)。
	vector<bool> isBankOnly(cellCount, false);
	{
		vector<int> riverCells;
		riverCells.reserve(cellCount / 8);
		for (int idx = 0; idx < cellCount; idx++) if (isRiver[idx]) riverCells.push_back(idx);
		for (int idx : riverCells) {
			int rx = idx % width, ry = idx / width;
			for (int dy = -RIVER_BANK_SEARCH_RADIUS; dy <= RIVER_BANK_SEARCH_RADIUS; dy++) {
				for (int dx = -RIVER_BANK_SEARCH_RADIUS; dx <= RIVER_BANK_SEARCH_RADIUS; dx++) {
					int nx = rx + dx, ny = ry + dy;
					if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
					float d = sqrtf(float(dx * dx + dy * dy));
					int nIdx = ny * width + nx;
					if (d < distToRiver[nIdx]) distToRiver[nIdx] = d;
				}
			}
		}
		for (int idx = 0; idx < cellCount; idx++) {
			if (isRiver[idx]) continue;
			float d = distToRiver[idx];
			if (d <= float(RIVER_BLUR_RADIUS)) {
				isRiver[idx] = true; // 平滑范围内的格子并入河流
				dropFalloff[idx] = bankFalloff(d);
			}
			else if (d < float(RIVER_BLUR_RADIUS + 1)) {
				// 以前这一圈直接不降(d=2还降0.26×RIVER_HEIGHT_DROP，下一格d=√5就是0)，粗高度
				// 本身就有一个台阶，细化高度没法光滑地接上。这一圈只按同一条smoothstep把高度
				// 降一点(降完仍高于水位，是干地)，地形类型/水面都不动，不影响路网/zone。
				isBankOnly[idx] = true;
				dropFalloff[idx] = bankFalloff(d);
			}
		}
	}

	int riverCount = 0;
	for (int idx = 0; idx < cellCount; idx++) {
		if (isRiver[idx]) {
			int x = idx % width, y = idx / width;
			// 必须在setTerrain覆盖之前读一下这个格子原来是不是"terrain_ocean"——根因就在这里：
			// 加宽/blur是纯几何扩散，不看原来是什么地形，会把边缘一圈真正的海洋格子也
			// 并进isRiver。这些格子的"原高度"是OceanTerrain自己算的海底深度(不是0)，如果
			// 水位也套河流那套"原高度+SEA_LEVEL"，算出来的水位会比海洋自己固定的-0.1更低
			// (深度越深、水位越低)，在入海口附近凹下去一块；旁边没被卷进来的真正海洋格子
			// 水位还是固定的-0.1，相对之下，海洋自己那圈贴着海岸、本来就只比水面高一点点
			// 的海岸线格子(高度0)，相对这片凹陷的水面就显得"从水里凸出来了一截"——水面凹陷
			// +海岸线凸起这两个一叠加，看起来就是河流和海洋从中间断开了。
			bool wasOcean = (getTerrain(x, y) == "terrain_ocean");
			setTerrain(x, y, GetType());
			// 第三阶段：河床比原来的地形低一截，水面盖在河床上方一点，水位跟着(已经降过的)
			// 河床高度走——相邻河流格子高度本来就不一样(沿下坡方向递减)，水位自然跟着不同，
			// 倾斜就是这样自动出来的，不用另外算坡度方向。falloff<1的那一圈(河岸平滑范围)
			// 降得比核心河道少，越往外越接近原高度，做出缓坡而不是直角。
			float originalHeight = getHeight(x, y);
			float riverBedHeight = originalHeight - RIVER_HEIGHT_DROP * dropFalloff[idx];
			setHeight(x, y, riverBedHeight);
			// 水位不是"河床高度+偏移"，是跟海洋同一个公式："原地形高度+SEA_LEVEL"——
			// 河口附近原地形(plain)高度本来就接近0，算出来的水位天然就是SEA_LEVEL，正好
			// 和海面对上，不需要再夹一下；原地形越往上游/越高，水位跟着一起抬，倾斜还是
			// 同样会自然出来(相邻格子原地形高度本来就不一样，常数偏移不影响相对高低)。
			// 水位不随falloff打折——河岸平滑范围的格子水位和核心河道用同一个公式，水面才能
			// 连续地盖过去，不会在平滑范围边缘又断一截。
			// 唯一的例外：这个格子原来就是ocean的话，水位必须沿用海洋自己那个固定值
			// (SEA_LEVEL)，不能用"原高度+SEA_LEVEL"重新算——原高度是海底深度，不是河流
			// 沿岸的地形高度，跟河流那套公式的假设(原地形高度本来接近0)完全不是一回事，
			// 套上去只会凹陷。这不是"入海口加宽"那种基于坐标范围的特殊区域，是基于这个
			// 格子真实的原始地形类型判断的，覆盖所有被加宽/blur卷入的真海洋格子。
			float waterHeight = wasOcean ? SEA_LEVEL : originalHeight + SEA_LEVEL;
			setWater(x, y, true, waterHeight);
			// 河床比周围矮一截，只给河流格子自己铺水面的话，凹陷边缘会露出水面mesh的侧面
			// (看起来像一块水"悬浮"在坑里，能看到切面)——把8个邻居也铺上同一片水面接住这个
			// 落差，挡住侧边界。跳过已经是河流/海洋的邻居，不覆盖它们各自更准确的水位
			// (海洋优先级更高，这次river跑的时候海洋早就定好了；河流邻居轮到它自己那个
			// idx时会用自己算出来的水位覆盖回来，顺序不影响最终结果)。
			for (auto& off : neighborOffsets) {
				int nx = x + off[0], ny = y + off[1];
				if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
				string nt = getTerrain(nx, ny);
				if (nt == "terrain_river" || nt == "terrain_ocean") continue;
				setWater(nx, ny, true, waterHeight);
			}
			riverCount++;
		}
	}

	for (int idx = 0; idx < cellCount; idx++) {
		if (!isBankOnly[idx]) continue;
		setHeight(idx % width, idx / width, heights[idx] - RIVER_HEIGHT_DROP * dropFalloff[idx]);
	}

	// 河岸细化高度：粗高度10m一格、双线性插值在格子边界只有C0，水岸线(河床和水面的交线)
	// 会是一段段折线。给河岸附近的格子生成17x17细化数组：
	//   fine(p) = 原高度双线性(p) - RIVER_HEIGHT_DROP * bankFalloff(d(p))
	// d(p)是对distToRiver做Catmull-Rom双三次插值——距离场近似线性，Catmull-Rom精确还原
	// 线性/常数，等值线(水岸线)是光滑曲线，核心河道内d恒为0、河床保持平；整数格点上插值
	// 权重退化成(0,1,0,0)，fine正好等于上面commit写进去的粗高度。原高度用双线性而不是
	// 双三次，是为了和没有细化数组的邻格在共享边上算法一致(那里下降量为0)。相邻两块细化
	// 数组的共享边只依赖同一列/行的数据，两边结果完全一致。必须在所有setHeight之后调用
	// (setHeight会清掉共用格点的细化数组)，见terrain_mod.md。
	int fineCount = 0;
	float maxFineCornerError = 0.0f; // 诊断：细化数组四角和粗高度的最大偏差，应该≈0
	{
		// Catmull-Rom权重只和细化网格下标有关，所有格子共用一份。
		float crWeights[TERRAIN_FINE_SIZE][4];
		for (int i = 0; i < TERRAIN_FINE_SIZE; i++) {
			float t = float(i) / TERRAIN_FINE_SUBDIV, t2 = t * t, t3 = t2 * t;
			crWeights[i][0] = 0.5f * (-t3 + 2.0f * t2 - t);
			crWeights[i][1] = 0.5f * (3.0f * t3 - 5.0f * t2 + 2.0f);
			crWeights[i][2] = 0.5f * (-3.0f * t3 + 4.0f * t2 + t);
			crWeights[i][3] = 0.5f * (t3 - t2);
		}
		auto clampX = [width](int x) { return max(0, min(width - 1, x)); };
		auto clampY = [height](int y) { return max(0, min(height - 1, y)); };

		vector<float> fine(TERRAIN_FINE_SIZE * TERRAIN_FINE_SIZE);
		float stencil[4][4];
		float rowInterp[4][TERRAIN_FINE_SIZE];
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				// 4x4模板(x-1..x+2, y-1..y+2，越界夹到边上)里有格子离河道够近、又不全是核心
				// 河道(全是核心时细化结果和粗双线性一样平)，才需要细化数组。
				float minDist = 1e30f, maxDist = 0.0f;
				for (int b = 0; b < 4; b++) {
					for (int a = 0; a < 4; a++) {
						float d = distToRiver[clampY(y - 1 + b) * width + clampX(x - 1 + a)];
						stencil[b][a] = d;
						minDist = min(minDist, d);
						maxDist = max(maxDist, d);
					}
				}
				if (minDist >= float(RIVER_BLUR_RADIUS) + 1.5f || maxDist <= 0.0f) continue;

				for (int b = 0; b < 4; b++)
					for (int i = 0; i < TERRAIN_FINE_SIZE; i++)
						rowInterp[b][i] = crWeights[i][0] * stencil[b][0] + crWeights[i][1] * stencil[b][1] +
							crWeights[i][2] * stencil[b][2] + crWeights[i][3] * stencil[b][3];

				int x1 = clampX(x + 1), y1 = clampY(y + 1);
				float h00 = heights[y * width + x], h10 = heights[y * width + x1];
				float h01 = heights[y1 * width + x], h11 = heights[y1 * width + x1];
				for (int j = 0; j < TERRAIN_FINE_SIZE; j++) {
					float ty = float(j) / TERRAIN_FINE_SUBDIV;
					for (int i = 0; i < TERRAIN_FINE_SIZE; i++) {
						float tx = float(i) / TERRAIN_FINE_SUBDIV;
						float d = crWeights[j][0] * rowInterp[0][i] + crWeights[j][1] * rowInterp[1][i] +
							crWeights[j][2] * rowInterp[2][i] + crWeights[j][3] * rowInterp[3][i];
						float base = h00 * (1 - tx) * (1 - ty) + h10 * tx * (1 - ty) + h01 * (1 - tx) * ty + h11 * tx * ty;
						fine[j * TERRAIN_FINE_SIZE + i] = base - RIVER_HEIGHT_DROP * bankFalloff(max(0.0f, d));
					}
				}
				const int last = TERRAIN_FINE_SIZE - 1;
				maxFineCornerError = max({ maxFineCornerError,
					fabsf(fine[0] - getHeight(x, y)), fabsf(fine[last] - getHeight(x1, y)),
					fabsf(fine[last * TERRAIN_FINE_SIZE] - getHeight(x, y1)),
					fabsf(fine[last * TERRAIN_FINE_SIZE + last] - getHeight(x1, y1)) });
				setFineHeights(x, y, fine.data());
				fineCount++;
			}
		}
	}

	debugf("Log: River mountain cells=%d, max accum=%.2f, threshold=%.2f, origins=%d, kept=%d, too short=%d, marked river=%d, fine cells=%d, max fine corner error=%.6f.\n",
		(int)mountainIndices.size(), maxAccum, RIVER_FLOW_THRESHOLD, originCount, keptCount, shortCount, riverCount, fineCount, maxFineCornerError);
}
