#pragma once

#include <string>
#include <vector>


// 编辑器(MapEditor"线路"工具)手工设计的独立线路：实体线路(节点+连线)和线路环。
// roadnet mod(roadnet_file)从.roadnet的output.transit读出来放进RoadnetMod::transit，Core原样拷贝一份，
// route mod在`--skip`时通过RouteMod::SetManualTransit拿到它的只读指针，用它代替自己的自动布局，见route_mod.md"手动线路"。
// 全是值类型。注意：route mod只读，不要往里写(跨DLL堆)。
struct TransitData {
	struct Node {
		float x = 0.f, y = 0.f, z = 0.f; // 地图单位(element)，z=该节点高度
		bool isExtern = false;           // 城外连接点(在地图边界上，线路从这里出城/回城)，route mod目前只当普通节点用
	};
	// 一条实体线路：from->to，二次Bezier(hasCtrl=false是直线)；高度由两端节点z决定(扁S)。
	// kind：ground平地 / hug贴合地形 / mouth隧道口 / tunnel隧道 / bridge桥。route=所属route mod id。
	struct Conn {
		int from = -1, to = -1;
		bool hasCtrl = false;
		float cx = 0.f, cy = 0.f;
		std::string kind = "ground";
		std::string route;
	};
	// 线路环上的一项：preset>=0是预设建筑(下标=roadnet output.presets的原下标，stop=是否停靠)，否则node>=0是线路节点(只表示"经过")。
	struct Item {
		int preset = -1;
		bool stop = false;
		int node = -1;
	};
	// 一个闭环：首尾相接。
	struct Loop {
		std::string route;
		std::vector<Item> items;
	};

	std::vector<Node> nodes;
	std::vector<Conn> conns;
	std::vector<Loop> loops;

	bool Empty() const { return nodes.empty() && conns.empty() && loops.empty(); }
};
