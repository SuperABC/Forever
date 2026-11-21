# power.h / power.cpp

## 职责
Core动态提供的"权力词汇表"`PowerCatalog`：党派mod通过`PartyMod::AssignPowers`把这里登记的权力分给党员。

## 关键设计
- `PowerDef{id,name,policy}`：`policy`非空表示该权力关联某个可调政策(如`manage_corporate_tax`关联`corporate_tax`)。
- `RegisterDefaults()`幂等登记9个内置权力，每个可调政策一个：收企业税/所得税/消费税/房租税/关税、发养老金/失业补助/未成年补助、批准移民，
  id一律`manage_<policy id>`。以后想加"发放游戏版号"之类的权力只要再`Register`一条，mod用字符串id引用，Core/mod都不用改接口。
- **权力没有任何内置效果**：不发工资、不改行为，城市运转不依赖任何职位。只记录谁担任，供以后(例如贪腐、脚本查询)使用；
  目前只有C++查询`Polit::PowersOf(citizen)`/`HoldersOf(powerId)`，没有接脚本变量。
- 同id重复`Register`覆盖旧定义。

## 依赖关系
- 被谁依赖：`Polit`持有一份；`Polit::AssignPowers`校验mod返回的权力id必须已登记。
