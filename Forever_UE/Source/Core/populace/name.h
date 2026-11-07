#pragma once

#include "populace/name_mod.h"
#include "populace/name_factory.h"

#include <string>
#include <unordered_set>


// Name实体：持有一个NameMod实例(由NameFactory创建/销毁)并转发取名方法，见name.md。
class Name {
	// 存档：PopulaceSaveCodec直接读写私有字段(populace_save.md)。
	friend class PopulaceSaveCodec;
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

	// 占位一个姓名，GenerateName不会返回它(给剧情脚本name_reserve用)。
	void ReserveName(const std::string& name);

private:
	NameMod* mod;
	NameFactory* factory;
	std::string type;
	std::string name;

	std::unordered_set<std::string> reserve;
};
