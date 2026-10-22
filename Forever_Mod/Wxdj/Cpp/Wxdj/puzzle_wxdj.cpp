#include "puzzle_wxdj.h"

#include "common/json.h"
#include "common/utility.h"

#include "battle/action.h"
#include "battle/controller.h"
#include "battle/cultivator.h"

#include <algorithm>
#include <cstdint>
#include <sstream>

#pragma comment(lib, "Dependence.lib")

#define MAX_LOG_LINES 200
#define ACTION_FILE_NAME "xiaohua.txt"


using namespace std;
using namespace wxdj;

int WxdjPuzzle::count = 0;

WxdjPuzzle::WxdjPuzzle() : id(count++) {

}

WxdjPuzzle::~WxdjPuzzle() {

}

const char* WxdjPuzzle::GetName() {
	name = "Wxdj" + to_string(id);
	return name.data();
}

void WxdjPuzzle::OnCanvasResize(void* userData, int width, int height) {
	WxdjPuzzle* self = static_cast<WxdjPuzzle*>(userData);
	self->canvasW = width;
	self->canvasH = height;
	self->ComputeLayout();
	self->ClampScroll();
}

// 双招的真气消耗，和DualAction构造函数里的公式一致(取整方式也一致)。
static int DualCost(int point1, int point2) {
	return static_cast<int>(0.75 * (point1 + point2));
}

void WxdjPuzzle::ComputeLayout() {
	margin = max(8, canvasW / 50);
	statusBarH = max(90, canvasH / 5);
	gap = max(4, margin / 2);

	// 顶部状态条占据[margin, margin+statusBarH]，下面的body左右各半：左战报、右操作栏+招式list
	int bodyTop = margin + statusBarH + margin;
	int bodyBottom = max(bodyTop + 100, canvasH - margin);
	int mid = canvasW / 2;

	logPanel = { margin, bodyTop, mid - margin / 2, bodyBottom };
	actionPanel = { mid + margin / 2, bodyTop, canvasW - margin, bodyBottom };

	btnH = max(28, canvasH / 14);
	rowH = max(24, canvasH / 20);
	rowGap = 2;
	scrollBarW = max(10, canvasW / 120);
	hintH = max(18, canvasH / 36);

	modeRow = { actionPanel.x1, bodyTop, actionPanel.x2, bodyTop + btnH };
	opRow = { actionPanel.x1, modeRow.y2 + gap, actionPanel.x2, modeRow.y2 + gap + btnH };
	hintRect = { actionPanel.x1, opRow.y2 + gap, actionPanel.x2, opRow.y2 + gap + hintH };
	listRect = { actionPanel.x1, hintRect.y2 + gap, actionPanel.x2, max(hintRect.y2 + gap + rowH, bodyBottom) };
	trackRect = { actionPanel.x2 - scrollBarW, listRect.y1, actionPanel.x2, listRect.y2 };

	visibleRows = max(1, (listRect.y2 - listRect.y1 + rowGap) / (rowH + rowGap));
}

void WxdjPuzzle::ClampScroll() {
	int rowCount = player ? static_cast<int>(player->GetActions().size()) : 0;
	int maxOffset = max(0, rowCount - visibleRows);
	scrollOffset = max(0, min(scrollOffset, maxOffset));
}

int WxdjPuzzle::ThumbHeight() const {
	int rowCount = player ? static_cast<int>(player->GetActions().size()) : 0;
	int trackH = trackRect.y2 - trackRect.y1;
	if (rowCount <= visibleRows) return trackH;
	return min(trackH, max(20, trackH * visibleRows / rowCount));
}

int WxdjPuzzle::ThumbTop() const {
	int rowCount = player ? static_cast<int>(player->GetActions().size()) : 0;
	int maxOffset = max(0, rowCount - visibleRows);
	int travel = (trackRect.y2 - trackRect.y1) - ThumbHeight();
	if (maxOffset <= 0 || travel <= 0) return trackRect.y1;
	return trackRect.y1 + static_cast<int>(static_cast<int64_t>(travel) * scrollOffset / maxOffset);
}

void WxdjPuzzle::BeginScrollDrag(int mouseY) {
	dragging = true;
	dragGrab = mouseY - ThumbTop();
}

void WxdjPuzzle::UpdateScrollDrag(int mouseY) {
	int rowCount = player ? static_cast<int>(player->GetActions().size()) : 0;
	int maxOffset = max(0, rowCount - visibleRows);
	int travel = (trackRect.y2 - trackRect.y1) - ThumbHeight();
	if (maxOffset <= 0 || travel <= 0) return;

	// 拖动期间只看鼠标的y，不要求鼠标还在滑块上
	int64_t offsetY = static_cast<int64_t>(mouseY) - dragGrab - trackRect.y1;
	int64_t offset = (offsetY * maxOffset + travel / 2) / travel;
	scrollOffset = static_cast<int>(max<int64_t>(0, min<int64_t>(offset, maxOffset)));
}

bool WxdjPuzzle::RowEnabled(int actionIdx) const {
	if (!player) return false;
	const auto& actions = player->GetActions();
	if (actionIdx < 0 || actionIdx >= static_cast<int>(actions.size())) return false;

	int point = actions[actionIdx].GetPoint();
	int mp = player->GetCurrentMP();
	if (mode == MODE_SINGLE) return point <= mp;

	if (firstPick >= 0) return DualCost(actions[firstPick].GetPoint(), point) <= mp;

	// 还没选第一招：只要它配上最便宜的一招付得起就算可选
	int cheapest = point;
	for (const auto& action : actions) cheapest = min(cheapest, action.GetPoint());
	return DualCost(point, cheapest) <= mp;
}

void WxdjPuzzle::RebuildWidgets() {
	widgets.clear();

	auto add = [&](int widgetId, int x1, int y1, int x2, int y2, bool enabled) {
		Widget widget;
		widget.id = widgetId;
		widget.x1 = x1; widget.y1 = y1; widget.x2 = x2; widget.y2 = y2;
		widget.enabled = enabled;
		widgets.push_back(widget);
	};

	if (phase == PHASE_RESULT) {
		int w = max(120, canvasW / 8), h = max(36, canvasH / 14);
		int x = (canvasW - w) / 2, y = canvasH / 2 + canvasH / 20;
		add(ID_CLOSE, x, y, x + w, y + h, true);
		return;
	}

	ClampScroll();

	// 模式行：单招/双招
	int halfW = (modeRow.x2 - modeRow.x1 - gap) / 2;
	add(ID_MODE_SINGLE, modeRow.x1, modeRow.y1, modeRow.x1 + halfW, modeRow.y2, true);
	add(ID_MODE_DUAL, modeRow.x2 - halfW, modeRow.y1, modeRow.x2, modeRow.y2, true);

	// 操作行：积攒/释放真气炸弹/跳过，没有积攒过炸弹时释放不可用
	int thirdW = (opRow.x2 - opRow.x1 - 2 * gap) / 3;
	add(ID_ACCUMULATE, opRow.x1, opRow.y1, opRow.x1 + thirdW, opRow.y2, true);
	add(ID_BOMB, opRow.x1 + thirdW + gap, opRow.y1, opRow.x1 + 2 * thirdW + gap, opRow.y2,
		player && player->GetBomb() > 0);
	add(ID_SKIP, opRow.x2 - thirdW, opRow.y1, opRow.x2, opRow.y2, true);

	// 招式list：只生成当前可见的行
	int rowCount = player ? static_cast<int>(player->GetActions().size()) : 0;
	bool scrollable = rowCount > visibleRows;
	int rowRight = scrollable ? trackRect.x1 - gap / 2 : listRect.x2;
	for (int r = 0; r < visibleRows; ++r) {
		int idx = scrollOffset + r;
		if (idx >= rowCount) break;
		int y1 = listRect.y1 + r * (rowH + rowGap);
		add(ID_ROW_BASE + idx, listRect.x1, y1, rowRight, y1 + rowH, RowEnabled(idx));
	}

	// 滚动条：后加入的优先命中，所以滑块放在轨道后面
	if (scrollable) {
		add(ID_SCROLL_TRACK, trackRect.x1, trackRect.y1, trackRect.x2, trackRect.y2, true);
		add(ID_SCROLL_THUMB, trackRect.x1, ThumbTop(), trackRect.x2, ThumbTop() + ThumbHeight(), true);
	}
}

int WxdjPuzzle::HitTest(int x, int y) const {
	for (auto it = widgets.rbegin(); it != widgets.rend(); ++it) {
		if (x >= it->x1 && x <= it->x2 && y >= it->y1 && y <= it->y2) return it->id;
	}
	return ID_NONE;
}

const WxdjPuzzle::Widget* WxdjPuzzle::FindWidget(int widgetId) const {
	for (const auto& widget : widgets) {
		if (widget.id == widgetId) return &widget;
	}
	return nullptr;
}

void WxdjPuzzle::SubmitPlayerAction(shared_ptr<Action> playerAction) {
	firstPick = -1;
	ResolveTurn(playerAction);
}

void WxdjPuzzle::Activate(int widgetId) {
	const Widget* widget = FindWidget(widgetId);
	if (!widget || !widget->enabled) return;

	if (widgetId == ID_CLOSE) {
		closeRequested = true;
	}
	else if (widgetId == ID_MODE_SINGLE || widgetId == ID_MODE_DUAL) {
		// 切换模式(含重复点击当前模式)都会取消已选的第一招
		mode = (widgetId == ID_MODE_SINGLE) ? MODE_SINGLE : MODE_DUAL;
		firstPick = -1;
	}
	else if (widgetId == ID_ACCUMULATE) {
		SubmitPlayerAction(make_shared<AccumulateAction>());
	}
	else if (widgetId == ID_BOMB) {
		SubmitPlayerAction(make_shared<BombAction>());
	}
	else if (widgetId == ID_SKIP) {
		SubmitPlayerAction(make_shared<SkipAction>());
	}
	else if (widgetId >= ID_ROW_BASE) {
		int idx = widgetId - ID_ROW_BASE;
		if (mode == MODE_SINGLE) {
			SubmitPlayerAction(player->GetAction(idx));
		}
		else if (firstPick < 0) {
			firstPick = idx;
		}
		else {
			SubmitPlayerAction(player->GetAction(firstPick, idx));
		}
	}
}

string WxdjPuzzle::FormatLog(const Log& log) const {
	auto nameOf = [&](int idx) -> string {
		if (idx == 0) return player->GetName();
		if (idx == 1) return enemy->GetName();
		return "";
	};
	auto cultivatorOf = [&](int idx) -> const shared_ptr<Cultivator>& {
		return idx == 0 ? player : enemy;
	};

	ostringstream oss;
	switch (log.type) {
	case LOG_MP:
		oss << nameOf(log.subject) << "真气量回复至" << cultivatorOf(log.subject)->GetCurrentMP();
		break;
	case LOG_ATK:
		oss << nameOf(log.subject) << "武力值回复至" << cultivatorOf(log.subject)->GetCurrentATK();
		break;
	case LOG_HP:
		oss << nameOf(log.subject) << "健康值回复至" << cultivatorOf(log.subject)->GetCurrentHP();
		break;
	case LOG_CONSUME:
		oss << nameOf(log.subject) << "消耗" << log.amount << "点武力值";
		break;
	case LOG_SKIP:
		oss << nameOf(log.subject) << "跳过回合";
		break;
	case LOG_MOVE:
		oss << nameOf(log.subject) << "消耗" << log.amount << "点真气对" << nameOf(log.object)
			<< "出招：" << log.name << "，点数" << log.pointAtk;
		break;
	case LOG_DAMAGE:
		oss << nameOf(log.subject) << "对" << nameOf(log.object) << "造成";
		if (log.pointAtk > 0) oss << Action::attributeText[log.attribute] << log.pointAtk << "点招式武力伤害";
		else oss << log.pointHp << "点招式物理伤害";
		break;
	case LOG_DODGE:
		oss << nameOf(log.object) << "闪避成功";
		break;
	case LOG_FAIL:
		oss << nameOf(log.subject) << "出招失败";
		break;
	case LOG_EFFECT:
		switch (log.effect) {
		case EFFECT_PENETRATE:
			oss << nameOf(log.subject) << "对" << nameOf(log.object) << "造成";
			if (log.pointAtk > 0) oss << Action::attributeText[log.attribute] << log.pointAtk << "点穿透武力伤害";
			else oss << log.pointHp << "点穿透物理伤害";
			break;
		case EFFECT_REBOUND:
			oss << nameOf(log.subject) << "对" << nameOf(log.object) << "造成";
			if (log.pointAtk > 0) oss << Action::attributeText[log.attribute] << log.pointAtk << "点反弹武力伤害";
			else oss << log.pointHp << "点反弹物理伤害";
			break;
		case EFFECT_ABSORB:
			oss << nameOf(log.subject) << "吸收" << log.amount << "点真气";
			break;
		case EFFECT_LOCK:
			oss << nameOf(log.object) << "被" << nameOf(log.subject) << "禁锢";
			break;
		case EFFECT_DOT:
			oss << nameOf(log.object) << "受到" << log.amount << "点持续武力伤害";
			break;
		case EFFECT_REBATE:
			oss << nameOf(log.subject) << "恢复" << log.amount << "点真气";
			break;
		case EFFECT_RECOVER:
			oss << nameOf(log.subject) << "恢复" << log.amount << "点武力值";
			break;
		case EFFECT_CURE:
			oss << nameOf(log.subject) << "恢复" << log.amount << "点健康值";
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return oss.str();
}

void WxdjPuzzle::PushLogLine(const string& text) {
	if (text.empty()) return;
	logLines.push_back(text);
	if (static_cast<int>(logLines.size()) > MAX_LOG_LINES) {
		logLines.erase(logLines.begin());
	}
}

void WxdjPuzzle::AppendRoundHeader() {
	PushLogLine("== 第" + to_string(roundNumber) + "回合 ==");
}

void WxdjPuzzle::AppendNews() {
	if (!controller) return;
	for (const auto& log : controller->GetNews()) {
		PushLogLine(FormatLog(log));
	}
}

void WxdjPuzzle::ResolveTurn(shared_ptr<Action> playerAction) {
	// 敌方AI：3/4概率出单招(下标范围ac+6含积攒/炸弹等特殊选项)，1/4概率出双招
	int ac = static_cast<int>(enemy->GetActions().size());
	shared_ptr<Action> enemyAction;
	if (GetRandom(4)) enemyAction = enemy->GetAction(GetRandom(ac + 6));
	else enemyAction = enemy->GetAction(GetRandom(ac), GetRandom(ac));

	// pair的第二个字段是该招式指向的对手下标：玩家(0)出招指向敌人(1)，敌人(1)出招指向玩家(0)
	controller->ActionTurn({ {playerAction, 1}, {enemyAction, 0} });
	AppendNews();
	controller->CheckTurn({ {playerAction, 1}, {enemyAction, 0} });
	AppendNews();
	controller->EndTurn();
	AppendNews();

	if (!player->IsAlive()) {
		// 玩家阵亡(含同归于尽)都判定为失败
		pendingResult = -1;
		phase = PHASE_RESULT;
	}
	else if (!enemy->IsAlive()) {
		pendingResult = 1;
		phase = PHASE_RESULT;
	}
	else {
		++roundNumber;
		AppendRoundHeader();
		controller->StartTurn();
		AppendNews();
	}
}

void WxdjPuzzle::Init(Canvas* canvas, PostHandle* post) {
	canvasW = canvas->GetWidth();
	canvasH = canvas->GetHeight();
	canvas->SetResizeFunc(&WxdjPuzzle::OnCanvasResize, this);
	ComputeLayout();

	phase = PHASE_SELECT;
	loadFailed = false;
	closeRequested = false;
	pendingResult = 0;
	mode = MODE_SINGLE;
	firstPick = -1;
	scrollOffset = 0;
	hoverId = ID_NONE;
	pressedId = ID_NONE;
	dragging = false;
	logLines.clear();
	widgets.clear();
	player.reset();
	enemy.reset();
	controller.reset();

	if (!post) {
		loadFailed = true;
		return;
	}

	JsonValue request(DATA_OBJECT);
	request["post"] = "resource path";
	request["name"] = ACTION_FILE_NAME;
	post->Post(request);
	const JsonValue& result = post->GetResult();
	if (result["result"].AsString() != "success") {
		loadFailed = true;
		return;
	}
	string actionPath = result["path"].AsString();

	playerMaxHP = 1200; playerMaxATK = 1300; playerMaxMP = 1800;
	enemyMaxHP = 1000; enemyMaxATK = 1100; enemyMaxMP = 1600;

	try {
		player = make_shared<Cultivator>("玩家", actionPath, playerMaxHP, playerMaxATK, playerMaxMP, 22, 45);
		enemy = make_shared<Cultivator>("售货员", actionPath, enemyMaxHP, enemyMaxATK, enemyMaxMP, 20, 40);
	}
	catch (const std::exception&) {
		player.reset();
		enemy.reset();
		loadFailed = true;
		return;
	}

	controller = make_unique<Controller>(vector<shared_ptr<Cultivator>>{ player, enemy });
	roundNumber = 1;
	AppendRoundHeader();
	controller->StartTurn();
	AppendNews();
}

vector<string> WxdjPuzzle::WrapText(Canvas* canvas, const string& text, int maxWidth) {
	vector<string> lines;
	string line;

	size_t i = 0;
	while (i < text.size()) {
		// UTF-8首字节决定这个字符占几个字节，保证折行不会把多字节字符劈开
		unsigned char lead = static_cast<unsigned char>(text[i]);
		size_t len = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : (lead >> 3) == 0x1E ? 4 : 1;
		string ch = text.substr(i, len);
		i += len;

		if (!line.empty() && canvas->StringWidth(line + ch) > maxWidth) {
			lines.push_back(line);
			line.clear();
		}
		line += ch;
	}
	if (!line.empty()) lines.push_back(line);
	return lines;
}

void WxdjPuzzle::AttributeColor(ATTRIBUTE_TYPE attribute, int& r, int& g, int& b) {
	switch (attribute) {
	case ATTRIBUTE_METAL: r = 180; g = 180; b = 190; break;
	case ATTRIBUTE_WOOD:  r = 60;  g = 150; b = 70;  break;
	case ATTRIBUTE_WATER: r = 50;  g = 110; b = 200; break;
	case ATTRIBUTE_FIRE:  r = 200; g = 70;  b = 40;  break;
	case ATTRIBUTE_EARTH: r = 170; g = 130; b = 60;  break;
	case ATTRIBUTE_ALL:   r = 150; g = 70;  b = 190; break;
	default:              r = 80;  g = 80;  b = 95;  break;
	}
}

void WxdjPuzzle::ShadeColor(const Widget& widget, int& r, int& g, int& b) const {
	auto clamp = [](int v) { return max(0, min(255, v)); };

	if (!widget.enabled) {
		// 禁用：向暗灰压，不响应悬停/按下
		r = r * 35 / 100 + 35;
		g = g * 35 / 100 + 35;
		b = b * 35 / 100 + 35;
		return;
	}

	// 滑块被拖着的时候，鼠标跑出滑块范围也保持按下的暗色
	bool pressed = pressedId == widget.id && (hoverId == widget.id || (widget.id == ID_SCROLL_THUMB && dragging));
	if (pressed) {
		r = clamp(r * 65 / 100);
		g = clamp(g * 65 / 100);
		b = clamp(b * 65 / 100);
	}
	else if (hoverId == widget.id && pressedId == ID_NONE) {
		r = clamp(r + 40);
		g = clamp(g + 40);
		b = clamp(b + 40);
	}
}

void WxdjPuzzle::DrawButton(Canvas* canvas, const Widget& widget, const string& label,
	int r, int g, int b, bool selected) {
	ShadeColor(widget, r, g, b);
	canvas->SetAlpha(1.f);
	canvas->SetColor(r, g, b);
	canvas->PutRect(widget.x1, widget.y1, widget.x2, widget.y2, true);

	if (selected) canvas->SetColor(255, 230, 120);
	else canvas->SetColor(15, 15, 20);
	canvas->PutRect(widget.x1, widget.y1, widget.x2, widget.y2, false);
	if (selected) canvas->PutRect(widget.x1 + 1, widget.y1 + 1, widget.x2 - 1, widget.y2 - 1, false);

	// 文字太宽就缩小字号，保证窄窗口里也放得下
	int fontSize = max(12, (widget.y2 - widget.y1) / 3);
	canvas->SetFontSize(fontSize);
	while (fontSize > 10 && canvas->StringWidth(label) > widget.x2 - widget.x1 - 12) {
		canvas->SetFontSize(--fontSize);
	}

	if (widget.enabled) canvas->SetColor(255, 255, 255);
	else canvas->SetColor(150, 150, 150);
	int textW = canvas->StringWidth(label);
	canvas->PutString(label, widget.x1 + (widget.x2 - widget.x1 - textW) / 2,
		widget.y1 + (widget.y2 - widget.y1 - canvas->GetFontHeight()) / 2);
}

void WxdjPuzzle::DrawStatusBar(Canvas* canvas, const shared_ptr<Cultivator>& cultivator, int x, int y, int w) {
	bool isPlayer = (cultivator == player);
	int maxHP = isPlayer ? playerMaxHP : enemyMaxHP;
	int maxMP = isPlayer ? playerMaxMP : enemyMaxMP;
	int maxATK = isPlayer ? playerMaxATK : enemyMaxATK;
	int hp = max(0, cultivator->GetCurrentHP());
	int mp = max(0, cultivator->GetCurrentMP());
	int atk = max(0, cultivator->GetCurrentATK());

	int nameFontSize = max(13, statusBarH / 6);
	canvas->SetColor(230, 230, 230);
	canvas->SetFontSize(nameFontSize);
	canvas->PutString(cultivator->GetName(), x, y);

	int barH = max(12, statusBarH / 6);
	int rowGap = barH + 5;
	int barY = y + nameFontSize + 6;
	int barW = static_cast<int>(w * 0.6); // 条只占约60%宽度，右边留给数值文字，保证条和数字直观对应
	int textFontSize = max(11, barH - 2);

	// MP(真气,蓝)、ATK(武力值,黄)、HP(健康值,红)，自上而下——武力值是普通武力伤害真正扣减的资源，
	// 阵亡判定也要求武力值>0，所以也要画出来。
	auto drawRow = [&](const char* label, int curVal, int maxVal, int r, int g, int b, int rowY) {
		canvas->SetColor(30, 20, 20);
		canvas->SetAlpha(1.f);
		canvas->PutRect(x, rowY, x + barW - 1, rowY + barH - 1, true);
		if (maxVal > 0 && curVal > 0) {
			canvas->SetColor(r, g, b);
			int fillW = static_cast<int>(static_cast<int64_t>(barW) * min(curVal, maxVal) / maxVal);
			if (fillW > 0) canvas->PutRect(x, rowY, x + fillW - 1, rowY + barH - 1, true);
		}
		canvas->SetColor(255, 255, 255);
		canvas->SetFontSize(textFontSize);
		canvas->PutString(string(label) + " " + to_string(curVal) + "/" + to_string(maxVal),
			x + barW + 8, rowY + max(0, (barH - textFontSize) / 2));
	};

	drawRow("MP", mp, maxMP, 60, 130, 220, barY);
	drawRow("ATK", atk, maxATK, 220, 175, 50, barY + rowGap);
	drawRow("HP", hp, maxHP, 210, 60, 60, barY + rowGap * 2);
}

void WxdjPuzzle::DrawLogPanel(Canvas* canvas) {
	canvas->SetColor(28, 28, 36);
	canvas->SetAlpha(1.f);
	canvas->PutRect(logPanel.x1, logPanel.y1, logPanel.x2, logPanel.y2, true);
	canvas->SetColor(70, 70, 85);
	canvas->PutRect(logPanel.x1, logPanel.y1, logPanel.x2, logPanel.y2, false);

	int fontSize = max(13, canvasH / 45);
	canvas->SetFontSize(fontSize);
	int lineH = canvas->GetFontHeight();
	int maxWidth = logPanel.x2 - logPanel.x1 - 12;

	// 从最新一条往上画，画到面板顶就停——保证最新的战报始终看得见，旧的被挤出去
	int y = logPanel.y2 - 6;
	for (auto it = logLines.rbegin(); it != logLines.rend(); ++it) {
		bool isHeader = it->size() >= 2 && (*it)[0] == '=' && (*it)[1] == '=';
		vector<string> lines = WrapText(canvas, *it, maxWidth);
		int height = static_cast<int>(lines.size()) * lineH + 2 + (isHeader ? 10 : 0);
		if (y - height < logPanel.y1 + 6) break;

		y -= height;
		canvas->SetColor(isHeader ? 230 : 220, isHeader ? 190 : 220, isHeader ? 90 : 220);
		int lineY = y + (isHeader ? 4 : 0);
		for (const auto& line : lines) {
			canvas->PutString(line, logPanel.x1 + 6, lineY);
			lineY += lineH;
		}
	}
}

void WxdjPuzzle::DrawScrollBar(Canvas* canvas) {
	const Widget* track = FindWidget(ID_SCROLL_TRACK);
	const Widget* thumb = FindWidget(ID_SCROLL_THUMB);
	if (!track || !thumb) return;

	int r = 40, g = 40, b = 52;
	canvas->SetAlpha(1.f);
	canvas->SetColor(r, g, b);
	canvas->PutRect(track->x1, track->y1, track->x2, track->y2, true);

	r = 120; g = 120; b = 145;
	ShadeColor(*thumb, r, g, b);
	canvas->SetColor(r, g, b);
	canvas->PutRect(thumb->x1, thumb->y1, thumb->x2, thumb->y2, true);
	canvas->SetColor(15, 15, 20);
	canvas->PutRect(thumb->x1, thumb->y1, thumb->x2, thumb->y2, false);
}

void WxdjPuzzle::DrawActionPanel(Canvas* canvas) {
	// 模式行/操作行
	auto drawFixed = [&](int widgetId, const string& label, int r, int g, int b, bool selected) {
		if (const Widget* widget = FindWidget(widgetId)) DrawButton(canvas, *widget, label, r, g, b, selected);
	};
	drawFixed(ID_MODE_SINGLE, "单招", 70, 90, 130, mode == MODE_SINGLE);
	drawFixed(ID_MODE_DUAL, "双招", 70, 90, 130, mode == MODE_DUAL);
	drawFixed(ID_ACCUMULATE, "积攒真气炸弹", 140, 100, 40, false);
	drawFixed(ID_BOMB, "释放真气炸弹(" + to_string(player->GetBomb()) + ")", 170, 60, 50, false);
	drawFixed(ID_SKIP, "跳过", 90, 90, 100, false);

	// 提示行
	string hint;
	const auto& actions = player->GetActions();
	if (mode == MODE_SINGLE) hint = "单招：点击招式立即出招";
	else if (firstPick < 0) hint = "双招：请选择第一招";
	else hint = "双招：第一招「" + actions[firstPick].GetName() + "」，请选择第二招";
	canvas->SetColor(230, 190, 90);
	canvas->SetFontSize(max(12, hintH - 4));
	canvas->PutString(hint, hintRect.x1, hintRect.y1);

	// 招式list
	int fontSize = max(12, rowH / 2);
	canvas->SetFontSize(fontSize);
	for (const auto& widget : widgets) {
		if (widget.id < ID_ROW_BASE) continue;
		int idx = widget.id - ID_ROW_BASE;
		const SingleAction& action = actions[idx];

		int ar, ag, ab;
		AttributeColor(action.GetAttribute(), ar, ag, ab);

		int r = 60, g = 60, b = 72;
		ShadeColor(widget, r, g, b);
		canvas->SetAlpha(1.f);
		canvas->SetColor(r, g, b);
		canvas->PutRect(widget.x1, widget.y1, widget.x2, widget.y2, true);

		// 左侧属性色条
		int sr = ar, sg = ag, sb = ab;
		Widget stripe = widget;
		ShadeColor(stripe, sr, sg, sb);
		canvas->SetColor(sr, sg, sb);
		canvas->PutRect(widget.x1, widget.y1, widget.x1 + 5, widget.y2, true);

		// 双招模式下已选的第一招用亮边框标出
		bool picked = (mode == MODE_DUAL && firstPick == idx);
		canvas->SetColor(picked ? 255 : 15, picked ? 230 : 15, picked ? 120 : 20);
		canvas->PutRect(widget.x1, widget.y1, widget.x2, widget.y2, false);
		if (picked) canvas->PutRect(widget.x1 + 1, widget.y1 + 1, widget.x2 - 1, widget.y2 - 1, false);

		if (widget.enabled) canvas->SetColor(255, 255, 255);
		else canvas->SetColor(150, 150, 150);
		int textY = widget.y1 + (widget.y2 - widget.y1 - canvas->GetFontHeight()) / 2;
		canvas->PutString(action.GetName(), widget.x1 + 12, textY);

		string cost = "真气" + to_string(action.GetPoint()) + "  威力" + to_string(action.GetPower());
		canvas->PutString(cost, widget.x2 - 8 - canvas->StringWidth(cost), textY);
	}

	DrawScrollBar(canvas);
}

void WxdjPuzzle::DrawResultOverlay(Canvas* canvas) {
	bool won = (pendingResult > 0);

	canvas->SetColor(won ? 20 : 180, won ? 160 : 20, won ? 50 : 20);
	canvas->SetAlpha(0.6f);
	canvas->PutRect(0, 0, canvasW - 1, canvasH - 1, true);
	canvas->SetAlpha(1.f);

	canvas->SetColor(255, 255, 255);
	canvas->SetFontSize(max(24, canvasH / 12));
	string text = won ? "挑战成功" : "挑战失败";
	int textW = canvas->StringWidth(text);
	canvas->PutString(text, (canvasW - textW) / 2, canvasH / 2 - canvasH / 12);

	if (const Widget* close = FindWidget(ID_CLOSE)) DrawButton(canvas, *close, "关闭", 90, 90, 100, false);
}

void WxdjPuzzle::DrawScene(Canvas* canvas) {
	canvas->SetColor(18, 18, 24);
	canvas->SetAlpha(1.f);
	canvas->PutRect(0, 0, canvasW - 1, canvasH - 1, true);

	if (!player || !enemy) return;

	int halfW = (canvasW - 3 * margin) / 2;
	DrawStatusBar(canvas, player, margin, margin, halfW);
	DrawStatusBar(canvas, enemy, margin * 2 + halfW, margin, halfW);

	DrawLogPanel(canvas);
	DrawActionPanel(canvas);

	if (phase == PHASE_RESULT) DrawResultOverlay(canvas);
}

int WxdjPuzzle::Loop(Canvas* canvas, int ms, PostHandle* post) {
	if (loadFailed) {
		canvas->SetColor(18, 18, 24);
		canvas->SetAlpha(1.f);
		canvas->PutRect(0, 0, canvasW - 1, canvasH - 1, true);
		canvas->SetColor(220, 60, 60);
		canvas->SetFontSize(max(16, canvasH / 25));
		canvas->PutString("招式数据加载失败", margin, margin);
		return -1;
	}

	RebuildWidgets();

	MouseState mouse = canvas->GetMouseState();
	hoverId = HitTest(mouse.x, mouse.y);

	// 滚轮：鼠标在操作栏面板(含list和滚动条)内才滚动list，一刻度滚一行，向上滚为正
	int wheel = canvas->ConsumeMouseWheel();
	if (phase == PHASE_SELECT && wheel != 0 &&
		mouse.x >= actionPanel.x1 && mouse.x <= actionPanel.x2 &&
		mouse.y >= actionPanel.y1 && mouse.y <= actionPanel.y2) {
		scrollOffset -= wheel;
		ClampScroll();
		RebuildWidgets();
	}

	// 鼠标事件：按下记录控件，抬起时仍在同一个控件上才算一次点击(按钮标准语义)
	while (canvas->HasPendingMouseEvent()) {
		MouseEvent event = canvas->PopMouseEvent();
		if (event.button != MOUSE_LEFT) continue;

		int target = HitTest(event.x, event.y);
		if (event.down) {
			pressedId = target;
			if (target == ID_SCROLL_THUMB) BeginScrollDrag(event.y);
		}
		else {
			dragging = false;
			if (pressedId != ID_NONE && pressedId == target) {
				if (target == ID_SCROLL_TRACK) {
					// 点轨道空白处：往点击的方向翻一页
					scrollOffset += (event.y < ThumbTop()) ? -visibleRows : visibleRows;
					ClampScroll();
				}
				else if (target != ID_SCROLL_THUMB) {
					Activate(target);
				}
				RebuildWidgets();
			}
			pressedId = ID_NONE;
		}
	}

	// 左键已经松开(比如在窗口外松开，没收到抬起事件)就清掉按下状态
	mouse = canvas->GetMouseState();
	if (!mouse.buttons[MOUSE_LEFT]) {
		pressedId = ID_NONE;
		dragging = false;
	}
	if (dragging) {
		UpdateScrollDrag(mouse.y);
		RebuildWidgets();
	}

	hoverId = HitTest(mouse.x, mouse.y);

	DrawScene(canvas);
	return closeRequested ? pendingResult : 0;
}
