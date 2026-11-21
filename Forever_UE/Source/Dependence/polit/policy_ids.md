# policy_ids.h

## 职责

9个可调政策的id字符串常量(`PolicyId::kCorporateTax`等)，Core的`PolicyCatalog`、党派mod的提议`PolicyTarget::policy`、权力词汇`PowerInfo::policy`共用。

## 关键设计

- id统一"两个单词、小写下划线"：`corporate_tax`/`income_tax`/`consumption_tax`/`rent_tax`/`import_tariff`/`retirement_pension`/`unemployment_benefit`/`minor_allowance`/`immigration_ratio`。
- 含义、单位、上下限见`Core/polit/policy.md`；新增政策要同时在Core的`PolicyCatalog`注册。

## 依赖关系

- 无。被`Core/polit/*`和`Source/Basic/polit/party_*`引用。
