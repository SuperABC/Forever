#pragma once

#include "class.h"

#include "common/handle.h"
#include "common/json.h"


// PostHandle的具体实现——聚合全部7个domain指针，供Mod（如ScriptMod::WrapScript）通过
// Post()反向查询Core状态。不做成全局单例：这个项目里Map/Populace/Story本来就不是单例，
// PostImplement同样只是一个按需现场构造的普通对象，见implement.md。
//
// 目前实现四种post类型："random citizen"（从Populace随机挑一个已分到住处的citizen姓名）、
// "game time"（Player全局时钟的当前日期/时间，见player.md）、"citizen home address"/
// "citizen workplace address"（按姓名查citizen家/工位地址）。society/industry/traffic
// 这三个指针目前构造出来传进去，但Post()里还用不到，等对应域迁移出需要被查询的数据时
// 再加新的else if分支。
class PostImplement : public PostHandle {
public:
	PostImplement(Map* map, Populace* populace, Society* society, Story* story,
		Industry* industry, Traffic* traffic, Player* player);

	virtual void Post(const JsonValue& request) override;
	virtual const JsonValue& GetResult() const override;

private:
	Map* map;
	Populace* populace;
	Society* society;
	Story* story;
	Industry* industry;
	Traffic* traffic;
	Player* player;

	// Post()的结果缓存，GetResult()返回这个的引用——结果对象的生命周期留在这一侧
	// （PostImplement实例）管理，调用方（可能是mod DLL）只读引用，不持有、不释放，
	// 见common/handle.md。
	JsonValue result;
};
