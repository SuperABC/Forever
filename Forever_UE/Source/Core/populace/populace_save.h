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

// 人口域的存档读写，见populace_save.md。
class PopulaceSaveCodec {
public:
	// society用来给JobExperience里的Organization*编号(可以为nullptr，这时这些引用存成空)。
	static void Save(const Populace& populace, const Map& map, const Society* society, SaveArchive& archive);

	// 按存档重建Populace，并把归属关系写回map(map必须已经按同一份存档恢复)。
	// 没有人口段或段损坏时返回nullptr并给出error。
	// JobExperience里的Organization*要等社会段读完才能连上，先留空，(经历, 组织下标)列表放进
	// outPendingOrganizations交给SocietySaveCodec::Load。
	static Populace* Load(const SaveArchive& archive, Map& map,
		std::vector<std::pair<JobExperience*, int>>& outPendingOrganizations, std::string& error);
};
