#pragma once

#include <string>
#include <vector>


// DoorSpec：一扇门"长什么样、怎么开"的纯数据描述——供BuildingMod/RoomMod/ZoneMod共用，
// 填在各自的doorSpecs表/gate.door里，Core按tag查出对应的DoorSpec决定要不要真正放一扇门
// (ResolveDoorSpec，见door.h)。本身不含任何UE类型，mesh只是一个待LoadObject的软路径字符串。
//
// mesh为空="只是门洞，不放门"——这是这次门系统的核心设计：layout里没有标签的门洞不可选中，
// 永远只是个洞；有标签但查不到对应DoorSpec、或者查到了但mesh为空，同样只是个洞；只有mesh
// 非空才会真正创建Core的Door实例和UE的门组件，见door.h的ResolveDoorSpec。
struct DoorSpec {
	// ---- 外观 ----
	std::string mesh;                         // 门扇资产软路径；空=只是门洞，不放门
	std::string frameMesh;                    // 可选门框资产(静止不动)，空=不放门框

	// ---- 交互 ----
	std::string name;                         // 门名；非空才创建Script，见door.h"Script"一节
	std::string scriptModName = "empty";      // 仅name非空时使用
	std::vector<std::string> milestoneNames;  // 仅name非空时使用

	// ---- 门禁 ----
	int access = 0;                           // 0=Open 1=Owner 2=Locked，默认Open

	// ---- 开门动画参数(全部由Forever层代码驱动，资产不带动画) ----
	int style = 0;                            // 0=Slide推拉 1=Swing平开
	int leaves = 1;                           // 1=单扇 2=双扇对开(第二扇由代码镜像生成)
	float openAmount = 0.f;                   // Slide:滑出距离占扇宽比例(0=默认1)；
	                                           // Swing:开门角度，度(0=默认90)
	float openSeconds = 0.5f;                 // 开/关一次的时长，秒

	// 单扇门(leaves==1)的朝向随机化——双扇门不读这个字段(两扇对称镜像，没有"选哪一边"的
	// 歧义)。true=这扇门的"单侧朝向"(Slide的滑动方向/Swing的铰链边)在Door创建时随机决定
	// 一次，之后固定不变；同一扇门以后每次开关都用这个固定结果，不是每次开门重新随机。
	// 具体随机结果存在Core的Door实例上(Door::IsFlippedSide())，这个字段只是"要不要随机"
	// 的开关，不存结果本身。
	bool randomSide = false;
};
