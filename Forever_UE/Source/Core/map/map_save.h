#pragma once

#include "common/save_archive.h"

#include <string>


class Map;
class Node;
class Connection;
class Road;
class Lot;
class Floor;
class Room;

// 段名。地图拆成4段，各自带段版本(版本表见common/save.cpp的GetSaveSectionVersions)，改哪一段
// 的布局只升哪一段。
#define SAVE_SECTION_MAP_TERRAIN "world.map.terrain"
#define SAVE_SECTION_MAP_ROADNET "world.map.roadnet"
#define SAVE_SECTION_MAP_STRUCTURES "world.map.structures"
#define SAVE_SECTION_MAP_VEGETATION "world.map.vegetation"

// 地图域(地形格子/路网/导航图/Zone/Building/Room/Component/Door/植被)的存档读写。世界生成不可
// 复现(见SAVE_PLAN.md)，所以这里存的是生成结果本身：各类对象的字段原样写出，对象之间的指针
// 换成编号(Road/Lot/导航边按表下标，导航锚点按Node id，Zone/Building/Door按顺序下标)，读档
// 时按编号重建对象再连回指针，不调用任何mod的Layout/Distribute。需要直接读写私有字段，
// 所以相关类都把这个类声明成friend。详见map_save.md。
class MapSaveCodec {
public:
	// 把map写进archive的4个地图段。
	static void Save(const Map& map, SaveArchive& archive);

	// 按archive里的4个地图段重建一张Map。失败返回nullptr并给出原因(这时已经建出来的部分
	// 对象不保证释放干净——读档失败本来就要退回重新生成世界)。
	static Map* Load(const SaveArchive& archive, std::string& error);

	// 最近一次Save/Load的统计(各类对象数量、无法解析的引用数)。Core的debugf只走
	// OutputDebugString、不进UE日志，所以由UE层取出来打进Output Log。
	static const std::string& GetLastReport();

private:
	static std::string lastReport;

	struct SaveTables;
	struct LoadTables;

	static void WriteTerrain(const Map& map, ByteWriter& w);
	static bool ReadTerrain(Map& map, ByteReader& r, std::string& error);

	static void BuildSaveTables(const Map& map, SaveTables& t);
	static void WriteRoadnet(const Map& map, const SaveTables& t, ByteWriter& w);
	static bool ReadRoadnet(Map& map, LoadTables& t, ByteReader& r, std::string& error);

	static void WriteStructures(const Map& map, const SaveTables& t, ByteWriter& w);
	static bool ReadStructures(Map& map, LoadTables& t, ByteReader& r, bool restoreOptions, std::string& error);

	static void WriteVegetation(const Map& map, ByteWriter& w);
	static bool ReadVegetation(Map& map, ByteReader& r, std::string& error);

	static void WriteNode(const Node& node, ByteWriter& w);
	static Node ReadNode(ByteReader& r);
	static void WriteConnection(const Connection& c, ByteWriter& w);
	static Connection* ReadConnection(ByteReader& r);
	static void WriteRoad(const Road& road, ByteWriter& w);
	static Road* ReadRoad(ByteReader& r);
	static void WriteFloor(const Floor& floor, ByteWriter& w);
	static void ReadFloor(Floor& floor, ByteReader& r);
};
