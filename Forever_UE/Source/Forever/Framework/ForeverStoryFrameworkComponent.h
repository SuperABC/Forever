#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"

#include "story/dialog.h"

#include "ForeverStoryFrameworkComponent.generated.h"

class Story;
class Change;
class ChangeControlChange;
class StartPuzzleChange;
class PayWageChange;
class CreateTimerChange;
class RemoveTimerChange;
class Script;
class Citizen;
class Room;
class Container;
class EnterVehicleChange;
class BoardVehicleChange;
class LeaveVehicleChange;
class ForceLeaveChange;
class Vehicle;
class Asset;
struct ScriptContext;

class PostImplement;
class Event;
class PuzzleResultEvent;
class CitizenDeceaseEvent;

// Story域的UE层接线点：事件广播入口、Dialog排队展示、需要UE对象的Change执行、计时器。
// 详见ForeverStoryFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverStoryFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	// 前台剧情对话是否正在展示(队列非空)——对话进行中不允许存档：对话队列里的中间状态不进存档。
	// 后台对话不算(它不抢操控，存档时直接丢弃)。
	bool IsDialogActive() const { return pendingQueue.Num() > 0; }

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 由AForeverFrameworkActor在Story::Init()跑完后调用一次。story生命周期由调用方持有。
	void Init(Story* inStory);

	// 广播一次GameStartEvent：主线走Story::BroadcastGameStart，其余所有挂了Script的对象走
	// BroadcastToAllScripts(includeMain=false)，否则它们的game_start milestone永远不会触发。
	// 匹配出的Dialog交给ShowDialog、Change转发给ApplyChange。
	void BroadcastGameStart();

	// 把游戏时钟写进系统变量system.time.year/month/day/hour/minute/second，剧情脚本的表达式里用
	// (比如spawn_npc的birthday "$$system.time.year-17+年8月31日")。AForeverFrameworkActor::Tick
	// 每帧在Player::Tick推进时钟之后调用一次；开局广播game_start之前也调用，保证第一个事件就能读到。
	void UpdateSystemTime();

	// 玩家点击MeetOption选项时调用：构造OptionDialogEvent，匹配主线Script和name对应目标
	// (市民/车辆/门/资产/电梯)自己的Script，见.md。
	// @name: 对话目标（市民）姓名字面量
	// @option: 被点击的选项文本字面量
	void OptionDialog(const FString& name, const FString& option);

	// 市民死亡时调用：构造CitizenDeceaseEvent，匹配主线Script + 死者自己的Scheduler Script
	// (死亡只销毁UE侧Actor，Core里的Citizen和Scheduler还在)。
	// 为什么不做通用广播见Source/Forever/Player/ForeverWeaponComponent.md。
	// @name: 死者姓名字面量
	// @reason: 死亡原因字面量（玩家用武器击杀时是"player"，武器种类见CitizenDeceaseEvent::weapon）
	void BroadcastCitizenDecease(const FString& name, const FString& reason);

	// 市民真正走到一个新房间时调用(UForeverPopulaceFrameworkComponent::ReportArrival)：
	// 构造npc_arrive事件，只发给这个市民自己的Scheduler脚本和Job脚本(不广播)，有嵌套深度保护。
	void BroadcastNpcArrive(Citizen* citizen, Room* room);

	// 同上，但直接发一个已经填好击杀信息(凶手/武器/部位/伤害/可见/死亡地址)的事件，武器击杀用。
	void BroadcastCitizenDecease(CitizenDeceaseEvent& event);

	// 执行global_message这个Change时调用(AForeverFrameworkActor::ApplyChange)：构造GlobalMessageEvent，
	// 广播给所有挂了Script的东西(含主线，范围同game_start)。嵌套深度超过kMaxGlobalMessageDepth时
	// 丢弃并警告，防止两个里程碑互相触发无限递归。
	// @message: 已经求值好的消息文本
	void BroadcastGlobalMessage(const std::string& message);

	// 天气变了(自动随机换或脚本change_weather)时由AForeverFrameworkActor::Tick调用：广播
	// WeatherChangeEvent给所有脚本(含主线)，范围和global_message一样。读档不广播。
	// @weather: 变化后的天气名(sunny/cloudy/overcast/light_rain/heavy_rain/light_snow/heavy_snow)
	void BroadcastWeatherChange(const std::string& weather);

	// 一句带label的台词播完(玩家点击/空格推进离开这一句)时由AdvanceDialog调用：广播
	// SpeakingFinishEvent给所有脚本(含主线)，范围和global_message一样。
	void BroadcastSpeakingFinish(const std::string& label);

	// 玩家进出园区/建筑/房间(各自的Trigger碰撞盒Overlap回调里调用)：先更新系统变量
	// system.player.zone/building/room(进入时写名字；离开时只有当前值还是这个名字才清空，
	// 避免"先进B后离开A"的回调顺序把刚写进去的B清掉)，再把事件**只发给target**(被进出的那个
	// zone/building/room自己的Script，target为空=它没挂脚本，不发给任何人)，不再广播给其它脚本。
	// kind是"zone"/"building"/"room"。
	void BroadcastPlayerLocation(Event& event, const std::string& kind, const std::string& name, bool entering,
		Script* target);

	// 计时器(见Core/story/story.md"计时器")：create_timer/remove_timer由AForeverFrameworkActor::
	// ApplyChange转发到这里，FireExpiredTimers每帧在Story::Tick之后调用。
	void ApplyCreateTimer(const CreateTimerChange* change, const ScriptContext& context);
	void ApplyRemoveTimer(const RemoveTimerChange* change, const ScriptContext& context);
	void FireExpiredTimers(float deltaSeconds);

	// ChangeControlChange的实际执行：把玩家操控权切换到指定姓名的市民身上。Core层不认识
	// Actor/Controller，所以不在Story::ApplyChange里处理；由AForeverFrameworkActor::ApplyChange转发到这里。
	// @change: 待执行的切换控制变化
	// @context: 变量路由上下文，用于求值change->GetName()
	void ApplyControlChange(const ChangeControlChange* change, const ScriptContext& context);

	// StartPuzzleChange的实际执行：让AForeverPlayerController持有的PuzzleWidget开始一局小游戏
	// (Core层不知道UMG Widget，理由同ApplyControlChange)，并记下启动它的脚本。
	// @change: 待执行的开始小游戏变化
	// @context: 变量路由上下文，用于求值change->GetPuzzle()
	void ApplyStartPuzzle(const StartPuzzleChange* change, const ScriptContext& context);

	// 小游戏结束时由UPuzzleWidget::EndPuzzle调用：给启动它的那个脚本(ApplyStartPuzzle记下的)发
	// puzzle_result事件，事件里是已展平的结果字段，脚本里用local.<字段名>读，用来在小游戏结束后
	// 按成绩发工资之类。
	// @event: 已经SetFromJson好的结果事件
	void NotifyPuzzleFinished(PuzzleResultEvent& event);

	// open_shop：找到执行这个Change的Job脚本所属的岗位和组织，打开商店界面并让它记录这个组织
	// (购买时付款给它)。
	void ApplyOpenShop(const ScriptContext& context);

	// pay_wage：由执行这个Change的Job脚本所属组织，给被玩家操控的市民发一笔工资(Flow::WAGE，
	// 代扣所得税)。玩家有没有真的去上班不影响每日工资结算，这是玩家手动打工额外拿的一份。
	void ApplyPayWage(const PayWageChange* change, const ScriptContext& context);

	// EnterVehicleChange的实际执行：求值载具名后转发给UForeverTrafficFrameworkComponent::ApplyEnterVehicle
	// (查找/隐藏原pawn/Possess都在那边)。
	// @change: 待执行的进入载具变化
	// @context: 变量路由上下文，用于求值change->GetVehicle()
	void ApplyEnterVehicle(const EnterVehicleChange* change, const ScriptContext& context);

	// board_vehicle(搭乘线路公交)/leave_vehicle(下车)/force_leave(强制下车)的实际执行，同样只能在UE层做，
	// 真正的逻辑在UForeverTrafficFrameworkComponent::ApplyBoardVehicle/ApplyLeaveVehicle。
	void ApplyBoardVehicle(const BoardVehicleChange* change, const ScriptContext& context);
	void ApplyLeaveVehicle(const LeaveVehicleChange* change, const ScriptContext& context);
	void ApplyForceLeave(const ForceLeaveChange* change, const ScriptContext& context);

	// 把一个事件只发给这辆载具自己的Script(enter_stop/leave_stop/enter_vehicle/leave_vehicle)，匹配出的
	// Dialog/Change交给ProcessScriptActions。内部现场构造PostImplement。
	void BroadcastToVehicle(Event& event, Vehicle* vehicle);

	// 资产(Asset)脚本：新建的资产(凭空给出/第一次变成场景物体)若有脚本且还没收过game_start，补发一次
	// GameStartEvent给它自己的脚本(脚本里一般在这时add_option)，并置scriptStarted。含容器内容。
	void StartAssetScript(Asset* asset);

	// get_asset/lose_asset事件分发：对root及其递归容器内的每一件资产各发一次(根资产是容器时容器本身和里面
	// 所有的都要发)。sender非空(来自pick/grant/deduct/drop这些Change)=发给执行Change的那个脚本；sender为空
	// (背包界面里的移动)=发给该资产自己的脚本(没有脚本则不发)。同时在屏幕左上角显示“获取/失去 xxx”。
	// result是"success"或"fail"。
	void NotifyAssetEvent(Asset* root, bool bGet, const std::string& result, Script* sender);

	// 没有具体资产实例的失败(身上没有指定类型/数量不足)：只按类型发一个事件给sender。
	void NotifyAssetResult(bool bGet, const std::string& type, const std::string& result, Script* sender);

	// 把匹配出的Dialog求值并展平进pendingQueue展示：队列空时立刻显示，否则排到队尾(不打断、不并发)。
	// 求值必须在这次调用里同步完成，因为context.local指向调用方栈上的Event，返回后即悬垂；
	// 入队的context会清掉local，详见.md"Dialog展示"一节。
	// @dialog: 待展示的对话（引用，不持有所有权，本体挂在触发它的Milestone上）
	// @context: 变量路由上下文，只在这次调用同步使用
	void ShowDialog(const Dialog* dialog, const ScriptContext& context);

	// SectionSpeaking点击/空格后调用：关闭当前台词(带label则广播speaking_finish)，推进到下一项。队列为空时no-op。
	void AdvanceDialog();

	// SectionButton点击后调用：执行被选中Option的GetChanges()(转发ApplyChange)，并把它的GetDialogs()
	// 展平插到队头紧接着播放，然后推进显示。队首不是分支或index越界时no-op。
	// @index: 入队时过滤condition之后的选项数组下标
	void SelectDialogOption(int32 index);

private:
	// 把event广播给组织/岗位/市民Scheduler/车辆/门的全部Script(includeMain为true时连主线也算)，
	// 匹配出的Dialog/Change交给ProcessScriptActions。BroadcastGameStart和BroadcastGlobalMessage共用。
	void BroadcastToAllScripts(Event& event, PostImplement& postImplement, bool includeMain);

	// 只把event发给一个Script(进出事件用)，匹配出的Dialog/Change交给ProcessScriptActions。
	void BroadcastToScript(Event& event, Script* script, PostImplement& postImplement);

	// 计时器归属：Script* <-> ScriptSaveCodec::CollectScripts的稳定键。找不到返回空串/nullptr。
	std::string FindScriptKey(const Container* script) const;
	Script* FindScriptByKey(const std::string& key) const;
	// create_timer的category/label指定的目标脚本(category为空时就是self)。
	const Container* ResolveTimerOwner(const std::string& category, const std::string& label, const Container* self) const;

	// get_asset/lose_asset事件处理里又执行Change再触发事件的嵌套深度，超过上限丢弃并警告，防止互相触发。
	int32 assetEventDepth = 0;
	static constexpr int32 kMaxAssetEventDepth = 8;

	// 当前这局小游戏是哪个脚本启动的(稳定键，见FindScriptKey)，结束时给它发puzzle_result。
	std::string puzzleOwnerKey;

	// npc_arrive处理里又执行npc_teleport等再触发npc_arrive的嵌套深度，超过上限丢弃并警告。
	int32 npcArriveDepth = 0;
	static constexpr int32 kMaxNpcArriveDepth = 8;

	// BroadcastGlobalMessage当前的嵌套深度，见那边的注释。
	int32 globalMessageDepth = 0;
	static constexpr int32 kMaxGlobalMessageDepth = 8;

	// 队列里的一项：要么是一条已求值好的台词，要么是一个分支（已求值好的选项文本+原始Option
	// 拷贝，供SelectDialogOption执行）。
	struct FPendingItem {
		bool bBranch = false;

		// bBranch为false时有效
		FString speaker;
		FString content;
		// 大于0时是后台对话(显示秒数)：排在pendingQueue里等前面的前台条目播完，轮到队首时由
		// ShowCurrentItem转进backgroundQueue，见ForeverStoryFrameworkComponent.md"后台对话"。
		float background = 0.f;
		// 台词的label(已求值)，非空时这一句播完广播speaking_finish
		std::string label;

		// bBranch为true时有效，两个数组下标一一对应
		TArray<FString> optionTexts;
		TArray<Option> options;

		// 求值/执行这一项时用的上下文——self/system取自ShowDialog传入的context，local强制清空。
		ScriptContext context;
	};

	// 把dialog->GetDialogs()展平成若干FPendingItem，插入pendingQueue的insertIndex位置，求值在这里同步完成。
	// @insertIndex: 0表示插到队头（SelectDialogOption的嵌套对话用），
	// pendingQueue.Num()表示追加到队尾（ShowDialog用）
	void EnqueueDialog(const Dialog* dialog, const ScriptContext& context, int32 insertIndex);

	// 显示pendingQueue最前面那一项（不弹出，弹出由AdvanceDialog/SelectDialogOption做）；
	// 队列为空则隐藏两个Widget并恢复游戏输入。找不到AForeverPlayerController/Widget时静默no-op。
	void ShowCurrentItem();

	// 后台对话：显示backgroundQueue队首并开始计时；队列为空就隐藏后台对话控件。
	void ShowCurrentBackground();
	// 计时到：弹出队首(带label就广播speaking_finish)，显示下一条。
	void AdvanceBackground();

	Story* story = nullptr;

	TArray<FPendingItem> pendingQueue;

	// 已经放行的后台台词，按顺序一条条计时播放，和pendingQueue互不阻塞。
	TArray<FPendingItem> backgroundQueue;
	FTimerHandle backgroundTimer;
};
