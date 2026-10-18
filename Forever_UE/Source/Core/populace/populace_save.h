#pragma once

#include "common/save_archive.h"

#include <string>
#include <utility>
#include <vector>


class Map;
class Populace;
class Society;
class JobExperience;

#define SAVE_SECTION_POPULACE "world.populace"

// 人口域的存档读写：市民(姓名/性别/生日/性格/人际关系/经历/住处/当前所在房间/位置/生命值/
// Scheduler)、班级、Job/Scheduler两套定时器、取名器的占位名，以及市民和地图之间的归属关系
// (Zone/Building/Room的owner、Room的tenants/occupants)。和地图段一样存生成结果本身，读档时
// 不跑任何生成。详见populace_save.md。
class PopulaceSaveCodec {
public:
	// society用来给JobExperience里的Organization*编号(可以为nullptr，这时这些引用存成空)。
	static void Save(const Populace& populace, const Map& map, const Society* society, SaveArchive& archive);

	// 按存档重建Populace，并把归属关系写回map(map必须已经按同一份存档恢复)。存档里没有人口段
	// 时返回nullptr、error为空(调用方按新开局生成)；段损坏时返回nullptr并给出原因。
	// JobExperience里的Organization*要等社会段读完才能连上，先留空，(经历, 组织下标)列表放进
	// outPendingOrganizations交给SocietySaveCodec::Load。
	static Populace* Load(const SaveArchive& archive, Map& map,
		std::vector<std::pair<JobExperience*, int>>& outPendingOrganizations, std::string& error);
};
