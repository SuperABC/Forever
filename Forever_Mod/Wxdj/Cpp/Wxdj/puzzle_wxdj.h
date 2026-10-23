#pragma once

#include "player/puzzle_mod.h"

#include "battle/def.h"

#include <memory>
#include <string>
#include <vector>


namespace wxdj {
class Action;
class Cultivator;
class Controller;
class PolicyAI;
class Log;
}

// 五行斗诀回合制战斗小游戏：玩家(下标0)对售货员(下标1)，战斗规则全在battle/里，这里只负责
// 画面(顶部状态条/左半战报/右半操作栏+招式家族list)和鼠标交互。见puzzle_wxdj.md。
class WxdjPuzzle : public PuzzleMod {
public:
	WxdjPuzzle();
	virtual ~WxdjPuzzle();

	static const char* GetId() { return "puzzle_wxdj"; }
	virtual const char* GetType() const override { return "puzzle_wxdj"; }
	virtual const char* GetName() override;

	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual int Loop(Canvas* canvas, int ms, PostHandle* post) override;

	// 存档保存/恢复命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }

private:
	// 等待玩家出招，或战斗结束停留展示结果
	enum Phase { PHASE_SELECT, PHASE_RESULT };

	// 招式list点击的含义：单招=点一个家族按钮立即出招，双招=依次点两个按钮后出招
	enum Mode { MODE_SINGLE, MODE_DUAL };

	// 可交互控件的id。家族按钮id = ID_ROW_BASE + 家族下标；统一的"第几式"滑块是ID_SLIDER。
	enum WidgetId {
		ID_NONE = -1,
		ID_MODE_SINGLE = 1, ID_MODE_DUAL, ID_ACCUMULATE, ID_BOMB, ID_SKIP,
		ID_CLOSE,
		ID_SCROLL_TRACK, ID_SCROLL_THUMB,
		ID_SLIDER,
		ID_ROW_BASE = 1000,
	};

	// 一个招式家族(名字去掉"第X式"后相同的招式)，招式在xiaohua.txt里按式从低到高连续排列
	struct Family {
		std::string name;
		int firstAction = 0; // 第一式在玩家招式表里的下标
		int formCount = 0;   // 一共多少式
	};

	// 一个可点击的矩形。每帧由RebuildWidgets生成，命中测试和绘制共用同一份，避免两套坐标。
	struct Widget {
		int id = ID_NONE;
		int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
		bool enabled = true;
	};

	struct Rect {
		int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	};

	void ComputeLayout();
	void ClampScroll();

	// 控件
	void RebuildWidgets();
	int HitTest(int x, int y) const;
	const Widget* FindWidget(int id) const;
	void Activate(int id);
	void BeginScrollDrag(int mouseY);
	void UpdateScrollDrag(int mouseY);
	bool RowEnabled(int familyIdx) const;
	int RowCount() const;
	// 家族当前滑块选中的那一式在招式表里的下标
	int ActionIndexOf(int familyIdx) const;
	void BuildFamilies();
	// 滑块轨道的x范围(左边留给"第N式"文字)
	void SliderTrack(const Widget& widget, int& x1, int& x2) const;
	void SetSliderFromMouse(int mouseX);
	int ThumbTop() const;
	int ThumbHeight() const;

	// 出招与战报
	void SubmitPlayerAction(std::shared_ptr<wxdj::Action> playerAction);
	void ResolveTurn(std::shared_ptr<wxdj::Action> playerAction);
	void AppendNews();
	void PushLogLine(const std::string& text);
	void AppendRoundHeader();
	std::string FormatLog(const wxdj::Log& log) const;

	// 绘制
	void DrawScene(Canvas* canvas);
	void DrawStatusBar(Canvas* canvas, const std::shared_ptr<wxdj::Cultivator>& cultivator,
		int x, int y, int w);
	void DrawLogPanel(Canvas* canvas);
	void DrawActionPanel(Canvas* canvas);
	void DrawResultOverlay(Canvas* canvas);
	void DrawScrollBar(Canvas* canvas);
	void DrawTooltip(Canvas* canvas, int mouseX, int mouseY);
	// 招式的效果文字，每个效果一行；没有效果返回空
	std::vector<std::string> EffectLines(int actionIdx) const;
	// 按控件当前状态调整底色：禁用=压灰，悬停=变亮，按下=变暗，其余保持原色。
	void ShadeColor(const Widget& widget, int& r, int& g, int& b) const;
	// 按控件当前状态(禁用/悬停/按下)给底色调色后画一个按钮，label居中，selected为true时加亮边框。
	void DrawButton(Canvas* canvas, const Widget& widget, const std::string& label,
		int r, int g, int b, bool selected);

	// 在maxWidth内按UTF-8字符折行，返回折好的每一行。Canvas没有自带折行接口。
	static std::vector<std::string> WrapText(Canvas* canvas, const std::string& text, int maxWidth);

	// 招式属性对应的底色
	static void AttributeColor(wxdj::ATTRIBUTE_TYPE attribute, int& r, int& g, int& b);

	// 画布尺寸变化回调(Canvas::SetResizeFunc要求裸函数指针，userData带this)。
	static void OnCanvasResize(void* userData, int width, int height);

	int canvasW = 0, canvasH = 0;
	Phase phase = PHASE_SELECT;
	bool loadFailed = false;
	bool closeRequested = false;
	int pendingResult = 0; // 战斗结束的判定，1=胜利，-1=失败
	int roundNumber = 0;

	std::shared_ptr<wxdj::Cultivator> player;
	std::shared_ptr<wxdj::Cultivator> enemy;
	std::unique_ptr<wxdj::Controller> controller;
	std::unique_ptr<wxdj::PolicyAI> enemyAI; // 敌方策略；加载失败时为空，退回随机出招
	std::vector<int> enemyFamilyActions;     // 敌方每个家族出招用的那一式(最高式)的招式下标

	std::vector<std::string> logLines; // 滚动战报，超出上限丢弃最旧的

	// 交互状态
	Mode mode = MODE_SINGLE;
	int firstPick = -1;        // 双招模式下已选的第一招下标，-1表示还没选
	int firstPickFamily = -1;  // 第一招所在家族
	int scrollOffset = 0;      // list首个可见行的下标
	int hoverId = ID_NONE;     // 鼠标当前所在控件
	int pressedId = ID_NONE;   // 左键按下时所在的控件，抬起后清除
	bool dragging = false;     // 正在拖动滚动条滑块
	bool sliderDragging = false; // 正在拖动"第几式"滑块
	std::vector<Family> families;
	int selectedForm = 0;      // 统一滑块选中的式(从0起)，所有家族共用；双招必须两招同式
	int formMax = 1;           // 滑块可选的式数(各家族式数的最小值)
	int dragGrab = 0;          // 按下滑块时鼠标距滑块顶部的偏移
	std::vector<Widget> widgets;

	// 布局(按画布尺寸每次ComputeLayout重算)
	int margin = 0, statusBarH = 0, gap = 0;
	int btnH = 0, rowH = 0, rowGap = 0, scrollBarW = 0, hintH = 0;
	int visibleRows = 0;
	Rect logPanel, actionPanel;
	Rect modeRow, opRow, sliderRow, hintRect, listRect, trackRect;

	// 双方初始属性上限(用于血条/真气条/武力条比例)，双方都用4-6阶位的标准属性
	int playerMaxHP = 0, playerMaxATK = 0, playerMaxMP = 0;
	int enemyMaxHP = 0, enemyMaxATK = 0, enemyMaxMP = 0;

	static int count;
	int id;
	std::string name;
};
