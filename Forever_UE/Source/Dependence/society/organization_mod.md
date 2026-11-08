# organization_mod.h / organization_mod.cpp

## 职责

`OrganizationMod`：一种组织类型(比如商店)的行为定义——需要哪些`Component`、每个工位房间配什么`Job`。

## 关键设计

- **Dependence层看不到`Job`/`Component`/`Room`/`Citizen`这些Core类**(Core依赖Dependence，反过来不行)，所以接口只用字符串/int/`Time`/`Change*`表达"要什么""配什么"，真正的`Component`/`Room`遍历和`new Job(...)`全部在Core侧的`Organization`类里做，见`Core/society/organization.md`。
- **`GetPower()`**是static，通过`OrganizationFactory::RegisterOrganization`的`PowerFunc`登记，不是mod实例方法：`Society::Init`要对着每个`Component`实例x每种已注册组织类型做加权判断，不能为每个组合构造实例。
- **`static GetRequirements()`**：返回`Component`类型 -> [min, max]数量范围的const引用(函数内static变量)，可以列多种类型，也可以对同一种类型要多个。随`RegisterOrganization`的最后一个参数注册，和`GetPower`同一个模式，`Society::Init`通过`OrganizationFactory::GetRequirements(id)`读取，不需要构造mod实例。
- **`vacancies`（岗位计划）**：`std::vector<Vacancy>`，`Vacancy{room, type}`=房间名+岗位类型，同一个(房间, 类型)出现几次就计划几个岗位。由mod持有并就地修改，Core只读（里面的`string`在mod堆上分配，Core侧释放会跨DLL堆崩溃）；Core按它创建/重开/关闭`Job`，每个房间的计划总数最多取容量的2倍。
- **`DesignJobsForRoom(..., bool reset)`**：Core侧`Organization`遍历已认领`Component`的每个workspace房间，每个房间调一次，把这个房间的完整上下文(组合类型/名字、房间类型/名字、工位数`workspaceCapacity`)喂给mod；mod往`vacancies`追加这个房间的`{房间名, job类型}`(可以混着配)。`reset`为true时mod先自己`vacancies.clear()`再追加——只有倒闭重启时对第一个房间传true，其余调用（包括构造）都是false。不需要配Job就不追加。
- **`MonthlySettle(const OrganizationMonthlyState&)`**：每月调一次（倒闭中的组织不调）。状态含余额、当月利润、初始资金、总股数/库存股/股价、全组织在岗人数和开放岗位数，以及`rooms`（每个工位房间的名字和容量，供mod按房间安排人数）。mod就地改`vacancies`（多追加几条=扩招，erase几条=裁员），也可以改`dividendPayoutRatio`、设`issueShares`（本月增发股数，Core调用前置0）；不改就是岗位、分红率保持不变、不增发。Core调用后据`vacancies`对账岗位，`vacancies`为空的mod（外部Mod）不动岗位。
- **`SaveVacancies`/`LoadVacancies`**：基类的虚函数（实现在头文件里，所以虚调用落在mod DLL里，`vacancies`的内存始终在mod一侧分配释放），把`vacancies`写成/读成字符串数组`[room,type,room,type,...]`，Core把它作为一段文本存进组织记录。
- **`DailyPlan`/`ExecNode`**可选(默认空实现)：大部分组织不需要自己的调度(比如发工资)；真要用的具体组织类型重写，直接写自己的`plans`/`changes`成员字段，和`JobMod`同一个模式、接口保持对称。
- **Script配置**：`scriptModName`/`milestoneNames`，和`JobMod`同一组字段/同一套安全模式；组织也能挂一个自己的`Script`。
- **经济参数**：`initialFunds`(分)、`shareholders`(每个元素一个股东的占股，Core归一化后随机选成年市民)、`wage`(每员工日工资，分)、`dividendPayoutRatio`(分红率[0,1]，默认1=全分；月度可分配利润缴企业税后，只有这个比例发给股东，其余留在企业账户，抬高净资产和股价)。
- **服务接口**：`ServiceOffer{name,type,unitPrice,quality,capacity}`，`capacity`是当日最多能提供的份数，系统按它加权分配并防超卖。`CollectServices(script, stockTypes, post)`：系统先把状态写进script(商店是`self.stock.<货物类型>`整数库存)，mod在自己的`services`里填(先`clear`再`push`)；服务名只给组织自己识别，商店直接用货物类型名；`post "product info"`可查零售价。`ApplyServiceUsage(name, count, script)`：mod只改script变量，系统随后按变量值回写`Storage`，mod不碰Core对象。

## 依赖关系

- 依赖：`common/mod_state.h`、`common/utility.h`、`common/handle.h`、`Change`(前向声明)。
- 被谁依赖：`organization_factory.h`创建它；`Core/society/organization.h`/`society.h`消费它；具体实现在`Source/Basic/society/organization_basic.*`。

## 待办/后续阶段

- 无。
