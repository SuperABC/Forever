#pragma once

#include "common/save_archive.h"

#include <string>
#include <utility>
#include <vector>


class Map;
class Populace;
class Society;
class JobExperience;

#define SAVE_SECTION_SOCIETY "world.society"

// 社会域的存档读写，见society_save.md。
class SocietySaveCodec {
public:
	static void Save(const Society& society, const Map& map, const Populace& populace, SaveArchive& archive);

	// 按存档重建Society，并把市民的Job连回去、给人口段留下的JobExperience连上组织
	// (pendingOrganizations见PopulaceSaveCodec::Load)。map/populace必须已经按同一份存档恢复。
	// 没有社会段或段损坏时返回nullptr并给出error。
	static Society* Load(const SaveArchive& archive, Map& map, Populace& populace,
		const std::vector<std::pair<JobExperience*, int>>& pendingOrganizations, std::string& error);
};
