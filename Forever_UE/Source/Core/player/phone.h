#pragma once

#include "player/app.h"
#include "player/canvas.h"
#include "common/handle.h"

#include <string>
#include <vector>


// Phone：手机UI/多任务外壳，PHASE4_PLAN.md点名的"无Mod扩展点骨干类"(不是<Concept>Factory
// 家族成员，本身不可被Mod替换，是这个项目自己的固定逻辑)。参考老工程Core/player/phone.h的
// 三态设计(Home桌面网格/InApp当前App全屏/TaskList最近任务缩略图切换器)，但有两处关键调整：
// 1) AppEntry直接持有一个真正的App*实例(不是老工程"App状态全存static字段、这里只记类型
//    字符串"的写法)，因为这个项目的AppMod已经是Factory创建真实实例的模式(见app_mod.h)。
// 2) 所有布局(网格列宽/图标大小/底部工具栏高度等)按画布宽高的比例算，不是老工程那种硬编码
//    480x640下的像素常量——这个Phone的画布分辨率会随宿主(UPhoneWidget)的显示尺寸变化，
//    见RecomputeLayout/Resize。
//
// 生命周期：跟Puzzle不同，Phone需要跨越"开手机->关手机->再开手机"保持状态(多任务/最近
// 任务要留着)，所以只在第一次打开手机时创建一次，此后由宿主Widget只切换可见性，不
// delete/重建，见Source/Forever/UI/PhoneWidget.h。
class Phone {
public:
	Phone();
	~Phone(); // delete entries里所有还活着的App*

	// 首次创建时调用一次：记录画布尺寸、算好初始布局、枚举已启用的App类型(桌面图标)。
	void Init(int width, int height);

	// 画布尺寸变化(宿主视口/DPI变化)时调用：只重算布局，不重置state/entries/当前打开的App，
	// 跟Init()的区别是不会清空已有的多任务状态。尺寸没变化时直接跳过。
	void Resize(int width, int height);

	// 每帧调用一次
	// @canvas: 画布，不持有所有权
	// @ms: 距上一帧的毫秒数
	// @post: 向Core发起查询的句柄(主屏日期/时间靠这个查"game time"，见Core/common/implement.cpp)
	// @return: 0表示继续，非0表示请求关闭整个手机UI(目前只有Home界面按ESC才会触发)
	int Loop(Canvas* canvas, int ms, PostHandle* post);

private:
	enum class State { Home, InApp, TaskList };
	enum class BottomButton { None, Back, Home, Tasks };

	struct AppEntry {
		std::string type;
		bool initialized = false;
		App* app = nullptr; // 真正持有的实例，见类注释第1点

		// 关闭这个App前(离开InApp状态时)存的原始BGRA像素快照，任务列表缩略图用。宽高
		// 单独记录(不是复用Phone当前的width/height)——因为快照拍下的那一刻画布尺寸可能
		// 跟"现在"的画布尺寸不一样(比如拍完快照之后用户拖动了窗口)，PutRawImage需要按
		// 快照自己实际的尺寸解读这份像素数据，否则会读越界。
		std::vector<uint8_t> snapshot;
		int snapshotWidth = 0;
		int snapshotHeight = 0;
	};

	int width = 0, height = 0;

	// 桌面图标网格布局，RecomputeLayout()按画布宽高的比例重算。
	int gridCols = 3;
	int cellWidth = 0, cellHeight = 0, gridStartY = 0, iconSize = 0;

	// 任务列表缩略图网格布局。
	int taskCols = 2;
	int taskCellWidth = 0, taskCellHeight = 0, thumbWidth = 0, thumbHeight = 0;

	// 底部工具栏(返回/主页/任务)，三态都常驻渲染。
	int barHeight = 0, barY = 0;

	std::vector<std::string> appTypes; // 已启用的App类型(桌面图标顺序)，来自AppFactory::GetRegisteredIds()，跳过"empty"占位id
	std::vector<AppEntry> entries;     // 打开过的App("最近任务")
	State state = State::Home;
	int homeSelection = 0;
	int taskSelection = 0;
	std::string currentAppType;        // state==InApp时，当前在跑的App类型(entries里的一项)

	// HandleHomeInput在Home态识别到ESC时置true，Loop()读到之后当作"关闭整个手机UI"的
	// 请求(返回非0)，每帧开头清零——只有Home态会设置它，InApp/TaskList态的ESC语义是
	// "退一级"(见HandleInAppInput/HandleTaskListInput)，不会碰这个标记。
	bool pendingClose = false;

	void BuildAppList();
	int FindEntry(const std::string& type) const;

	// 从桌面/任务列表点开一个App：entries里没有就先创建一个新的App*，再转发给OpenApp。
	void LaunchApp(Canvas* canvas, const std::string& type, PostHandle* post);

	// 打开entries[entryIndex]：第一次(initialized==false)调App::Init，否则调App::Refresh
	// (对应老工程"从后台恢复不重新Init"的设计)。
	void OpenApp(Canvas* canvas, int entryIndex, PostHandle* post);

	void SaveSnapshot(Canvas* canvas, int entryIndex);
	void CloseEntry(int entryIndex); // 从entries里erase，delete App*

	// 离开当前InApp状态前先存缩略图，再切到Home/TaskList状态。
	void GoHome(Canvas* canvas, PostHandle* post);
	void GoTaskList(Canvas* canvas, PostHandle* post);

	void HandleHomeInput(Canvas* canvas, PostHandle* post);

	// InApp状态下，只拦截Phone自己关心的几个键(ESC/Tab/Space/Backspace)，其余按键/鼠标
	// 事件原样PushKey/PushMouseButton还给队列，留给当前App的Loop()自己消费——跟老工程
	// "未命中的输入事件重新入队交给下一层处理"是同一个idiom。
	void HandleInAppInput(Canvas* canvas, PostHandle* post);

	void HandleTaskListInput(Canvas* canvas, PostHandle* post);

	void RenderHome(Canvas* canvas, PostHandle* post);
	void RenderTaskList(Canvas* canvas);
	void RenderBottomBar(Canvas* canvas);

	void RecomputeLayout(int width, int height);

	int HitHomeGrid(int x, int y) const;
	int HitTaskGrid(int x, int y) const;
	bool HitTaskCloseButton(int entryIndex, int x, int y) const;
	BottomButton HitBottomBar(int x, int y) const;
};
