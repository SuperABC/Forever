# condition.h / condition.cpp

原样移植自旧工程`E:\Projects\Forever_UE\Source\Dependence\story\condition.h/.cpp`，未做任何
修改（无windows/UE类型依赖，纯字符串/`ValueType`计算）。这是阶段4-0要最先迁移的"共享脚本
引擎"三件套（`condition.h`/`change.h`/`event.h`）里最底层的一份——一个**完全不感知任何具体
domain**的小型表达式求值器，`map`/`populace`/`society`/`traffic`等domain之后要在阶段4接上
真正的"变量从哪来"，但表达式语法/求值算法本身与它们无关，因此提前到阶段4-0、在Map域之前
迁移。详见`REFACTOR_PLAN.md`同目录`PHASE4_PLAN.md`"关键发现2"一节的说明。

## 职责

`Condition`把一段**字符串**（如`"$$level > 3 && $$job == \"doctor\""`）解析成表达式树
（`Expression`及其派生类），随后可以反复调用`EvaluateBool`/`EvaluateValue`求值——调用方每次
传入一个"变量名→值"的查询函数列表（`getValues`，从后往前查，允许后面的作用域覆盖前面的），
不需要重新解析字符串。这是`story/change.h`（变化的触发条件）、`story/event.h`（事件的匹配
条件）共用的底层能力，后续`Script`/`Milestone`等业务层要写"什么条件下触发什么效果"时统一走
这一套语法。

## 关键设计

- **变量语法用`$$name`前缀，间接寻址用`$$(expr)`**——`$$level`直接按名字查`getValues`；
  `$$(expr)`先对括号内的子表达式求值，把求值结果（转成字符串）**再当一次变量名**去查
  （`IndirectExpression`），用于"变量名本身也是动态算出来的"场景（如按某个索引变量拼出
  `"job_" + $$index`再间接取值）。
- **相邻操作数之间会隐式插入`+`**——`InfixToPostfix`里`isRhs(prev)`检测到"上一个token是操作
  数/右括号，当前token又是操作数"时，会主动压入一个`+`运算符。这让"文本模板拼接"可以不写
  `+`号，直接写`"你好，" $$name "！"`，效果等价于字符串拼接`"你好，" + $$name + "！"`——这是
  写故事脚本文本时的常见需求（把变量嵌进一段提示语里），旧工程选择用词法层面的隐式规则而不是
  单独的模板字符串语法。
- **中缀转后缀用标准Shunting-yard算法**，运算符优先级从高到低：`!`/`negate`(8) > `^`(7) >
  `* / %`(6) > `+ -`(5) > `in`和比较运算符(4，同级) > `&&`(3) > `||`(2)；`^`和一元`!`右结合，
  其余左结合。`in`用于数组包含判断（右操作数必须是`ArrayExpression`，`[a, b, c]`语法）。
- **数值/类型转换走`ValueType`（`utility.h`）的`std::variant`+`visit`模式**——比较两个不同
  类型的值时（`CompareDifferentType`），数值类型之间转`double`比较，只要有一方是字符串就都
  转字符串比较；算术运算里`+`对字符串是拼接、对数值是加法，除法/取模会检查除零并抛
  `RuntimeException`。
- **`Tokenize`本身处理引号字符串（含转义）、`$$(...)`平衡括号扫描、双字符运算符
  （`&&`/`||`/`==`/`!=`/`<=`/`>=`）**，不依赖成熟的词法分析库，是手写的单遍扫描器；全角空格
  （`　`）也被当成空白符处理，兼容中文输入法误触的场景。

## 依赖关系

- 依赖：`common/utility.h`（`ValueType`及`ToInt`/`ToDouble`/`ToBool`/`ToString`转换函数）、
  `common/error.h`（`THROW_EXCEPTION`，语法/求值错误统一抛`RuntimeException`，`ParseCondition`
  内部捕获解析异常返回`false`而不是让异常传出）。
- 被谁依赖：`story/change.h`（`Change::condition`）、`story/event.h`（`Event::condition`，
  以及`Match`实现里直接`new Condition`临时解析字段值做匹配，如`event.cpp`的
  `GlobalMessageEvent::Match`）。后续阶段4的`Script`/`Milestone`会是这套引擎真正的业务消费方。

## 待办/后续阶段

- 阶段4-6（Story域业务内容）：`Script`/`Milestone`落地后，才会有真实的"变量从哪查"的
  `getValues`实现（读Populace/Society/Map等domain的运行时数据），本文件届时不需要改动。
