#pragma once

#include "class.h"

#include "player/asset.h"
#include "player/asset_factory.h"

#include <string>
#include <unordered_map>
#include <vector>


// 注意：这是"物件/道具/手机"domain（Asset/App/Puzzle/Weapon四个Mod扩展点）的Player类，
// 不是UE的AForeverPlayerController，两者是完全不同的东西。
//
// 目前承载全局时钟（Time*+Init/Tick/GetTime/SetTime/CrossDay）、生命值、资产系统
// （leftHand/rightHand/backPack/leftShoulder/rightShoulder五槽位+不动产/载具表+路径
// 寻址，见下方"资产系统"一节和player.md）。手机(Phone)、存款等字段/方法尚未迁移，见
// player.md。

class Change;
struct ScriptContext;

class Player {
	// 存档：PlayerSaveCodec直接读写私有字段(player_save.md)。
	friend class PlayerSaveCodec;
public:
	Player();
	~Player();

	// 初始化玩家时钟：创建Time，默认设为8点（真正的起始日期由脚本后续SetTime/change_time
	// 决定）。
	void Init();

	// 时钟周期更新：按真实帧时长推进游戏时钟。time_flow_ratio默认2.0，可以被主线剧情
	// .script的global_settings.time_flow_ratio字段覆盖（见SetTimeFlowRatio），见
	// player.md"time_flow_ratio"一节。
	// @delta: 本帧时长（秒）
	void Tick(float delta);

	// 和其余六个域同一个约定：返回这个Change是不是本域认识的类型，由AForeverFrameworkActor::ApplyChange
	// 汇总。目前还不认识任何Change——玩家相关的(给物品/受伤/改时间/银行收支……)补的时候放在这里。
	bool ApplyChange(const Change* change, const ScriptContext& context);

	// 覆盖time_flow_ratio——由AForeverFrameworkActor::EnsurePlayerGenerated()读取主线
	// 剧情.script的global_settings.time_flow_ratio字段后调用，没有声明则保持默认值2.0。
	void SetTimeFlowRatio(double ratio);

	// 获取游戏时钟
	Time* GetTime() const;

	// 直接将游戏时钟设为绝对时间（脚本change_time瞬间跳变用）；会记录跳变前的天数，
	// 供CrossDay正确检测跨天。
	// @newTime: 目标绝对时间
	void SetTime(const Time& newTime);

	// 是否发生跨天（相对上一次Tick/SetTime时的天数）
	bool CrossDay();

	// 生命值——"扣血/回血/是否死亡"最小闭环，不做死亡后果(复活/game over等)，见
	// Source/Forever/Player/ForeverWeaponComponent.md。伤害/治疗量传负数没有意义，调用方
	// 保证传正数，这里不做防御性纠正。
	float GetHealth() const;
	float GetMaxHealth() const;
	// @amount: 扣除的生命值(正数)，返回值是扣除后的剩余生命值(已clamp到[0, maxHealth])
	float TakeDamage(float amount);
	// @amount: 恢复的生命值(正数)，返回值是恢复后的生命值(已clamp到[0, maxHealth])
	float Heal(float amount);
	bool IsDead() const;

	// ---- 资产系统 ----
	// 创建一个Asset实例，id需要在config.json的"asset_mods"数组里启用，否则返回nullptr。
	// 独占持有权转交给调用方（调用方负责走AddByPath/AddContent等接口把它放进某个槽位/
	// 容器/房间，否则要自己delete）。
	// 名字由mod的GetName()给出(全局唯一)。
	Asset* CreateAsset(const std::string& id);
	// 从src堆叠里拆出n个(0<n<src->count)：新建一个同type的实例(名字由它的mod保证唯一)，
	// count=n，src->count减n。失败返回nullptr、不改动src。所有权交给调用方。
	Asset* SplitAsset(Asset* src, int n);
	// 调用方确认这个Asset已经不在任何槽位/容器/房间表里之后调用——内部走
	// factory->DestroyAsset(mod)+delete this，不能跳过这个方法直接delete（见
	// AssetFactory跨DLL new/delete安全的约定）。
	void DestroyAsset(Asset* asset);

	static std::vector<std::string> SplitPath(const std::string& path);
	// 按路径查询/摘除/写入——"left"/"right"/"back"/"leftShoulder"/"rightShoulder"是五个
	// 槽位，"room"是当前房间，多段路径(比如"back/PistolAmmo")按'/'切分后先解析到容器
	// 再进GetContents()查。
	Asset* GetByPath(const std::string& path) const;
	Asset* RemoveByPath(const std::string& path); // 从槽位/容器/房间摘除，不delete
	// 放入规则：
	// - left/right(手)：要求mobility==Object、count==1且当前槽位为空（武器也可以拿在手上，
	//   只是不能激活，激活仍然只走肩膀）。
	// - back：同上，额外要求asset->GetBackpack()==true。
	// - leftShoulder/rightShoulder：要求asset->IsWeapon()==true且当前槽位为空（只能放
	//   武器）。
	// - room：要求GetCurrentRoom()!=nullptr，转发给currentRoom->AddAsset(asset)。
	// - 多段路径：GetByPath找到容器，container->AddContent(asset)，无mobility限制。
	bool AddByPath(const std::string& path, Asset* asset);

	Asset* GetLeftHand() const { return leftHand; }
	Asset* GetRightHand() const { return rightHand; }
	Asset* GetBackPack() const { return backPack; }
	Asset* GetLeftShoulder() const { return leftShoulder; }
	Asset* GetRightShoulder() const { return rightShoulder; }

	// 不动产/载具——不走五槽位，mobility必须是Estate/Vehicle。
	bool AddEstateAsset(Asset* asset);
	Asset* GetEstateAsset(const std::string& name) const;
	void RemoveEstateAsset(const std::string& name);
	const std::unordered_map<std::string, Asset*>& GetEstateAssets() const { return estateAssets; }

	// 真实物理位置(不是剧情/场景语义)，由BuildingElement的Room重叠检测回调维护，见
	// Source/Forever/Element/BuildingElement.cpp的OnRoomOverlapBegin/End。
	Room* GetCurrentRoom() const { return currentRoom; }
	void SetCurrentRoom(Room* room) { currentRoom = room; }

	// 转发到backPack(不存在返回0/false)——供ForeverWeaponComponent换弹时查/扣备弹。
	int CountByType(const std::string& type) const;
	bool ConsumeByType(const std::string& type, int amount);

	// ---- 脚本驱动的物品收纳/扣除(pick_asset/grant_asset/deduct_asset/drop_asset) ----
	// 把一件资产放到玩家身上，不处理它原来的位置(调用方先把它从房间/地面摘下来；失败时自己放回去)。
	// 顺序：①没有背包且它是可背的(GetBackpack、Object、count==1)就背上；②有背包且空间够(size*count，
	// 堆叠整体算)放进背包；③否则右手空放右手、再左手空放左手(手只收Object且count==1的单件)；④都不行失败，
	// 不改变任何状态。
	enum class StoreResult { Failed, AsBackpack, InBackpack, RightHand, LeftHand };
	StoreResult StoreAsset(Asset* asset);

	// 左右手和背包(递归容器)里指定类型的总数；不含肩膀。
	int CountOwned(const std::string& type) const;
	// 摘下amount个指定类型的物品(必要时拆堆叠)交给调用方(out)，所有权转给调用方。数量不够返回false且不改
	// 变任何状态。被摘下的件如果是容器，里面的东西跟着一起走。
	bool TakeOwned(const std::string& type, int amount, std::vector<Asset*>& out);

	// 按名字找资产(手/肩/背及其容器内容、estate、worldAssets)；找不到返回nullptr。房间里的见Map::FindAssetByName。
	Asset* FindAssetByName(const std::string& name) const;

	// ---- 投掷物 ----
	struct ThrowableEntry {
		std::string type;
		int count = 0;
		// 取自该type第一个堆叠，供UE侧显示投掷预览模型(同type的资产网格相同)。
		std::string meshPath;
		float meshScale = 1.f;
	};
	// 汇总身上所有投掷物，按type合并。扫描顺序固定：左手 -> 右手 -> 背包，每个位置先看资产
	// 本身是不是投掷物，再递归进它的contents(子项按name排序，保证顺序确定)——所以手里拿着
	// 的容器、背包里嵌套容器里的投掷物都能找到。返回顺序=各type第一次出现的顺序。
	std::vector<ThrowableEntry> CollectThrowables() const;
	// 按同样的扫描顺序找到第一个该type的堆叠，取出一个单位：count>1时count-1并新建一个
	// count=1的Asset返回；count==1时把整个Asset从手槽/父容器摘出返回。取不到返回nullptr。
	// 返回的Asset所有权交给调用方(用完走DestroyAsset)。
	Asset* TakeOneThrowable(const std::string& type);
	// 仅判断手槽里"直接拿着"该type的资产；在手持容器里面的不算(那只手仍被容器占用)。
	bool HasThrowableInHand(const std::string& type) const;

	// ---- 房间外的物品(室外) ----
	// 不在任何房间、也不在身上/容器里的物品(比如被扔出去的投掷物)归Player直接持有，key=
	// Asset::GetName()。transform是UE世界坐标(厘米)+旋转(度)，Core只存数不解释：UE侧存档前
	// 把场景里对应Actor的实际位置写回来，读档后按它重新生成Actor(速度不存，从静止开始模拟)。
	// 物品落点即使最终在室内也照样记在这里，不转进Room::assets。
	struct WorldAssetTransform {
		float x = 0.f, y = 0.f, z = 0.f;
		float pitch = 0.f, yaw = 0.f, roll = 0.f;
	};
	struct WorldAsset {
		Asset* asset = nullptr;
		WorldAssetTransform transform;
	};
	// 成功后Player独占持有asset；同名已存在返回false(调用方仍持有asset)。
	bool AddWorldAsset(Asset* asset, const WorldAssetTransform& transform);
	// 摘除不delete，调用方接手所有权。
	Asset* RemoveWorldAsset(const std::string& name);
	void SetWorldAssetTransform(const std::string& name, const WorldAssetTransform& transform);
	const std::unordered_map<std::string, WorldAsset>& GetWorldAssets() const { return worldAssets; }

private:
	// 游戏时钟（持有所有权）
	Time* time = nullptr;

	// 上一次Tick/SetTime时的游戏天数，用于跨天检测
	int day = -1;

	// 默认1现实秒=10游戏分钟，一天(1440分钟)需要144现实秒(2.4分钟)。未被脚本
	// (global_settings.time_flow_ratio)覆盖时就是这个值生效。
	double timeFlowRatio = 10.0;

	// 100点是随手给的默认值，具体数值等PIE里试手感再调。
	float health = 100.f;
	float maxHealth = 100.f;

	// ---- 资产系统 ----
	AssetFactory& assetFactory;

	Asset* leftHand = nullptr;
	Asset* rightHand = nullptr;
	Asset* backPack = nullptr;
	Asset* leftShoulder = nullptr;
	Asset* rightShoulder = nullptr;

	// 不动产/载具，key=name，mobility校验在AddEstateAsset里做。
	std::unordered_map<std::string, Asset*> estateAssets;

	Room* currentRoom = nullptr;

	// 房间外的物品，Player独占持有，见上面"房间外的物品"一节。
	std::unordered_map<std::string, WorldAsset> worldAssets;
};
