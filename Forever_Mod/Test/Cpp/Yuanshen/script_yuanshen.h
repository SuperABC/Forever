#pragma once

#include <array>
#include <string>

#include "common/json.h"
#include "story/script_mod.h"

// YuanshenScript：把剧情脚本里label为"teleport_<拼音>"的PlaceHolderChange换成"把对应角色瞬移到
// yuanshen建筑的某个房间"，30个角色(顺序和ys.script开头的names数组一致，拼音和fuck_xxx变量一致)。
// yuanshen建筑全图只有一栋，先向Core查"list buildings"(type=yuanshen)拿到它的地址，再逐个角色找
// 占位符、拼上房间号得到房间地址。
//
// 房间分配(YuanshenBuilding::Layout的楼层结构)：1楼放前4个角色，房间号00001~00004；之后每层放2个，
// 房间号是"<楼层数字>0000"、"<楼层数字>0001"(1楼层数字是0，2楼是1……)，30个角色用到14层。
//
// 对应的JSON是{"type": "teleport_citizen", "name": ..., "destination": ...}——本项目里这个效果的
// Change类型叫npc_teleport，对应NPCTeleportChange(name/destination都是表达式，所以这里把姓名和地址
// 都包成带引号的字符串字面量)，由AForeverFrameworkActor::ApplyChange执行。
//
// 替换用的teleportChanges是mod自己长期持有的成员(固定大小的数组，永不重新分配、永不delete)，
// actionStack只存它们的指针，见script_mod.h"mod往actionStack里塞的指针必须是mod自己长期维护"的约定。
class YuanshenScript : public ScriptMod {
public:
	YuanshenScript() { name = std::string("yuanshen") + std::to_string(count++); }

	static const char* GetId() { return "script_yuanshen"; }
	virtual const char* GetType() const override { return "script_yuanshen"; }
	virtual const char* GetName() override { return name.c_str(); }

	virtual void WrapScript(const Event* event, const std::vector<ScriptAction>& actions,
		const ScriptContext& context, PostHandle* post) override {
		AutoCopy(actions);
		if (!post) return;

		// test.script的game_start里的"control"占位符：这个mod是主线脚本的mod，要接手empty脚本mod原来
		// 的工作——向Core要一个有家的随机市民交给玩家控制，否则玩家停在飞行相机上。
		int controlIdx = FindLabel("control", context);
		if (controlIdx >= 0) {
			JsonValue controlRequest(DATA_OBJECT);
			controlRequest["post"] = "random citizen";
			post->Post(controlRequest);
			const JsonValue& controlResult = post->GetResult();
			if (controlResult["result"].AsString() == "success") {
				controlChange.SetName(controlResult["name"].AsString());
				actionStack.back()[controlIdx] = static_cast<const Change*>(&controlChange);
			}
		}

		// 先找建筑：全图只有一栋yuanshen，找不到(这张地图没放它)就什么都不替换。
		JsonValue request(DATA_OBJECT);
		request["post"] = "list buildings";
		request["type"] = "building_yuanshen";
		post->Post(request);

		const JsonValue& result = post->GetResult();
		if (result["result"].AsString() != "success") return;
		const JsonValue& addresses = result["addresses"];
		if (!addresses.IsArray() || addresses.size() == 0) return;
		const std::string buildingAddress = addresses[0u].AsString();

		// 再逐个角色找占位符。
		for (size_t i = 0; i < kCharacters.size(); i++) {
			int idx = FindLabel(std::string("teleport_") + kCharacters[i].label, context);
			if (idx < 0) continue;

			NPCTeleportChange& change = teleportChanges[i];
			change.SetName(std::string("\"") + kCharacters[i].name + "\"");
			change.SetDestination("\"" + buildingAddress + " " + RoomNumber(static_cast<int>(i)) + "\"");
			actionStack.back()[idx] = static_cast<const Change*>(&change);
		}
	}

	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }

private:
	struct Character {
		const char* name;
		const char* label; // teleport_<label>
	};

	// 顺序和Test/Resource/ys.script开头的names数组、fuck_xxx变量一致。
	static inline const std::array<Character, 30> kCharacters = { {
		{ "莫娜", "mona" }, { "琴", "qin" }, { "优菈", "youla" }, { "迪卢克", "diluke" },
		{ "阿贝多", "abeiduo" }, { "凝光", "ningguang" }, { "北斗", "beidou" }, { "刻晴", "keqing" },
		{ "胡桃", "hutao" }, { "甘雨", "ganyu" }, { "夜兰", "yelan" }, { "达达利亚", "dadaliya" },
		{ "钟离", "zhongli" }, { "魈", "xiao" }, { "神里绫华", "shenlilinghua" }, { "宵宫", "xiaogong" },
		{ "雷电将军", "leidianjiangjun" }, { "八重神子", "bachongshenzi" }, { "千织", "qianzhi" },
		{ "枫原万叶", "fengyuanwanye" }, { "妮露", "nilu" }, { "莱依拉", "laiyila" }, { "散兵", "sanbing" },
		{ "艾尔海森", "aierhaisen" }, { "夏洛蒂", "xialuodi" }, { "芙宁娜", "funingna" },
		{ "娜维娅", "naweiya" }, { "克洛琳德", "keluolinde" }, { "纳维莱特", "naweilaite" },
		{ "莱欧斯利", "laiousili" },
	} };

	// 第i个角色(从0起)的房间号：前4个在1楼，00001~00004；之后每层2个，"<楼层数字>0000"/"<楼层数字>0001"
	// (2楼的楼层数字是1)，即Room::SetNumber的"楼层数字+4位序号"格式。
	static std::string RoomNumber(int i) {
		if (i < 4) return "0000" + std::to_string(i + 1);
		int j = i - 4;
		return std::to_string(1 + j / 2) + "000" + std::to_string(j % 2);
	}

	static inline int count = 0;
	std::string name;
	std::array<NPCTeleportChange, 30> teleportChanges;
	ChangeControlChange controlChange;
};
