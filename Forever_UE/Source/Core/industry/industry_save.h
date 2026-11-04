#pragma once

#include "common/save_archive.h"

#include <string>


class Map;
class Industry;

#define SAVE_SECTION_INDUSTRY "world.industry"

// 工业域的存档读写：各product的城内批发价、每个仓库的库存、每个工坊两个cache的库存和已开工
// 尚未结算的批次。Storage/Manufacture的结构（有哪些、容量、需求）由Room和mod决定，不进存档，
// 读档时Industry::Init按map重新生成，再按名字把库存放回去。详见industry_save.md。
class IndustrySaveCodec {
public:
	static void Save(const Industry& industry, SaveArchive& archive);

	// 按存档重建Industry（map必须已经恢复）。没有工业段时返回nullptr、error为空；段损坏时返回nullptr
	// 并给出原因。
	static Industry* Load(const SaveArchive& archive, const Map& map, std::string& error);
};
