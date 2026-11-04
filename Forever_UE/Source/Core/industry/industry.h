#pragma once

#include "industry/product.h"
#include "industry/storage.h"
#include "industry/manufacture.h"

#include "common/class.h"
#include "common/handle.h"

#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct ScriptContext;

// 一笔货物流向。from/to为nullptr表示城市外：from为空=进口，to为空=出口，否则是城内流转。
// Industry只记录"谁给谁多少货"，不涉及任何金额，付款由Society按这份流水结算，见industry.md。
struct GoodsFlow {
	Storage* from = nullptr;
	Storage* to = nullptr;
	std::string product;
	int quantity = 0;

	bool IsImport() const { return from == nullptr; }
	bool IsExport() const { return to == nullptr; }
};

class Industry {
	friend class IndustrySaveCodec;
public:
	Industry();
	~Industry(); // delete全部products/storages/manufactures

	// 按config里启用的product mod建目录，再按各Component的Room生成Storage(Room::StorageType())和
	// Manufacture(Room::ManufactureTypes()里的manufacture mod id)。先建目录后建工坊，工坊展开
	// 配方要查目录。
	void Init(const std::vector<Component*>& components);

	// 新开局调用:每个工坊的产出cache放进半天的产量。读档恢复库存时不调用。
	void SeedInitialStock();

	Product* CreateProduct(const std::string& modId);
	Product* FindProduct(const std::string& type) const;
	std::vector<Product*> GetProducts() const; // 按type字典序

	Storage* CreateStorage(const std::string& modId, const std::string& name);
	void DestroyStorage(const std::string& name);
	Storage* FindStorageByName(const std::string& name) const;

	Manufacture* CreateManufacture(const std::string& modId, const std::string& name);
	void DestroyManufacture(const std::string& name);
	Manufacture* FindManufactureByName(const std::string& name) const;

	// 某个Storage(含工坊的两个cache)所在的Room，Init按Room生成的才有，手工创建的返回nullptr。
	Room* GetStorageRoom(const Storage* storage) const;
	// 房间按storageType生成的那个仓库（不含工坊cache），没有返回nullptr。
	Storage* GetRoomStorage(const Room* room) const;
	Room* GetManufactureRoom(const Manufacture* manufacture) const;

	const std::map<std::string, Storage*>& GetStorages() const { return storages; }
	const std::map<std::string, Manufacture*>& GetManufactures() const { return manufactures; }

	void Tick(const Time& currentTime, bool crossedDay, PostHandle* post);

	// 当天Tick产生的货物流水，下一次Tick开始时清空。
	const std::vector<GoodsFlow>& GetFlows() const { return flows; }
	// 当天各工坊结算进产出cache的产量合计（product type->数量），下一次Tick开始时清空，报表用。
	const std::map<std::string, double>& GetDailyProduction() const { return dailyProduction; }

	// 停业的房间（企业倒闭）：这些房间的仓库不再参与物流，工坊不再开工。由Society每天根据企业状态
	// 同步进来（派生数据，不单独存档）。
	void SetClosedRooms(const std::unordered_set<const Room*>& rooms) { closedRooms = rooms; }

	// 工坊所在房间的到岗率(实际在职人数/组织指定的招聘人数)，由Society每天同步(派生数据，不存档)。
	// 工坊产能比例=50%*(1+到岗率)，不设上限；没登记的房间(没有指定人数)按到岗率1(即100%产能)。
	// 出口阈值：工坊产出cache内销之后至少留下容量的这个比例，只有超出的部分才出口。由Economy每天按外部
	// 注资比例算出(1-1/(x+1))，默认0.5(x=1)。
	void SetExportKeepRatio(float ratio) { exportKeepRatio = ratio; }
	void SetStaffingRatios(const std::unordered_map<const Room*, float>& ratios) { staffingRatios = ratios; }

	bool ApplyChange(const Change* change, const ScriptContext& context);

private:
	// 工坊产出cache内销之后至少留下容量的这个比例，只有超出的部分才出口。
	float exportKeepRatio = 0.5f;

	void Distribute();
	void DistributeDemand(const std::vector<Storage*>& putOnly, const std::vector<Storage*>& sources);
	void DistributeSupply(const std::vector<Storage*>& takeOnly, const std::vector<Storage*>& takePut);

	struct Demand {
		Storage* dest;
		int amount;
	};
	// 满足一组需求：candidates是可以满足这批需求的product（按优先顺序），importProduct是缺口进口
	// 时买哪一种。按统一比例把内部供给分给各需求方，没满足的部分直接进口。
	void Fulfil(const std::vector<std::string>& candidates, const std::string& importProduct,
		std::vector<Demand>& demands, const std::vector<Storage*>& sources);

	void Record(Storage* from, Storage* to, const std::string& product, int quantity);

	ProductFactory& productFactory;
	StorageFactory& storageFactory;
	ManufactureFactory& manufactureFactory;

	// 全部用有序map：物流分配的先后顺序必须确定，才能让同一份数据每次跑出同一个结果。
	std::map<std::string, Product*> products;        // 目录，key=产品type
	std::map<std::string, Storage*> storages;         // key=name，普通仓库
	std::map<std::string, Manufacture*> manufactures; // key=name

	std::unordered_map<const Storage*, Room*> storageRooms;
	std::unordered_map<const Manufacture*, Room*> manufactureRooms;
	std::unordered_map<const Room*, Storage*> roomStorages;

	std::unordered_set<const Room*> closedRooms;
	std::unordered_map<const Room*, float> staffingRatios;
	bool IsClosed(const Storage* storage) const;

	std::vector<GoodsFlow> flows;
	std::map<std::string, double> dailyProduction;
};
