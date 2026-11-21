#pragma once

#include "class.h"

#include "polit/party_mod.h"
#include "polit/party_factory.h"

#include <string>
#include <utility>
#include <vector>


// 一个党派实例：独占一个PartyMod，加上Core维护的运行时状态(党员/党首/会议室/执政期数/权力/当月党代会进度)。
// 状态字段公开：只有Polit(选举/党代会)和PolitSaveCodec读写，设计见party.md。
class Party {
public:
	Party(PartyFactory* factory, PartyMod* mod, const std::string& id);
	~Party(); // 走工厂的deleter释放mod

	Party(const Party&) = delete;
	Party& operator=(const Party&) = delete;

	PartyMod* GetMod() const { return mod; }
	// 注册id(party_<name>)，日志/事件里的党派标识。
	const std::string& GetId() const { return id; }
	std::string GetName() const;

	bool HasMember(const Citizen* citizen) const;

	std::vector<Citizen*> members;
	Citizen* leader = nullptr;        // 党首：目前只记录/存档/写日志，不参与任何其它逻辑
	std::string meetingAddress;       // 会议室地址(空=没有会议室)
	Room* meetingRoom = nullptr;      // meetingAddress解析出的房间(读档时由地址重新解析)

	bool ruling = false;
	int terms = 0;                    // 连续执政期数(当前是执政党时>=1)
	std::vector<std::pair<std::string, Citizen*>> powers; // (权力id, 担任者)，只在执政期间有内容

	// 当月党代会进度。monthKey=year*12+month，和plan对应的月份；月份变了就重新取日程。
	struct Congress {
		int monthKey = 0;
		PartyCongress plan;
		bool started = false;
		bool ended = false;
		bool skipLogged = false;      // 执政党本月不开会，只写一次日志
	} congress;

private:
	PartyMod* mod;
	PartyFactory* factory;
	std::string id;
};
