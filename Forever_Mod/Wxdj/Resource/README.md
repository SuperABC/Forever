# Wxdj/Resource

五行斗诀的资源：

- `Wxdj.script`——主线脚本(`config.json`的`story_scripts`；`main_story`用通用的`script_empty`，Wxdj没有自己的ScriptMod)。开局用`record_object`找到唯一建筑`building_wxdj`
  的地址，`spawn_npc`生成何逸峰(无家)，`npc_teleport`到1楼00001房间，`change_control`操控。全程无C++剧情逻辑。
- `wxdj_fg+.layout`/`wxdj_fu+-.layout`——`WxdjBuilding`的1楼/2楼以上模板，拷贝自Test组yuanshen模板(无电梯，
  无行人导航图，见`Test/Resource/README.md`)。
