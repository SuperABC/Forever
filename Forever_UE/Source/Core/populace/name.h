#pragma once

#include "populace/name_mod.h"
#include "populace/name_factory.h"

#include <string>
#include <unordered_set>


// Name实体：持有一个具体NameMod实例(由NameFactory创建/销毁)，把它的取名业务方法转发出来。
// 架构上和Terrain(Core/map/terrain.h)同一个模式——Populace只允许操作这个Core层concept，
// 不允许直接持有/调用NameMod*，详见populace.md。
class Name {
public:
	Name() = delete;

	// @factory: 姓名工厂; @nameId: 姓名生成算法静态类型标识(工厂里已注册的id)
	Name(NameFactory* factory, const std::string& nameId);
	~Name();

	std::string GetType() const;
	std::string GetName() const;

	// 持有的mod实例——以后需要读mod内部数据的调用方直接用，不需要另外拷贝。
	NameMod* GetMod() const;

	// 从一个完整姓名里截取"姓"（用于新生儿继承父亲的姓）
	std::string GetSurname(const std::string& fullName) const;

	// 全随机生成一个姓名
	std::string GenerateName(bool allowMale, bool allowFemale, bool allowNeutral) const;

	// 给定一个姓，只生成名
	std::string GenerateName(const std::string& surname,
		bool allowMale, bool allowFemale, bool allowNeutral) const;

	// 占位一个姓名——GenerateName不会返回这个名字（内部撞上就重试，见.cpp）。给主线剧情
	// 脚本里出现的角色名占位用（name_reserve字段），见populace.md"InitNames"一节。
	void ReserveName(const std::string& name);

private:
	NameMod* mod;
	NameFactory* factory;
	std::string type;
	std::string name;

	std::unordered_set<std::string> reserve;
};
