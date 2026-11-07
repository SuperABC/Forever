#pragma once

#include "story/script_factory.h"

#include "story/script.h"

#include <vector>
#include <map>
#include <functional>

// 一个计时器：到target时刻(游戏时间)给ownerKey指向的那个Script发一次time_up，然后删除。
// ownerKey是ScriptSaveCodec::CollectScripts的稳定键(story.main/scheduler:<i>/vehicle:<名>/...)，
// 不存Script*：持有者可能被销毁/读档重建，到时再按键查，查不到就丢弃。
// realTime为true时是现实时间计时器：不看target，每帧由AdvanceRealTimers扣remaining(现实秒)，
// <=0到时。
struct StoryTimer {
	Time target;
	std::string name;
	std::string ownerKey;
	bool realTime = false;
	double remaining = 0.0;
};

// 剧情聚合：持有主线Script、系统/全局变量池和计时器，见story.md。
class Story {
	friend class ScriptSaveCodec;
public:
	// 绑定Registry里的ScriptFactory引用，mod注册不在这里做。
	Story();

	~Story();

	// 从config.json的main_story取ScriptMod，读取story_scripts里的全部主线脚本，见story.md。
	void Init();

	// 构造GameStartEvent广播给主线Script，用回调同步交付actions(ScriptContext.local指向栈上的事件，
	// 回调返回后失效，见story.md)。mainScript为空时不调用回调。
	void BroadcastGameStart(const std::function<void(const std::vector<ScriptAction>&, const ScriptContext&)>& onActions,
		PostHandle* post);

	// 返回是不是本域认识的类型(目前只认SetValueChange)，见story.md。
	bool ApplyChange(const Change* change, const ScriptContext& context);

	// 占位：保持"每个Core域都有Tick"的形状，目前没有每帧逻辑。
	void Tick(const Time& currentTime, bool crossedDay, PostHandle* post);

	Script* GetSystemScript() const;
	Script* GetGlobalScript() const;

	Script* GetMainScript() const;

	void CreateTimer(const std::string& name, const Time& target, const std::string& ownerKey);
	// 现实时间计时器：seconds现实秒之后到时，由AdvanceRealTimers(每帧传入帧间隔)推进。
	void CreateRealTimer(const std::string& name, double seconds, const std::string& ownerKey);
	void AdvanceRealTimers(double deltaSeconds);
	void RemoveTimer(const std::string& name, const std::string& ownerKey);
	// 取出所有target<=now的计时器(按到时先后)并从列表里删除。
	// maxCount：最多取出多少个，到时但没取完的留在队首，下一次接着取(分摊到多帧)。
	std::vector<StoryTimer> PopExpiredTimers(const Time& now, size_t maxCount = SIZE_MAX);

private:
	// 计时器。每个上班的岗位脚本都有两个，全城可以有几千个：游戏时间计时器放在按到时时刻排序的
	// multimap(红黑树)里，每帧只看队首，不扫全表；timerIndex按(名字, 归属键)索引，保证同owner同名
	// 只留一个，创建/删除都是O(log n)。现实时间计时器很少，另放一个vector。
	std::multimap<Time, StoryTimer> gameTimers;
	std::map<std::pair<std::string, std::string>, std::multimap<Time, StoryTimer>::iterator> timerIndex;
	std::vector<StoryTimer> realTimers;
	void EraseTimer(const std::string& name, const std::string& ownerKey);

public:
	// 存档用：全部计时器(游戏时间的按到时先后在前，现实时间的在后)；读档先清空再逐个还原。
	std::vector<StoryTimer> GetTimers() const;
	void ClearTimers();
	void RestoreTimer(const StoryTimer& timer);

private:
	// 引用Registry里的全局ScriptFactory，构造函数初始化列表里绑定。
	ScriptFactory& scriptFactory;

	// 主线剧情Script（持有所有权）；多个story_scripts文件的里程碑合并进这一个Script
	Script* mainScript = nullptr;

	// 全局变量池（持有所有权）
	Script* systemScript = nullptr; // 系统变量(system.)：脚本只读，只由C++写
	Script* globalScript = nullptr; // 全局变量(没有前缀)：脚本可读可写

};
