#pragma once

#include "../common/utility.h"
#include "../common/error.h"
#include "../common/json.h"
#include "expression.h"

#include <functional>
#include <string>
#include <map>


#undef GetMessage


// Event::GetLocalValue把值交出去用的回调，见那里的说明。
using ValueReceiver = std::function<void(const ValueType&)>;

// 事件基类：Match默认只比较GetType()，需要字段级过滤的子类override，见event.md
class Event {
public:
	Event();

	virtual ~Event();

	virtual const std::string& GetType() const = 0;

	virtual bool Match(Event* e, const ScriptContext& context);

	const std::string& GetCondition() const;

	void SetCondition(const std::string& condition);

	// 取这个事件的一个字段(脚本里的local.xxx)。有这个字段就把值交给receive并返回true，没有返回false(不调receive)。
	// 不按值返回：事件对象和调用方可能在不同模块里(宿主建的事件、mod来读)，按值返回带字符串的ValueType就是
	// "一侧分配、另一侧释放"(CONVENTIONS.md第4节)。现在值在事件这一侧算出来、这一侧销毁，调用方在receive里
	// 自己拷一份——receive的函数体编译在调用方那一侧。和NameMod::GenerateName的setResult是同一个做法。
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const;

private:
	// 控制条件
	std::string condition;
};

// 游戏开始
class GameStartEvent : public Event {
public:
	GameStartEvent();

	virtual ~GameStartEvent();

	virtual const std::string& GetType() const override;

private:
};

// 全局消息
class GlobalMessageEvent : public Event {
public:
	GlobalMessageEvent(std::string message);

	virtual ~GlobalMessageEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetMessage(std::string message);

	const std::string& GetMessage() const;

private:
	// 消息内容
	std::string message;
};

// 选项对话
class OptionDialogEvent : public Event {
public:
	OptionDialogEvent(std::string name, std::string option);

	virtual ~OptionDialogEvent();

	virtual const std::string& GetType() const override;

	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetName(std::string name);

	const std::string& GetName() const;

	void SetOption(std::string option);

	const std::string& GetOption() const;

private:
	// 选项名称
	std::string name;

	// 选项文本
	std::string option;
};

// 对话完成
class SpeakingFinishEvent : public Event {
public:
	SpeakingFinishEvent(std::string label);

	virtual ~SpeakingFinishEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetLabel(std::string label);

	const std::string& GetLabel() const;

private:
	// 对话标签
	std::string label;
};

// 进入园区
class EnterZoneEvent : public Event {
public:
	EnterZoneEvent(std::string zone);

	virtual ~EnterZoneEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetZone(std::string zone);

	const std::string& GetZone() const;

private:
	// 园区名称
	std::string zone;
};

// 离开园区
class LeaveZoneEvent : public Event {
public:
	LeaveZoneEvent(std::string zone);

	virtual ~LeaveZoneEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetZone(std::string zone);

	const std::string& GetZone() const;

private:
	// 园区名称
	std::string zone;
};

// 进入建筑
class EnterBuildingEvent : public Event {
public:
	EnterBuildingEvent(std::string zone, std::string building);

	virtual ~EnterBuildingEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetZone(std::string zone);

	const std::string& GetZone() const;

	void SetBuilding(std::string building);

	const std::string& GetBuilding() const;

private:
	// 园区名称
	std::string zone;

	// 建筑名称
	std::string building;
};

// 离开建筑
class LeaveBuildingEvent : public Event {
public:
	LeaveBuildingEvent(std::string zone, std::string building);

	virtual ~LeaveBuildingEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetZone(std::string zone);

	const std::string& GetZone() const;

	void SetBuilding(std::string building);

	const std::string& GetBuilding() const;

private:
	// 园区名称
	std::string zone;

	// 建筑名称
	std::string building;
};

// 进入房间
class EnterRoomEvent : public Event {
public:
	EnterRoomEvent(std::string zone, std::string building, std::string room);

	virtual ~EnterRoomEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetZone(std::string zone);

	const std::string& GetZone() const;

	void SetBuilding(std::string building);

	const std::string& GetBuilding() const;

	void SetRoom(std::string room);

	const std::string& GetRoom() const;

private:
	// 园区名称
	std::string zone;

	// 建筑名称
	std::string building;

	// 房间名称
	std::string room;
};

// 离开房间
class LeaveRoomEvent : public Event {
public:
	LeaveRoomEvent(std::string zone, std::string building, std::string room);

	virtual ~LeaveRoomEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetZone(std::string zone);

	const std::string& GetZone() const;

	void SetBuilding(std::string building);

	const std::string& GetBuilding() const;

	void SetRoom(std::string room);

	const std::string& GetRoom() const;

private:
	// 园区名称
	std::string zone;

	// 建筑名称
	std::string building;

	// 房间名称
	std::string room;
};

// 小游戏结果
class PuzzleResultEvent : public Event {
public:
	PuzzleResultEvent();

	virtual ~PuzzleResultEvent();

	virtual const std::string& GetType() const override;

	virtual bool Match(Event* e, const ScriptContext& context) override;

	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	bool SetFromJson(const JsonValue& root);

	const std::map<std::string, ValueType>& GetFields() const;

private:
	// 递归展平
	void Flatten(const std::string& name, const JsonValue& value);

	// 展平后的结果字段
	std::map<std::string, ValueType> fields;
};

// 交易结果
class TransactionResultEvent : public Event {
public:
	TransactionResultEvent(std::string result, std::string name);

	virtual ~TransactionResultEvent();

	virtual const std::string& GetType() const override;

	void SetResult(std::string result);

	const std::string& GetResult() const;

	void SetName(std::string name);

	const std::string& GetName() const;

private:
	// 是否成功
	std::string result;

	// 交易对象姓名，为空代表玩家
	std::string name;
};

// 计时器到时
class TimeUpEvent : public Event {
public:
	TimeUpEvent(std::string name);

	virtual ~TimeUpEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetName(std::string name);

	const std::string& GetName() const;

private:
	// 计时器名称
	std::string name;
};

// NPC抵达
class NpcArriveEvent : public Event {
public:
	NpcArriveEvent(std::string name, std::string address);

	virtual ~NpcArriveEvent();

	virtual const std::string& GetType() const override;

	virtual bool Match(Event* e, const ScriptContext& context) override;

	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetName(std::string name);

	const std::string& GetName() const;

	void SetAddress(std::string address);

	const std::string& GetAddress() const;

private:
	// 抵达者姓名
	std::string name;

	// 目标地址
	std::string address;
};

// 员工接到任务(组织的assign_task成功发起后)：只发给该员工的Job脚本。task=任务类别标签，destination=目的房间地址；
// 脚本里的触发条件两个字段都可省略(留空=不限)，运行时用$$local.task/$$local.destination读。
class ExecuteTaskEvent : public Event {
public:
	ExecuteTaskEvent(std::string task, std::string destination);

	virtual ~ExecuteTaskEvent();

	virtual const std::string& GetType() const override;

	virtual bool Match(Event* e, const ScriptContext& context) override;

	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

private:
	std::string task;
	std::string destination;
};

// 任务完成(组织的assign_task派出的员工走进了目的房间)：只发给该员工所在组织的脚本，由它决定下一步(比如再派一个返程任务)。
// task/destination触发条件可省略(留空=不限)。脚本里读$$local.xxx：task(任务标签)、destination(目的房间地址)、
// worker(员工姓名)、origin(这一段出发时员工所在的房间地址)。
class TaskCompleteEvent : public Event {
public:
	TaskCompleteEvent(std::string task, std::string destination, std::string worker = std::string(), std::string origin = std::string());

	virtual ~TaskCompleteEvent();

	virtual const std::string& GetType() const override;

	virtual bool Match(Event* e, const ScriptContext& context) override;

	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

private:
	std::string task;
	std::string destination;
	std::string worker;
	std::string origin;
};

// 任务失败(assign_task派不出去，或派出后因路上出了问题取消)：只发给组织的脚本。除了TaskCompleteEvent的task/destination/
// worker/origin之外多一个reason(no_worker没有可用员工/no_vehicle没有可用载具/no_slot目的地没有能停的车位/
// already_there员工已经在目的房间或同一栋建筑里不用开车/navigation导航失败/vehicle_taken车被玩家开走)。
// 派不出去时worker是指定的员工名(自动挑员工时为空)、origin为空；destination始终是这个任务原来的目的房间地址。
class TaskFailedEvent : public Event {
public:
	TaskFailedEvent(std::string task, std::string destination, std::string worker = std::string(), std::string origin = std::string(),
		std::string reason = std::string());

	virtual ~TaskFailedEvent();

	virtual const std::string& GetType() const override;

	virtual bool Match(Event* e, const ScriptContext& context) override;

	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

private:
	std::string task;
	std::string destination;
	std::string worker;
	std::string origin;
	std::string reason;
};

// 载具抵达：车辆Controller导航到房间(vehicle_navigate)后，只发给这辆车自己的script。
class VehicleArriveEvent : public Event {
public:
	VehicleArriveEvent(std::string vehicle, std::string address);

	virtual ~VehicleArriveEvent();

	virtual const std::string& GetType() const override;

	virtual bool Match(Event* e, const ScriptContext& context) override;

	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetVehicle(std::string vehicle);

	const std::string& GetVehicle() const;

	void SetAddress(std::string address);

	const std::string& GetAddress() const;

private:
	// 抵达的载具名称
	std::string vehicle;

	// 抵达的房间地址
	std::string address;
};

// NPC相遇
class NPCMeetEvent : public Event {
public:
	NPCMeetEvent(std::string npc);

	virtual ~NPCMeetEvent();

	virtual const std::string& GetType() const override;

	void SetNPC(std::string npc);

	const std::string& GetNPC() const;

private:
	// NPC名称
	std::string npc;
};

// 市民出生
class CitizenBornEvent : public Event {
public:
	CitizenBornEvent(std::string name);

	virtual ~CitizenBornEvent();

	virtual const std::string& GetType() const override;

	void SetName(std::string name);

	const std::string& GetName() const;

private:
	// 市民姓名
	std::string name;
};

// 市民死亡
class CitizenDeceaseEvent : public Event {
public:
	CitizenDeceaseEvent(std::string name, std::string reason);

	CitizenDeceaseEvent(std::string name, std::string reason, std::string killer, std::string weapon,
		std::string part, double damage, bool visible, std::string location);

	virtual ~CitizenDeceaseEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetName(std::string name);

	const std::string& GetName() const;

	void SetReason(std::string reason);

	const std::string& GetReason() const;

	void SetKiller(std::string killer);
	const std::string& GetKiller() const;
	void SetWeapon(std::string weapon);
	const std::string& GetWeapon() const;
	void SetPart(std::string part);
	const std::string& GetPart() const;
	void SetDamage(double damage);
	double GetDamage() const;
	void SetVisible(bool visible);
	bool GetVisible() const;
	void SetLocation(std::string location);
	const std::string& GetLocation() const;

private:
	// 市民姓名
	std::string name;

	// 死亡原因
	std::string reason;

	// 击杀信息(武器击杀才有，其它死因为空/0)。killer/weapon/part作为触发条件时是表达式，
	// 其余三个只在运行时事件里有意义。
	std::string killer;
	std::string weapon;
	std::string part;
	double damage = 0.0;
	bool visible = false;
	std::string location;
};

// 受伤
class PlayerInjuredEvent : public Event {
public:
	PlayerInjuredEvent(std::string wound);

	virtual ~PlayerInjuredEvent();

	virtual const std::string& GetType() const override;

	void SetWound(std::string wound);

	const std::string& GetWound() const;

private:
	// 伤势描述
	std::string wound;
};

// 痊愈
class PlayerCuredEvent : public Event {
public:
	PlayerCuredEvent(std::string wound);

	virtual ~PlayerCuredEvent();

	virtual const std::string& GetType() const override;

	void SetWound(std::string wound);

	const std::string& GetWound() const;

private:
	// 痊愈的伤势
	std::string wound;
};

// 生病
class PlayerIllEvent : public Event {
public:
	PlayerIllEvent(std::string illness);

	virtual ~PlayerIllEvent();

	virtual const std::string& GetType() const override;

	void SetIllness(std::string illness);

	const std::string& GetIllness() const;

private:
	// 病症描述
	std::string illness;
};

// 康复
class PlayerRecoverEvent : public Event {
public:
	PlayerRecoverEvent(std::string illness);

	virtual ~PlayerRecoverEvent();

	virtual const std::string& GetType() const override;

	void SetIllness(std::string illness);

	const std::string& GetIllness() const;

private:
	// 康复的病症
	std::string illness;
};

// 短暂休息
class PlayerRestEvent : public Event {
public:
	PlayerRestEvent(std::string minute);

	virtual ~PlayerRestEvent();

	virtual const std::string& GetType() const override;

	void SetMinute(std::string minute);

	const std::string& GetMinute() const;

private:
	// 休息时长（分钟）
	std::string minute;
};

// 睡觉
class PlayerSleepEvent : public Event {
public:
	PlayerSleepEvent(std::string hour);

	virtual ~PlayerSleepEvent();

	virtual const std::string& GetType() const override;

	void SetHour(std::string hour);

	const std::string& GetHour() const;

private:
	// 睡眠时长（小时）
	std::string hour;
};

// 通缉变化
class WantedChangeEvent : public Event {
public:
	WantedChangeEvent(std::string reason, std::string level);

	virtual ~WantedChangeEvent();

	virtual const std::string& GetType() const override;

	void SetReason(std::string reason);

	const std::string& GetReason() const;

	void SetLevel(std::string level);

	const std::string& GetLevel() const;

private:
	// 通缉原因
	std::string reason;

	// 通缉等级
	std::string level;
};

// 被捕
class PlayerArrestedEvent : public Event {
public:
	PlayerArrestedEvent(std::string reason);

	virtual ~PlayerArrestedEvent();

	virtual const std::string& GetType() const override;

	void SetReason(std::string reason);

	const std::string& GetReason() const;

private:
	// 被捕原因
	std::string reason;
};

// 释放
class PlayerReleasedEvent : public Event {
public:
	PlayerReleasedEvent(std::string reason);

	virtual ~PlayerReleasedEvent();

	virtual const std::string& GetType() const override;

	void SetReason(std::string reason);

	const std::string& GetReason() const;

private:
	// 释放原因
	std::string reason;
};

// 天气变化
class WeatherChangeEvent : public Event {
public:
	WeatherChangeEvent(std::string weather);

	virtual ~WeatherChangeEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetWeather(std::string weather);

	const std::string& GetWeather() const;

private:
	// 天气类型
	std::string weather;
};

// 使用资产
// 使用资产：玩家在背包里"使用"一件可用资产(AssetMod::usable!=0，如汉堡)时，只发给这件资产自己的脚本(没有脚本则不发)，
// 事件发出之后UE层才扣数量/销毁。脚本里读$$local.xxx：asset(这件资产的名字)、type(资产mod id)、user(使用者市民姓名)。
// 没有过滤字段，不参与Match(只发给自己的脚本)。
class UseAssetEvent : public Event {
public:
	UseAssetEvent(std::string asset, std::string type = std::string(), std::string user = std::string());

	virtual ~UseAssetEvent();

	virtual const std::string& GetType() const override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetAsset(std::string asset);
	const std::string& GetAsset() const;
	void SetAssetType(std::string type);
	const std::string& GetAssetType() const;
	void SetUser(std::string user);
	const std::string& GetUser() const;

private:
	// 资产名字
	std::string asset;
	// 资产类型(mod id)
	std::string type;
	// 使用者姓名
	std::string user;
};

// 载具(公交)进站并停稳。由UE层在Core检测到停靠边沿后发给这辆载具自己的script(不广播)，
// 让script控制上下车选项：见Resource/Story/vehicle_bus.script。
class EnterStopEvent : public Event {
public:
	EnterStopEvent(std::string stop);

	virtual ~EnterStopEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetStop(std::string stop);
	const std::string& GetStop() const;

private:
	std::string stop;
};

// 载具(公交)启动离站，字段同EnterStopEvent。
class LeaveStopEvent : public Event {
public:
	LeaveStopEvent(std::string stop);
	virtual ~LeaveStopEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetStop(std::string stop);
	const std::string& GetStop() const;

private:
	std::string stop;
};

// 玩家进入载具(驾驶或搭乘都算)。由UE层只发给被进入的那辆载具自己的script。注意：和同名的
// EnterVehicleChange(让玩家去开某辆车)是两套东西——这个是"发生了"的通知，那个是"去做"的指令。
class EnterVehicleEvent : public Event {
public:
	EnterVehicleEvent(std::string vehicle, std::string mode);
	virtual ~EnterVehicleEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetVehicle(std::string vehicle);
	const std::string& GetVehicle() const;
	void SetMode(std::string mode);
	const std::string& GetMode() const;

private:
	std::string vehicle;
	std::string mode;
};

// 玩家离开载具(驾驶或搭乘都算)，字段同EnterVehicleEvent。
class LeaveVehicleEvent : public Event {
public:
	LeaveVehicleEvent(std::string vehicle, std::string mode);
	virtual ~LeaveVehicleEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetVehicle(std::string vehicle);
	const std::string& GetVehicle() const;
	void SetMode(std::string mode);
	const std::string& GetMode() const;

private:
	std::string vehicle;
	std::string mode;
};

// 获得资产：捡起(pick_asset/背包界面拖进身上)或凭空获得(grant_asset)之后通知脚本成功与否。执行Change的
// 情况发给执行这个Change的脚本，背包界面里的移动发给这件资产自己的脚本；根资产是容器时，容器和递归进去
// 的每一件资产各发一次。
class GetAssetEvent : public Event {
public:
	GetAssetEvent(std::string type, std::string result);
	virtual ~GetAssetEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetAssetType(std::string type);
	const std::string& GetAssetType() const;
	void SetResult(std::string result);
	const std::string& GetResult() const;

private:
	std::string type;
	std::string result;
};

// 失去资产：drop_asset/deduct_asset(成功或失败：身上没有指定物品或数量不足)，以及背包界面里把东西
// 放到地面/房间。分发规则同GetAssetEvent。
class LoseAssetEvent : public Event {
public:
	LoseAssetEvent(std::string type, std::string result);
	virtual ~LoseAssetEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetAssetType(std::string type);
	const std::string& GetAssetType() const;
	void SetResult(std::string result);
	const std::string& GetResult() const;

private:
	std::string type;
	std::string result;
};

// 投掷：玩家把一件可投掷资产(手雷/烟雾弹等，AssetMod::throwable)扔出去之后，只发给被扔出的这件资产自己的脚本
// (没有脚本就不发)。脚本里读$$local.type(资产mod id)。手雷脚本在这里create_timer计时爆炸。
class ThrowAssetEvent : public Event {
public:
	// thrower：投掷者(玩家当前操控的市民)姓名，只作为local变量给脚本用，不参与Match。
	ThrowAssetEvent(std::string type, std::string thrower = std::string());
	virtual ~ThrowAssetEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetAssetType(std::string type);
	const std::string& GetAssetType() const;
	void SetThrower(std::string thrower);
	const std::string& GetThrower() const;

private:
	std::string type;
	std::string thrower;
};

// 开火：玩家用一把武器(WeaponMod)开了一枪(霰弹每颗弹丸各一次)，通过瞄准/冷却/弹药闸门之后由UE层发出，
// 只发给这把武器自己的脚本(Weapon持有，没有脚本则不发)。触发条件里weapon_type可空=不限，留空匹配所有武器。
// 脚本里读$$local.xxx：
//   weapon(武器mod id)、shooter(开火市民姓名)、sufferer(被射线打中的市民姓名，没有则空)、
//   part(命中部位head/body/legs，没有则空)、distance(浮点，摄像机到hit的距离)；
//   vec3：asset(武器模型位置)、fire(枪口)、hit(射线交点，没打中是射程终点)、direction(出射单位方向)、
//   normal(命中面法线，没打中朝上)。每个vec3有字符串形式$$local.hit="(x, y, z)"和浮点分量$$local.hit.x/.y/.z。
class WeaponFireEvent : public Event {
public:
	WeaponFireEvent(std::string weapon);
	virtual ~WeaponFireEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetWeapon(std::string weapon);
	const std::string& GetWeapon() const;

	// 设置一个普通local字段(字符串/浮点)。
	void SetField(const std::string& name, ValueType value);
	// 设置一个vec3 local：同时写入name="(x, y, z)"和name.x/name.y/name.z三个浮点。
	void SetVec3(const std::string& name, double x, double y, double z);

private:
	std::string weapon;
	std::map<std::string, ValueType> fields;
};

// 弹射物命中：mod蓝图里的弹射物Actor(UForeverProjectileBridgeComponent)撞到东西后报告，发给发射它的武器脚本。
// 脚本里读$$local.hit/normal(vec3，规则同WeaponFireEvent)、$$local.sufferer(被击中的市民姓名，可空)。
class ProjectileHitEvent : public Event {
public:
	ProjectileHitEvent();
	virtual ~ProjectileHitEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	void SetField(const std::string& name, ValueType value);
	void SetVec3(const std::string& name, double x, double y, double z);

private:
	std::map<std::string, ValueType> fields;
};

// 市民受伤(单体伤害和范围伤害都发，不区分；致死的那一击只发CitizenDeceaseEvent)：发给主脚本和受伤市民自己的Scheduler脚本。
// 过滤字段name/attacker/weapon/part都可省略(留空=不限)；damage/health只在运行时事件里有，不能做过滤。
// $$local：name受伤者、attacker攻击者姓名(没有为空)、weapon、part、damage本次伤害、health剩余血量。
class CitizenInjuredEvent : public Event {
public:
	CitizenInjuredEvent(std::string name, std::string attacker = std::string(), std::string weapon = std::string(),
		std::string part = std::string(), double damage = 0.0, double health = 0.0);

	virtual ~CitizenInjuredEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	const std::string& GetName() const;
	const std::string& GetAttacker() const;
	const std::string& GetWeapon() const;
	const std::string& GetPart() const;
	double GetDamage() const;
	double GetHealth() const;

private:
	std::string name;
	std::string attacker;
	std::string weapon;
	std::string part;
	double damage;
	double health;
};

// 市民开始逃跑(citizen_flee真的生效时，已经在逃的只换目标不发)：只发给逃跑市民自己的Scheduler脚本。name=逃跑者，from=要远离的市民姓名；
// 过滤字段name/from可省略；destination只在运行时事件里有，不能做过滤。
// destination=开始逃跑那一刻"要回到的房间地址"：正在走路=走向的目的房间，没在走=当前所在房间，两者都没有=空串，
// 供脚本记下来、flee_end之后导航回去(公共脚本citizen_common就是这么用的)。
class FleeStartEvent : public Event {
public:
	FleeStartEvent(std::string name, std::string from = std::string(), std::string destination = std::string());

	virtual ~FleeStartEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	const std::string& GetName() const;
	const std::string& GetFrom() const;
	const std::string& GetDestination() const;

private:
	std::string name;
	std::string from;
	std::string destination;
};

// 市民结束逃跑：只发给逃跑市民自己的Scheduler脚本。$$local：name、from、reason——
// far=跑出了实例化距离阈值，lost=要远离的市民没了(被销毁/死亡/离城/不在场)，interrupted=被新的导航/任务/瞬移/死亡/玩家接管打断。
// 过滤字段name/from/reason都可省略(留空=不限)。
class FleeEndEvent : public Event {
public:
	FleeEndEvent(std::string name, std::string from = std::string(), std::string reason = std::string());

	virtual ~FleeEndEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	const std::string& GetName() const;
	const std::string& GetFrom() const;
	const std::string& GetReason() const;

private:
	std::string name;
	std::string from;
	std::string reason;
};

// 党代会开始/结束：只发给该党每个党员自己的Scheduler脚本(Core产出通知队列，UE层转发，见Core/polit/polit.md)。
// $$local：party(党派mod名)、address(会议室地址，没有会议室时为空串)、home(收到事件的这个党员的家的地址，UE层广播时填)。过滤字段两个都可省略(留空=不限)。
// start在党代会开始时发，end在结算完到场统计和党首选举之后发。
class MeetingStartEvent : public Event {
public:
	MeetingStartEvent(std::string party = std::string(), std::string address = std::string(), std::string home = std::string());

	virtual ~MeetingStartEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	const std::string& GetParty() const;
	const std::string& GetAddress() const;
	const std::string& GetHome() const;

private:
	std::string party;
	std::string address;
	std::string home; // 收到事件的这个党员的家(UE层广播时填，不参与Match)，脚本散会后用$$local.home回家
};

class MeetingEndEvent : public Event {
public:
	MeetingEndEvent(std::string party = std::string(), std::string address = std::string(), std::string home = std::string());

	virtual ~MeetingEndEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual bool GetLocalValue(const std::string& name, const ValueReceiver& receive) const override;

	const std::string& GetParty() const;
	const std::string& GetAddress() const;
	const std::string& GetHome() const;

private:
	std::string party;
	std::string address;
	std::string home;
};
