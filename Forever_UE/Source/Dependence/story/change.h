#pragma once

#include "../common/utility.h"
#include "../common/error.h"
#include "expression.h"

#include <string>


#undef GetMessage
#undef GetObject


// 变化基类：纯数据类，执行逻辑在各域的ApplyChange里按dynamic_cast分派，见change.md。
class Change {
public:
	Change();

	virtual ~Change();

	virtual const std::string& GetType() const = 0;

	const std::string& GetCondition() const;

	void SetCondition(const std::string& condition);

private:
	// 控制条件
	std::string condition;

};

// 范围循环
class ForRangeChange : public Change {
public:
	ForRangeChange(std::string var, std::string from, std::string to, std::string step,
		std::vector<const Change*> changes);

	virtual ~ForRangeChange();

	virtual const std::string& GetType() const override;

	std::string GetVar() const;

	const std::string& GetFrom() const;

	const std::string& GetTo() const;

	const std::string& GetStep() const;

	const std::vector<const Change*>& GetChanges() const;

private:
	// 循环变量名
	std::string var;

	// 起始表达式
	std::string from;

	// 终止表达式
	std::string to;

	// 步长表达式
	std::string step;

	// 循环体变化列表：本体归ForRangeChange自己持有，析构时delete
	std::vector<const Change*> changes;

};

// 占位符
class PlaceHolderChange : public Change {
public:
	PlaceHolderChange();

	PlaceHolderChange(std::string label);

	virtual ~PlaceHolderChange();

	virtual const std::string& GetType() const override;

	void SetLabel(std::string label);

	const std::string& GetLabel() const;

private:
	// 标签
	std::string label;

};

// 全局广播
class GlobalMessageChange : public Change {
public:
	GlobalMessageChange();

	GlobalMessageChange(std::string message);

	virtual ~GlobalMessageChange();

	virtual const std::string& GetType() const override;

	void SetMessage(std::string message);

	const std::string& GetMessage() const;

private:
	// 消息内容
	std::string message;

};

// 调试打印：只用于开发期调试输出，不是游戏内广播消息（和GlobalMessageChange同形状但语义不同，
// 所以单独成类型）。在Forever层执行，见change.md
class DebugPrintChange : public Change {
public:
	DebugPrintChange();

	DebugPrintChange(std::string message);

	virtual ~DebugPrintChange();

	virtual const std::string& GetType() const override;

	void SetMessage(std::string message);

	const std::string& GetMessage() const;

private:
	// 打印内容
	std::string message;

};

// 游戏结束
class GameEndChange : public Change {
public:
	GameEndChange();

	virtual ~GameEndChange();

	virtual const std::string& GetType() const override;

private:
};

// 变量赋值
class SetValueChange : public Change {
public:
	SetValueChange();

	SetValueChange(std::string variable, std::string value);

	virtual ~SetValueChange();

	virtual const std::string& GetType() const override;

	void SetVariable(std::string variable);

	std::string GetVariable() const;

	void SetValue(std::string value);

	const std::string& GetValue() const;

private:
	// 变量名
	std::string variable;

	// 值表达式
	std::string value;

};

// 全局设置修改
class GlobalSettingChange : public Change {
public:
	GlobalSettingChange();

	GlobalSettingChange(std::string setting, std::string value);

	virtual ~GlobalSettingChange();

	virtual const std::string& GetType() const override;

	void SetSetting(std::string setting);

	std::string GetSetting() const;

	void SetValue(std::string value);

	const std::string& GetValue() const;

private:
	// 设置名
	std::string setting;

	// 值表达式
	std::string value;

};

// 移除变量
class RemoveValueChange : public Change {
public:
	RemoveValueChange();

	RemoveValueChange(std::string variable);

	virtual ~RemoveValueChange();

	virtual const std::string& GetType() const override;

	void SetVariable(std::string variable);

	std::string GetVariable() const;

private:
	// 变量名
	std::string variable;

};

// 停用里程碑
class DeactivateMilestoneChange : public Change {
public:
	DeactivateMilestoneChange();

	DeactivateMilestoneChange(std::string milestone);

	virtual ~DeactivateMilestoneChange();

	virtual const std::string& GetType() const override;

	void SetMilestone(std::string milestone);

	std::string GetMilestone() const;

private:
	// 里程碑名称
	std::string milestone;

};

// 添加选项
class AddOptionChange : public Change {
public:
	AddOptionChange();

	AddOptionChange(std::string name, std::string option);

	virtual ~AddOptionChange();

	virtual const std::string& GetType() const override;

	void SetName(std::string name);

	const std::string& GetName() const;

	void SetOption(std::string option);

	const std::string& GetOption() const;

private:
	// 目标名称
	std::string name;

	// 选项文本
	std::string option;

};

// 移除选项
class RemoveOptionChange : public Change {
public:
	RemoveOptionChange();

	RemoveOptionChange(std::string name, std::string option);

	virtual ~RemoveOptionChange();

	virtual const std::string& GetType() const override;

	void SetName(std::string name);

	const std::string& GetName() const;

	void SetOption(std::string option);

	const std::string& GetOption() const;

private:
	// 目标名称
	std::string name;

	// 选项文本
	std::string option;

};

// 生成NPC
class SpawnNpcChange : public Change {
public:
	SpawnNpcChange();

	SpawnNpcChange(std::string avatar, std::string name, std::string gender, std::string birthday, std::string height, std::string weight,
		std::string nick, std::string deposit, std::string phone, std::string home, std::vector<std::string> jobs, std::string scheduler,
		std::string economy = std::string());

	virtual ~SpawnNpcChange();

	virtual const std::string& GetType() const override;

	void SetAvatar(std::string avatar);

	const std::string& GetAvatar() const;

	void SetName(std::string name);

	const std::string& GetName() const;

	void SetGender(std::string gender);

	const std::string& GetGender() const;

	void SetBirthday(std::string birthday);

	const std::string& GetBirthday() const;

	void SetHeight(std::string height);

	const std::string& GetHeight() const;

	void SetWeight(std::string weight);

	const std::string& GetWeight() const;

	void SetNick(std::string nick);

	const std::string& GetNick() const;

	void SetDeposit(std::string deposit);

	const std::string& GetDeposit() const;

	void SetPhone(std::string phone);

	const std::string& GetPhone() const;

	void SetHome(std::string home);

	const std::string& GetHome() const;

	void SetJobs(std::vector<std::string> jobs);

	const std::vector<std::string>& GetJobs() const;

	void SetScheduler(std::string scheduler);

	const std::string& GetScheduler() const;

	void SetEconomy(std::string economy);
	const std::string& GetEconomy() const;

private:
	// 形象标识
	std::string avatar;

	// 姓名
	std::string name;

	// 性别
	std::string gender;

	// 生日
	std::string birthday;

	// 身高
	std::string height;

	// 体重
	std::string weight;

	// 昵称
	std::string nick;

	// 存款金额
	std::string deposit;

	// 手机号
	std::string phone;

	// 住所
	std::string home;

	// 职业类型标识列表
	std::vector<std::string> jobs;

	// 调度类型标识
	std::string scheduler;

	// 是否接入经济系统的布尔表达式
	std::string economy;

};

// 移除NPC
class RemoveNpcChange : public Change {
public:
	RemoveNpcChange();

	RemoveNpcChange(std::string name);

	virtual ~RemoveNpcChange();

	virtual const std::string& GetType() const override;

	void SetName(std::string name);

	const std::string& GetName() const;

private:
	// NPC姓名
	std::string name;

};

// 复活市民：isDead清除、血量恢复。复活后没有房间也没有位置，需要后续npc_teleport把他放到某个房间。
class NPCReviveChange : public Change {
public:
	NPCReviveChange();

	NPCReviveChange(std::string name, std::string health);

	virtual ~NPCReviveChange();

	virtual const std::string& GetType() const override;

	void SetName(std::string name);
	const std::string& GetName() const;

	void SetHealth(std::string health);
	const std::string& GetHealth() const;

private:
	std::string name;
	std::string health;
};

// 瞬移市民
class NPCTeleportChange : public Change {
public:
	NPCTeleportChange();

	NPCTeleportChange(std::string name, std::string destination);

	virtual ~NPCTeleportChange();

	virtual const std::string& GetType() const override;

	void SetName(std::string name);

	const std::string& GetName() const;

	void SetDestination(std::string destination);

	const std::string& GetDestination() const;

private:
	// 市民姓名
	std::string name;

	// 目标房间名称
	std::string destination;

};

// NPC自动导航。mod（Basic的Job）构造时传的是已算好的姓名/房间地址，脚本里的npc_navigate写的是表达式，
// 执行端两种都兼容，见change.md
class NPCNavigateChange : public Change {
public:
	NPCNavigateChange();

	NPCNavigateChange(std::string name, std::string destination);

	virtual ~NPCNavigateChange();

	virtual const std::string& GetType() const override;

	void SetName(std::string name);

	const std::string& GetName() const;

	void SetDestination(std::string destination);

	const std::string& GetDestination() const;

private:
	// NPC姓名
	std::string name;

	// 目标位置名称
	std::string destination;

};

// 瞬移角色
class TeleportPlayerChange : public Change {
public:
	TeleportPlayerChange();

	TeleportPlayerChange(std::string destination);

	virtual ~TeleportPlayerChange();

	virtual const std::string& GetType() const override;

	void SetDestination(std::string destination);

	const std::string& GetDestination() const;

private:
	// 目标房间名称
	std::string destination;

};

// 打开商店
class OpenShopChange : public Change {
public:
	OpenShopChange();

	OpenShopChange(std::string saler);

	virtual ~OpenShopChange();

	virtual const std::string& GetType() const override;

	void SetSaler(std::string saler);

	const std::string& GetSaler() const;

private:
	// 售货员姓名
	std::string saler;

};

// 启动小游戏
class StartPuzzleChange : public Change {
public:
	StartPuzzleChange();

	StartPuzzleChange(std::string puzzle);

	StartPuzzleChange(std::string puzzle, std::string param);

	virtual ~StartPuzzleChange();

	virtual const std::string& GetType() const override;

	void SetPuzzle(std::string puzzle);

	const std::string& GetPuzzle() const;

	void SetParam(std::string param);

	const std::string& GetParam() const;

private:
	// 小游戏类型标识
	std::string puzzle;

	// 剧情脚本传给小游戏的启动参数(表达式)
	std::string param;

};

// 给被玩家操控的市民发工资：由执行它的这个Job脚本所属岗位的组织支付(Flow::WAGE，代扣所得税)。
class PayWageChange : public Change {
public:
	PayWageChange();

	PayWageChange(std::string amount);
	virtual ~PayWageChange();

	virtual const std::string& GetType() const override;

	void SetAmount(std::string amount);
	const std::string& GetAmount() const;

private:
	// 工资金额(分)的表达式
	std::string amount;
};

// 进入载具
class EnterVehicleChange : public Change {
public:
	EnterVehicleChange();

	EnterVehicleChange(std::string vehicle);

	virtual ~EnterVehicleChange();

	virtual const std::string& GetType() const override;

	void SetVehicle(std::string vehicle);

	const std::string& GetVehicle() const;

private:
	// 载具名称
	std::string vehicle;

};

// 车辆导航测试：显示指定载具当前行驶路线的折线（不给载具发任何命令）。UE-only效果，
// 和TestNavigateCitizenChange对称，见UForeverTrafficFrameworkComponent::StartTestNavigation。
class TestNavigateVehicleChange : public Change {
public:
	TestNavigateVehicleChange();

	TestNavigateVehicleChange(std::string name);

	virtual ~TestNavigateVehicleChange();

	virtual const std::string& GetType() const override;

	void SetName(std::string name);
	const std::string& GetName() const;

private:
	// 载具名称
	std::string name;

};

// 给载具的Controller发运行指令（UE-only效果，见UForeverTrafficFrameworkComponent::StartBusTestRun/StopBusRun）：
//   command == "run"：随机选一条公交环线的随机站，先导航过去再开始循环；
//   command == "stop"：刹停并保持，线路公交同时退出时刻表。
class VehicleCommandChange : public Change {
public:
	VehicleCommandChange();

	VehicleCommandChange(std::string vehicle, std::string command);

	virtual ~VehicleCommandChange();

	virtual const std::string& GetType() const override;

	void SetVehicle(std::string vehicle);
	const std::string& GetVehicle() const;

	void SetCommand(std::string command);
	const std::string& GetCommand() const;

private:
	std::string vehicle;
	std::string command;

};

// 市民导航测试：让指定市民由AIController带着沿行人导航图走到测试目的地（机场一楼房间）。
// UE-only效果，见UForeverPopulaceFrameworkComponent::StartTestNavigation。
class TestNavigateCitizenChange : public Change {
public:
	TestNavigateCitizenChange();

	TestNavigateCitizenChange(std::string name);

	virtual ~TestNavigateCitizenChange();

	virtual const std::string& GetType() const override;

	void SetName(std::string name);
	const std::string& GetName() const;

private:
	// 市民姓名
	std::string name;

};

// 离开载具
class LeaveVehicleChange : public Change {
public:
	LeaveVehicleChange();

	LeaveVehicleChange(std::string vehicle);

	virtual ~LeaveVehicleChange();

	virtual const std::string& GetType() const override;

	void SetVehicle(std::string vehicle);

	const std::string& GetVehicle() const;

private:
	// 载具名称
	std::string vehicle;

};

// 搭乘载具(公交)：和EnterVehicleChange(驾驶)区别：搭乘只是坐进去，不能操控，载具自己运行，
// 视角可以用鼠标转。UE层执行(Possess载具Pawn)，见ForeverTrafficFrameworkComponent::ApplyBoardVehicle。
class BoardVehicleChange : public Change {
public:
	BoardVehicleChange();
	BoardVehicleChange(std::string vehicle);
	virtual ~BoardVehicleChange();

	virtual const std::string& GetType() const override;

	void SetVehicle(std::string vehicle);
	const std::string& GetVehicle() const;

private:
	// 载具名称(表达式)
	std::string vehicle;
};

// 强制玩家下车(比如公交收班)：驾驶和搭乘都生效，不检查下车点是否被挡、不看是否在站。
// vehicle留空=玩家当前所在的任何载具；非空=只在玩家正在这辆载具里时才执行。
class ForceLeaveChange : public Change {
public:
	ForceLeaveChange();
	ForceLeaveChange(std::string vehicle);
	virtual ~ForceLeaveChange();

	virtual const std::string& GetType() const override;

	void SetVehicle(std::string vehicle);
	const std::string& GetVehicle() const;

private:
	// 载具名称(表达式，可空)
	std::string vehicle;
};

// 设置门禁(常开/常锁)
class SetDoorChange : public Change {
public:
	SetDoorChange();

	SetDoorChange(std::string name, std::string access);

	virtual ~SetDoorChange();

	virtual const std::string& GetType() const override;

	void SetName(std::string name);

	const std::string& GetName() const;

	void SetAccess(std::string access);

	const std::string& GetAccess() const;

private:
	// 目标名称
	std::string name;

	// 门禁(布尔表达式)
	std::string access;

};

// 记录对象：按类别+mod type在地图上找一个对象，把它的地址写进变量，让脚本能拼出房间地址等
class RecordObjectChange : public Change {
public:
	RecordObjectChange();

	RecordObjectChange(std::string object, std::string kind, std::string policy, std::string variable);

	virtual ~RecordObjectChange();

	virtual const std::string& GetType() const override;

	void SetObject(std::string object);
	const std::string& GetObject() const;
	void SetKind(std::string kind);
	const std::string& GetKind() const;
	void SetPolicy(std::string policy);
	const std::string& GetPolicy() const;
	void SetVariable(std::string variable);
	const std::string& GetVariable() const;

private:
	// 对象类别(表达式)
	std::string object;

	// mod type(表达式)
	std::string kind;

	// 选取策略(表达式)
	std::string policy;

	// 结果变量名
	std::string variable;

};

// 按人登记门禁例外：允许或不允许某个市民通过，优先于门本身的常开/常锁/仅主人
class RegisterDoorChange : public Change {
public:
	RegisterDoorChange();

	RegisterDoorChange(std::string name, std::string citizen, std::string pass);

	virtual ~RegisterDoorChange();

	virtual const std::string& GetType() const override;

	void SetName(std::string name);

	const std::string& GetName() const;

	void SetCitizen(std::string citizen);

	const std::string& GetCitizen() const;

	void SetPass(std::string pass);

	const std::string& GetPass() const;

private:
	// 目标名称
	std::string name;

	// 市民名称
	std::string citizen;

	// 是否允许通过(布尔表达式)
	std::string pass;

};

// 跳过游戏时间：把游戏时钟快进到目标时刻，期间的计时器/经济/事件都照常发生。time和mode的含义同
// CreateTimerChange(absolute/relative/next，不支持real)；目标不晚于现在时什么都不做。
class JumpTimeChange : public Change {
public:
	JumpTimeChange();

	JumpTimeChange(std::string time, std::string mode = std::string());

	virtual ~JumpTimeChange();

	virtual const std::string& GetType() const override;

	void SetTime(std::string time);
	const std::string& GetTime() const;

	void SetMode(std::string mode);
	const std::string& GetMode() const;

private:
	// 目标时刻(表达式)
	std::string time;

	// time的解释方式(表达式)，空=absolute
	std::string mode;
};

// 创建计时器
class CreateTimerChange : public Change {
public:
	CreateTimerChange();

	CreateTimerChange(std::string name, std::string time, std::string category, std::string label,
		std::string mode = std::string());

	virtual ~CreateTimerChange();

	virtual const std::string& GetType() const override;

	void SetName(std::string name);

	const std::string& GetName() const;

	void SetTime(std::string time);

	const std::string& GetTime() const;

	void SetCategory(std::string category);

	const std::string& GetCategory() const;

	void SetLabel(std::string label);

	const std::string& GetLabel() const;

	void SetMode(std::string mode);
	const std::string& GetMode() const;

private:
	// 计时器名称
	std::string name;

	// 目标时刻
	std::string time;

	// time的解释方式
	std::string mode;

	// 所属脚本类型
	std::string category;

	// 所属实体名称
	std::string label;

};

// 移除计时器
class RemoveTimerChange : public Change {
public:
	RemoveTimerChange();

	RemoveTimerChange(std::string name);

	virtual ~RemoveTimerChange();

	virtual const std::string& GetType() const override;

	void SetName(std::string name);

	const std::string& GetName() const;

private:
	// 计时器名称
	std::string name;

};

// 启动电梯
class LaunchElevatorChange : public Change {
public:
	LaunchElevatorChange();

	LaunchElevatorChange(std::string building, std::string elevator, std::string command);

	virtual ~LaunchElevatorChange();

	virtual const std::string& GetType() const override;

	void SetBuilding(std::string building);

	const std::string& GetBuilding() const;

	void SetElevator(std::string elevator);

	const std::string& GetElevator() const;

	void SetCommand(std::string command);

	const std::string& GetCommand() const;

private:
	// 建筑名称
	std::string building;

	// 电梯名称
	std::string elevator;

	// 指令
	std::string command;

};

// 播放视频
class PlayVideoChange : public Change {
public:
	PlayVideoChange();

	PlayVideoChange(std::string path);

	virtual ~PlayVideoChange();

	virtual const std::string& GetType() const override;

	void SetPath(std::string path);

	const std::string& GetPath() const;

private:
	// 视频文件路径
	std::string path;

};

// 播放背景音乐
class PlayBgmChange : public Change {
public:
	PlayBgmChange();

	PlayBgmChange(std::string bgm, std::string loop);

	virtual ~PlayBgmChange();

	virtual const std::string& GetType() const override;

	void SetBgm(std::string bgm);

	const std::string& GetBgm() const;

	void SetLoop(std::string loop);

	const std::string& GetLoop() const;

private:
	// 背景音乐标识
	std::string bgm;

	// 是否循环播放
	std::string loop;

};

// 停止背景音乐
class StopBgmChange : public Change {
public:
	StopBgmChange();

	virtual ~StopBgmChange();

	virtual const std::string& GetType() const override;

private:
};

// 存款收支
class BankTransactionChange : public Change {
public:
	BankTransactionChange();

	BankTransactionChange(std::string name, std::string amount);

	virtual ~BankTransactionChange();

	virtual const std::string& GetType() const override;

	void SetName(std::string name);

	const std::string& GetName() const;

	void SetAmount(std::string amount);

	const std::string& GetAmount() const;

private:
	// 收款人姓名（空字符串表示玩家）
	std::string name;

	// 金额
	std::string amount;

};

// 给予房产
class GiveEstateChange : public Change {
public:
	GiveEstateChange();

	GiveEstateChange(std::string estate, std::string name, std::string force);

	virtual ~GiveEstateChange();

	virtual const std::string& GetType() const override;

	void SetEstate(std::string estate);

	const std::string& GetEstate() const;

	void SetName(std::string name);

	const std::string& GetName() const;

	void SetForce(std::string force);

	const std::string& GetForce() const;

private:
	// 房产名称
	std::string estate;

	// 接收者姓名（空字符串表示玩家）
	std::string name;

	// 是否强制覆盖已有归属
	std::string force;

};

// 移除房产
class RemoveEstateChange : public Change {
public:
	RemoveEstateChange();

	RemoveEstateChange(std::string estate, std::string name);

	virtual ~RemoveEstateChange();

	virtual const std::string& GetType() const override;

	void SetEstate(std::string estate);

	const std::string& GetEstate() const;

	void SetName(std::string name);

	const std::string& GetName() const;

private:
	// 房产名称
	std::string estate;

	// 当前所有者姓名（空字符串表示玩家）
	std::string name;

};

// 给予载具
class GiveVehicleChange : public Change {
public:
	GiveVehicleChange();

	GiveVehicleChange(std::string vehicle, std::string name, std::string force);

	virtual ~GiveVehicleChange();

	virtual const std::string& GetType() const override;

	void SetVehicle(std::string vehicle);

	const std::string& GetVehicle() const;

	void SetName(std::string name);

	const std::string& GetName() const;

	void SetForce(std::string force);

	const std::string& GetForce() const;

private:
	// 载具名称
	std::string vehicle;

	// 接收者姓名（空字符串表示玩家）
	std::string name;

	// 是否强制覆盖已有归属
	std::string force;

};

// 移除载具
class RemoveVehicleChange : public Change {
public:
	RemoveVehicleChange();

	RemoveVehicleChange(std::string vehicle, std::string name);

	virtual ~RemoveVehicleChange();

	virtual const std::string& GetType() const override;

	void SetVehicle(std::string vehicle);

	const std::string& GetVehicle() const;

	void SetName(std::string name);

	const std::string& GetName() const;

private:
	// 载具名称
	std::string vehicle;

	// 当前所有者姓名（空字符串表示玩家）
	std::string name;

};

// 受伤
class PlayerInjuredChange : public Change {
public:
	PlayerInjuredChange();

	PlayerInjuredChange(std::string wound);

	virtual ~PlayerInjuredChange();

	virtual const std::string& GetType() const override;

	void SetWound(std::string wound);

	const std::string& GetWound() const;

private:
	// 伤势描述
	std::string wound;

};

// 痊愈
class PlayerCuredChange : public Change {
public:
	PlayerCuredChange();

	PlayerCuredChange(std::string wound);

	virtual ~PlayerCuredChange();

	virtual const std::string& GetType() const override;

	void SetWound(std::string wound);

	const std::string& GetWound() const;

private:
	// 痊愈的伤势
	std::string wound;

};

// 生病
class PlayerIllChange : public Change {
public:
	PlayerIllChange();

	PlayerIllChange(std::string illness);

	virtual ~PlayerIllChange();

	virtual const std::string& GetType() const override;

	void SetIllness(std::string illness);

	const std::string& GetIllness() const;

private:
	// 病症描述
	std::string illness;

};

// 康复
class PlayerRecoverChange : public Change {
public:
	PlayerRecoverChange();

	PlayerRecoverChange(std::string illness);

	virtual ~PlayerRecoverChange();

	virtual const std::string& GetType() const override;

	void SetIllness(std::string illness);

	const std::string& GetIllness() const;

private:
	// 康复的病症
	std::string illness;

};

// 强制睡眠
class PlayerSleepChange : public Change {
public:
	PlayerSleepChange();

	PlayerSleepChange(std::string hour);

	virtual ~PlayerSleepChange();

	virtual const std::string& GetType() const override;

	void SetHour(std::string hour);

	const std::string& GetHour() const;

private:
	// 睡眠时长（小时）
	std::string hour;

};

// 变化时间
class ChangeTimeChange : public Change {
public:
	ChangeTimeChange();

	ChangeTimeChange(std::string delta);

	virtual ~ChangeTimeChange();

	virtual const std::string& GetType() const override;

	void SetDelta(std::string delta);

	const std::string& GetDelta() const;

private:
	// 时间偏移量表达式，求值后交给Time解析
	std::string delta;

};

// 变化通缉
class ChangeWantedChange : public Change {
public:
	ChangeWantedChange();

	ChangeWantedChange(std::string reason, std::string level);

	virtual ~ChangeWantedChange();

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

// 变化天气
class ChangeWeatherChange : public Change {
public:
	ChangeWeatherChange();

	ChangeWeatherChange(std::string weather);

	virtual ~ChangeWeatherChange();

	virtual const std::string& GetType() const override;

	void SetWeather(std::string weather);

	const std::string& GetWeather() const;

private:
	// 天气类型
	std::string weather;

};

// 切换控制：把玩家的操控权切换到指定姓名的市民身上，只能在Forever层执行，见change.md
class ChangeControlChange : public Change {
public:
	ChangeControlChange();

	ChangeControlChange(std::string name);

	virtual ~ChangeControlChange();

	virtual const std::string& GetType() const override;

	void SetName(std::string name);

	const std::string& GetName() const;

private:
	// 市民姓名
	std::string name;

};

// 捡起地上的资产(放进玩家身上)：顺序是①没背包且它可背就背上、②有背包且放得下就放包里、③否则放右手/左手、
// ④都不行失败。堆叠资产整体考虑，不会只捡一部分。通常写在资产自己的脚本里响应“捡起”选项。结果通过
// get_asset事件发回执行这个Change的脚本。
class PickAssetChange : public Change {
public:
	PickAssetChange();
	PickAssetChange(std::string asset);
	virtual ~PickAssetChange();

	virtual const std::string& GetType() const override;

	void SetAsset(std::string asset);
	const std::string& GetAsset() const;

private:
	std::string asset;
};

// 凭空给玩家一个或一组资产，放置顺序同PickAssetChange(没有场景里的Actor需要删除)。结果通过get_asset事件
// 发回执行这个Change的脚本。
class GrantAssetChange : public Change {
public:
	GrantAssetChange();
	GrantAssetChange(std::string type, std::string count);
	virtual ~GrantAssetChange();

	virtual const std::string& GetType() const override;

	void SetAssetType(std::string type);
	const std::string& GetAssetType() const;
	void SetCount(std::string count);
	const std::string& GetCount() const;

private:
	std::string type;
	std::string count;
};

// 扣除玩家一个或一组资产(递归左右手和背包)：身上够数才扣，否则整体失败。成功对每件被扣的资产、失败对
// 要求的类型发lose_asset事件给执行这个Change的脚本。
class DeductAssetChange : public Change {
public:
	DeductAssetChange();
	DeductAssetChange(std::string type, std::string count);
	virtual ~DeductAssetChange();

	virtual const std::string& GetType() const override;

	void SetAssetType(std::string type);
	const std::string& GetAssetType() const;
	void SetCount(std::string count);
	const std::string& GetCount() const;

private:
	std::string type;
	std::string count;
};

// 强制玩家把身上的资产丢到附近地面：数量判定同DeductAssetChange，不够整体失败。lose_asset事件同上。
class DropAssetChange : public Change {
public:
	DropAssetChange();
	DropAssetChange(std::string type, std::string count);
	virtual ~DropAssetChange();

	virtual const std::string& GetType() const override;

	void SetAssetType(std::string type);
	const std::string& GetAssetType() const;
	void SetCount(std::string count);
	const std::string& GetCount() const;

private:
	std::string type;
	std::string count;
};
