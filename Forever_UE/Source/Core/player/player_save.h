#pragma once

#include "common/save_archive.h"

#include <string>


class Map;
class Player;
class Asset;
class AssetFactory;

#define SAVE_SECTION_PLAYER "world.player"

// 玩家域的存档读写：游戏时钟(含跨天判断用的day)、时间流速、生命值、5个身体槽位和房产里的
// 物品(整棵Asset树)、当前所在房间，以及散落在各个房间里的物品(Room::assets，物品属于玩家域)。
// 详见player_save.md。
class PlayerSaveCodec {
public:
	static void Save(const Player& player, const Map& map, SaveArchive& archive);

	// 按存档重建Player，并把房间里的物品放回map(map必须已经按同一份存档恢复)。存档里没有玩家段
	// 时返回nullptr、error为空；段损坏时返回nullptr并给出原因。
	static Player* Load(const SaveArchive& archive, Map& map, std::string& error);

private:
	static void WriteAsset(const Asset* asset, ByteWriter& w);
	// 读一棵Asset树；类型已经不可用(mod没了)的物品连同它装着的东西一起丢弃，返回nullptr。
	static Asset* ReadAsset(AssetFactory& factory, ByteReader& r);
};
