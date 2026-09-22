# experience.h / experience.cpp

## 职责

四类人际关系（亲属/同学/同事/情感）的可追溯历史记录。`Experience`基类只存公共部分——
类别（`RELATIONSHIP_CATEGORY`）+起止年份（年份粒度，和`Citizen::GetAge`一致，不引入
精确到月/日的时间比较；`endYear == -1`表示这段经历仍在持续）。四个派生类
（`KinshipExperience`/`EducationExperience`/`JobExperience`/`EmotionExperience`）各自
附加自己需要的信息，存进`Citizen::experiences`（取得所有权，`~Citizen()`统一delete）。

参考老工程（`E:\Projects\Forever_UE`）`Person`身上的`Experience`及其派生类设计，但老工程
这套体系和`acquaintances`（关系强度dial）完全脱节——`AddAcquaintance`在老工程里从未被
生成算法调用过，`EducationExperience::classmates`/`teacher`字段声明了但从未赋值，
`GenerateJobs()`是空函数。这次要求两者对应，但不是简单的"一对一"照抄，见下"关键设计"。

## 关键设计

- **两种基数，不是统一的"一对一"**：
  - **亲属(`KinshipExperience`)/情感(`EmotionExperience`)是"一对一"**：这两类关系本身
    就是和某个具体他人的关系，`other`直接是派生类自己的字段，一条`Experience`对应一条
    `acquaintances`条目。
  - **同学(`EducationExperience`)/同事(`JobExperience`)是"一对多"**：一条`Experience`
    表达的是"我自己"参与的一段经历（在某个班/某个组织待过这段时间），不指向具体某个人；
    由这一条经历，*派生*出和"当时同一个班/同一个组织里其他人"的一批`acquaintances`条目
    （同学是全班都算，同事是随机抽一部分，见`populace.md`/`Source/Core/society/
    society.md`）。这批派生出来的acquaintances之所以"对应"这条Experience，是因为可以
    反过来查：两个citizen互为acquaintances，且各自都有一条`EducationExperience`指向
    同一个`SchoolClass`（或`JobExperience`指向同一个`Organization`且任期重叠）——不需要、
    也不应该为每一对同学/同事都单独`new`一条`Experience`。
- **`RELATIVE_TYPE`不区分父/母、儿子/女儿**：照抄老工程`Person::AddRelative(type,
  person)`的语义（`type`说的是`other`的身份，不是`self`的），但简化成
  `RELATIVE_SPOUSE`/`RELATIVE_PARENT`/`RELATIVE_CHILD`/`RELATIVE_SIBLING`四种，不像
  老工程拆`RELATIVE_FATHER`/`RELATIVE_MOTHER`/`RELATIVE_SON`/`RELATIVE_DAUGHTER`——
  `Citizen::GetGender()`已经能查性别，没必要在枚举里再拆一份。老工程也没有
  `RELATIVE_SIBLING`（兄弟姐妹只能靠"共同父母"反查），这次直接建模成一等公民，见
  `populace.md`"兄弟姐妹反推"一节。
- **`EmotionExperience`不需要标记是不是婚姻**：结婚与否、结婚年份去查
  `KinshipExperience(RELATIVE_SPOUSE)`就有，`EmotionExperience`只单纯记录"和这个人处过
  一段感情，从哪年到哪年"——恋爱到婚姻是同一段感情的连续记录，只有一条
  `EmotionExperience`（`beginYear`是恋爱开始年份），不会在结婚那年断开重写一条。
- **`EducationExperience`/`JobExperience`不持有`SchoolClass*`/`Organization*`的所有权**
  ——分别由`Populace::schoolClasses`、`Society`的`organizations`持有，`Experience`只存
  一个不持有所有权的指针。

## 依赖关系

- 依赖：`class.h`（`Citizen`/`SchoolClass`/`Organization`前向声明）。
- 被谁依赖：`Source/Core/populace/citizen.h/.cpp`（`experiences`列表、`GetCurrentLovers`
  派生查询）、`Source/Core/populace/populace.cpp`（`GenerateKinshipRelations`/
  `GenerateRomanticRelations`/`GenerateEducations`三个生成pass，见`populace.md`"四类
  人际关系生成"一节）、`Source/Core/society/society.cpp`（`GenerateEmploymentHistory`，
  见`society.md`同一节）。
