#pragma once

#include "class.h"

#include "common/save_archive.h"
#include "polit/party_factory.h"

#include <string>


#define SAVE_SECTION_POLIT "world.polit"

// 政治域的存档读写，见polit_save.md。
class PolitSaveCodec {
public:
	static void Save(const Polit& polit, const Populace& populace, SaveArchive& archive);

	// 按存档重建Polit：政策值、各党(mod按id重建+状态)、党员/党首/会议室/执政期数/党代会进度/权力。
	// map/populace/society必须已按同一份存档恢复；成功时已经Bind好。
	// 没有该段或段损坏时返回nullptr并给出error。
	static Polit* Load(const SaveArchive& archive, Map& map, Populace& populace, Society& society, std::string& error);
};
