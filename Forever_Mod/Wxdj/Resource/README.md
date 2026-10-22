# Wxdj/Resource

五行斗诀的资源：

- `Wxdj.script`——主线脚本(`config.json`的`story_scripts`；`main_story`用通用的`script_empty`，Wxdj没有自己的ScriptMod)。开局用`record_object`找到唯一建筑`building_wxdj`
  的地址，`spawn_npc`生成何逸峰(无家)，`npc_teleport`到1楼00001房间，`change_control`操控，随后`start_puzzle`启动`puzzle_wxdj`战斗小游戏(验证用，不需要时删掉这条)。
- `act/xiaohua.txt`——`puzzle_wxdj`的招式表(UTF-8)，由`WxdjPuzzle::Init`通过post的`"resource path"`查询按文件名找到。
  格式：`招式名 属性 真气消耗 威力 阶位 效果数`，其后跟对应行数的效果行；`#`开头为注释行。
- `wxdj_fg+.layout`/`wxdj_fu+-.layout`——`WxdjBuilding`的1楼/2楼以上模板，拷贝自Test组yuanshen模板(无电梯，
  无行人导航图，见`Test/Resource/README.md`)。
