# battle(def / action / effect / cultivator / controller)

## 职责

五行斗诀的回合制战斗内核，纯C++、不涉及画面和UE，全部放在`namespace wxdj`里：

- `def.h`：五行属性/克制表、阶位及其武力要求、阶段/招式/效果类型枚举。
- `effect.*`：招式附带效果(物理、穿透、减伤、反弹、吸收、禁锢、持续伤害、回真气/武力/健康，以及金火/金水/土火/土水场地效果)。
- `action.*`：招式(跳过、积攒真气炸弹、释放炸弹、单招、双招)。
- `cultivator.*`：修炼者的属性、阶位和招式表，`LoadActions`解析`xiaohua.txt`格式的招式文件(`#`开头为注释行，招式行后跟若干效果行)。
- `Cultivator::GetBomb()`：已积攒的真气炸弹能量(界面显示"释放真气炸弹(N)"和判断能否释放用)，是对原版唯一新增的接口。
- `controller.*`：回合控制器，`StartTurn → ActionTurn → CheckTurn → EndTurn`四阶段结算，战斗日志(`Log`)通过`GetNews()`取出。

## 关键设计

- 从老工程`Source/WXDJ`原样迁移，规则不改；只做了`namespace wxdj`、去BOM、去掉控制台输出、
  `def.h`里的全局`const map`改`inline const`(避免每个编译单元一份)。
- 全部对象由本DLL(Wxdj.dll)自己分配、自己释放，不跨DLL传递，不涉及跨DLL`new`/`delete`问题。
- 招式文件由调用方(`WxdjPuzzle`)通过post查到路径后传给`Cultivator`构造函数；文件必须是UTF-8
  (招式名直接显示在Canvas上)。

## 依赖关系

- 仅依赖标准库。被`puzzle_wxdj`使用。

## 待办/后续阶段

- 无。
