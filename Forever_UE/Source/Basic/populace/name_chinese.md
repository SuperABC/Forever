# name_chinese.h / name_chinese.cpp

## 职责

`ChineseName`(`name_chinese`)：中文取名算法的具体实现——姓氏词库+男/女/中性名词库+取名逻辑。

## 关键设计

- 接口是"传一个set结果的回调进去"的写法，不按值返回`std::string`(跨DLL安全，原因见`Dependence/populace/name_mod.md`)。
- `InitializeSurnames()`/`InitializeNames()`在构造函数里填`surnames`/`maleNames`/`femaleNames`/`neutralNames`四个词库。
- **姓氏非均匀采样**：靠前的姓更常见，不存权重表。从`[0,N^2)`均匀取`randVal`，`idx=floor(sqrt(randVal))`本身是倾斜的(`idx`越大概率越高)，取`surnameIdx=N-1-idx`把倾斜反过来，词库里常见姓氏排在前面就隐式得到更高概率。
- **名字**：`GenerateName`按`allowMale`/`allowFemale`/`allowNeutral`把三个词库视为一个拼接的候选区间，每个字随机抽一个；单字名概率1/9，两字名8/9。三类都不允许时兜底只用中性词库；词库为空则不调用`setResult`。
- `GetSurname`取第一个UTF-8字符(校验首字节长度和续字节)，非法UTF-8不调用`setResult`。
- `GetName()`是"中文姓名"+实例编号的debug名，不是生成出的人名；计数器存档见`Dependence/common/mod_state.md`。

## 依赖关系

- 依赖：`populace/name_mod.h`、`common/utility.h`(`GetRandom`)。
- 被谁依赖：`basic.cpp`注册；`Core/populace/name.h`/`populace.h`消费，详见`Core/populace/populace.md`。

## 待办/后续阶段

- 名字拼接不查重、不重试(一次必成功，仅词库为空时生成失败)；需要去重时再加。
