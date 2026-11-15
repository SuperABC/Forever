#pragma once

#include "common/save_archive.h"

#include <string>


class Map;
class Traffic;
class Populace;
class Society;

#define SAVE_SECTION_TRAFFIC "world.traffic"

// 交通域的存档读写：站点、线路、车辆和公交计时；详见traffic_save.md。
class TrafficSaveCodec {
public:
	// populace/society用来把载具资产的所有者写成下标(市民/组织)，可为空(没有对应段就不写所有者)。
	static void Save(const Traffic& traffic, const Map& map, const Populace* populace, const Society* society, SaveArchive& archive);

	// 按存档重建Traffic(map必须已经按同一份存档恢复)，包括车辆的交互选项。
	// 缺交通段或段损坏时返回nullptr并给出原因。
	static Traffic* Load(const SaveArchive& archive, Map& map, std::string& error);
};
