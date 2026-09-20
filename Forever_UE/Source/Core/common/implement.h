#pragma once

#include "class.h"

#include "common/handle.h"
#include "common/json.h"


// PostHandle第一个具体实现，复刻老工程Core/common/implement.h"顶层门面"的形状——聚合全部
// 7个domain，供Mod（这次是ScriptMod::WrapScript）通过Post()反向查询Core状态。不复刻老工程
// "GlobalBase持有的全局单例"这一层：这个项目里Map/Populace/Story本来就不是单例，
// PostImplement同样只是一个按需现场构造的普通对象，见implement.md。
//
// 这次实现了两种post类型："random citizen"（从Populace随机挑一个citizen姓名，供开局
// ChangeControlChange使用）和"game time"（Player全局时钟的当前日期/时间，见player.md）。
// society/industry/traffic这三个指针这次构造出来传进去，但Post()里还用不到，等对应域真正
// 迁移出业务逻辑、需要通过Post查询它们的数据时再加新的else if分支。
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
