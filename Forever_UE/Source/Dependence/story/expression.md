# expression.h / expression.cpp

## 职责

剧情作者的顶层原则是"只编辑json就能表达全部逻辑"（见`Core/story/script.md`），落到表达式层
就是：**理论上json里所有的值都是字符串表达式**，统一由这里的`Expression`门面类解析+求值，
不区分"条件"和"普通值"两种类型。

这是一套通用、domain-agnostic的规则引擎，不只服务于story域——`condition`/`change`/`event`
这几层是可以被其它domain复用的基础设施，不是story专属概念。

## 关键设计

- **不支持`$$(expr)`间接寻址语法**——不支持"用一个子表达式的求值结果去动态拼变量名"。
- **变量寻址**（`ResolveVariableName`，读`$$x`和写`set_value`共用），四个变量池对应`ScriptContext`的四个槽位：

  | 写法 | 变量池 | 槽位 | 脚本能不能写 |
  |---|---|---|---|
  | `self.x` | 当前脚本自己的变量 | `Container* self` | 能 |
  | `system.x` | 系统变量(Story的`systemScript`) | `Container* system` | 不能，只由C++写 |
  | `local.x` | 当前事件的字段(`GetLocalValue`) | `const Event* local` | 不能 |
  | **没有这三个前缀** | 全局变量(Story的`globalScript`) | `Container* global` | 能 |

  只有点号是分隔符，去掉前缀剩下的是变量名；没有前缀时整个名字(含下划线)就是变量名：`$$test`、
  `$$fuck_mona`、`$$self_x`读的都是全局变量。
  不需要调用方逐个变量显式注册。
- **`.`点号本身不参与分词**——`.`落在"既不是空白/引号/运算符/括号"的默认分支，和字母数字
  下划线一样被当成标识符的一部分，`$$self.name`会被分词成一个完整token`"self.name"`，不需要
  为点号写任何特判逻辑。`VariableExpression::Evaluate`按`ResolveVariableName`
  只认`self`/`system`/`local`后面紧跟的那一个`.`，切出的剩余部分原样保留、不做任何字符
  替换（`self.job.title`切出的变量名是`"job.title"`，不是`"job_title"`）。
- **表达式树节点抽象基类叫`ExpressionNode`**，`Expression`这个名字留给对外门面类用。树节点层
  的实现（`VariableExpression`/`ConstantExpression`/`ArrayExpression`/`UnaryExpression`/
  `BinaryExpression`）、分词器、中缀转后缀、运算符优先级、隐式字符串拼接，属于纯粹的表达式
  语法层，和"变量从哪来"无关。

## ScriptContext

```cpp
struct ScriptContext {
    Container* self = nullptr;   // self.前缀路由目标：拥有当前表达式的Script自己的变量池
    Container* system = nullptr; // system.前缀路由目标：Story挂载的系统变量池(脚本只读)
    Container* global = nullptr; // 没有前缀的变量的路由目标：Story挂载的全局变量池(脚本可读可写)
    const Event* local = nullptr; // local.前缀路由目标：触发本次匹配/求值的运行时Event
};
```

`self`/`system`都是`Container*`（`common/utility.h`已有的键值接口），`Script`类本身实现
`Container`，因此`self`直接指向当前Script、`system`指向`Story`挂载的一个"无逻辑只有变量表"的
`Script`（见`Core/story/story.md`）。`local`不是`Container*`——local变量来自触发这次匹配的
运行时`Event`实例自己的字段，通过`Event::GetLocalValue(name)`虚方法暴露（见`event.md`），没有
用`Container*`是因为不是每个`Event`都需要维护一份独立的键值表，让每个`Event`子类按需重载自己
的字段更轻量。三个槽位任意一个为`nullptr`（或前缀路由不到）时，对应的`$$xxx`求值到默认值（0），
不额外报错。

## 依赖关系

- 依赖：`common/utility.h`（`ValueType`/`Container`）、`common/error.h`（`THROW_EXCEPTION`）、
  `event.h`（`Event::GetLocalValue`，仅`expression.cpp`里`VariableExpression::Evaluate`需要，
  `expression.h`只前置声明`class Event;`，避免头文件循环——`event.h`反过来要include
  `expression.h`拿`Expression`类型）。
- 被谁依赖：`event.h`（`Event::condition`）、`change.h`（`Change::condition`）、`dialog.h`
  （`Dialog`/`Option`的`condition`、`Section`的四个台词字段）、`Core/story/milestone.h`
  （`Milestone::drop`）、`Core/story/script.h`（`Script::BuildExpression`统一解析json字符串）。

## 待办/后续阶段

- 表达式语法本身（运算符/字面量/隐式拼接）暂不需要扩展，等剧情作者反馈实际编写json时缺什么
  语法再补。
