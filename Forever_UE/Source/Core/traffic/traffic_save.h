#pragma once

#include "common/save_archive.h"

#include <string>


class Map;
class Traffic;

#define SAVE_SECTION_TRAFFIC "world.traffic"

// 交通域的存档读写：站点、线路、车辆和公交计时；详见traffic_save.md。
class TrafficSaveCodec {
public:
	static void Save(const Traffic& traffic, const Map& map, SaveArchive& archive);

	// 按存档重建Traffic(map必须已经按同一份存档恢复)，包括车辆的交互选项。
	// 缺交通段或段损坏时返回nullptr并给出原因。
	static Traffic* Load(const SaveArchive& archive, Map& map, std::string& error);
};
