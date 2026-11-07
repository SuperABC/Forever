#pragma once

#include "common/save_archive.h"

#include <string>


class Map;
class Industry;

#define SAVE_SECTION_INDUSTRY "world.industry"

// 工业域的存档读写，见industry_save.md。
class IndustrySaveCodec {
public:
	static void Save(const Industry& industry, SaveArchive& archive);

	// 按存档重建Industry（map必须已经恢复）。没有工业段或段损坏时返回nullptr并给出error。
	static Industry* Load(const SaveArchive& archive, const Map& map, std::string& error);
};
