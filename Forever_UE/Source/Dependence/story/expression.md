# expression.h / expression.cpp

阶段4新设计，取代阶段4-0原样移植的`condition.h`/`condition.cpp`（已删除）。剧情作者的顶层原则
是"只编辑json就能表达全部逻辑"（见`Core/story/script.md`），落到表达式层就是：**理论上json里
所有的值都是字符串表达式**，统一由这里的`Expression`门面类解析+求值，不区分"条件"和"普通值"两
种类型（老工程的`Condition`只服务于布尔条件，这次改名`Expression`并承担所有json值的解析）。

## 和老工程`condition.h`的差异

- **删除`$$(expr)`间接寻址语法**——不再支持"用一个子表达式的求值结果去动态拼变量名"。
- **变量寻址方式完全不同**：老工程`VariableExpression::Evaluate`遍历一个`vector<function<...>>`
  回调链（从后往前查，允许后面的作用域覆盖前面的），self.xxx这类变量需要调用方显式把每一个
  `self.xxx`写进某一层回调里。这次改成`self.`/`system.`/`local.`三个固定关键字前缀，直接路由到
  `ScriptContext`结构体的三个槽位（`Container* self`/`Container* system`/`const Event* local`），
  不需要调用方逐个变量显式注册，通用化程度更高。
- **`.`点号本身不参与分词**——这是从老工程`Tokenize`直接继承下来的行为（`.`落在“既不是空白/引号/
  运算符/括号”的默认分支，和字母数字下划线一样被当成标识符的一部分），这次没有改动分词器，只是
  明确了这个既有行为：`$$self.name`本来就会被分词成一个完整token`"self.name"`，不需要为点号写
  任何特判逻辑。`VariableExpression::Evaluate`路由时**找的是token文本里第一个出现的`.`或`_`**，
  两者等价，切出的前缀关键字之后的剩余部分（subkey）原样保留、不做任何字符替换（`self.job.title`
  切出的subkey是`"job.title"`，不是`"job_title"`）。
- **表达式树/分词器/中缀转后缀/运算符优先级/隐式字符串拼接** 这几块和老工程完全一致（照抄
  `BinaryExpression`/`UnaryExpression`/`ConstantExpression`/`ArrayExpression`的实现），因为这些
  和"变量从哪来"无关，属于纯粹的表达式语法层，不需要跟着新设计变。

## 命名调整

老工程`Expression`（表达式树节点抽象基类）在这次改名成`ExpressionNode`，腾出`Expression`这个
名字给对外门面类用（老工程门面类叫`Condition`）。这个重命名只是为了让类名贴合"表达式解析器"这个
更贴切的新定位，树节点层的实现细节（`VariableExpression`/`ConstantExpression`/`ArrayExpression`/
`UnaryExpression`/`BinaryExpression`）本身不受影响。

## ScriptContext

```cpp
struct ScriptContext {
    Container* self = nullptr;   // self.前缀路由目标：拥有当前表达式的Script自己的变量池
    Container* system = nullptr; // system.前缀路由目标：Story挂载的全局变量池
    const Event* local = nullptr; // local.前缀路由目标：触发本次匹配/求值的运行时Event
};
```

`self`/`system`都是`Container*`（`common/utility.h`已有的键值接口），`Script`类本身实现
`Container`，因此`self`直接指向当前Script、`system`指向`Story`挂载的一个"无逻辑只有变量表"的
`Script`（见`Core/story/story.md`）。`local`不是`Container*`——local变量来自触发这次匹配的
运行时`Event`实例自己的字段，通过`Event::GetLocalValue(name)`虚方法暴露（见`event.md`），没有
用`Container*`是因为不是每个`Event`都需要维护一份独立的键值表，让每个`Event`子类按需重载自己
的字段更轻量。三个槽位任意一个为`nullptr`（或前缀路由不到）时，对应的`$$xxx`求值到默认值（0），
不额外报错，和老工程"变量查不到"的容错风格一致。

## 依赖关系

- 依赖：`common/utility.h`（`ValueType`/`Container`）、`common/error.h`（`THROW_EXCEPTION`）、
  `event.h`（`Event::GetLocalValue`，仅`expression.cpp`里`VariableExpression::Evaluate`需要，
  `expression.h`只前置声明`class Event;`，避免头文件循环——`event.h`反过来要include
  `expression.h`拿`Expression`类型）。
- 被谁依赖：`event.h`（`Event::condition`）、`change.h`（`Change::condition`）、`dialog.h`
  （`Dialog`/`Option`的`condition`、`Section`的四个台词字段）、`Core/story/milestone.h`
  （`Milestone::drop`）、`Core/story/script.h`（`Script::BuildExpression`统一解析json字符串）。

## 待办/后续阶段

- 目前`Expression`每次`Parse`都是一次性构建表达式树、之后可以反复`Evaluate`，性能特征和老工程
  `Condition`一致，不需要额外优化。
- 表达式语法本身（运算符/字面量/隐式拼接）暂不需要扩展，等剧情作者反馈实际编写json时缺什么语法
  再补。
