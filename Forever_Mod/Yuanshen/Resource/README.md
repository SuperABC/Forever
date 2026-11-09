# Yuanshen/Resource

`Yuanshen`组mod用到的资源文件(`Forever_UE/Resource/Config/config.json`的`resource_paths`已收录这个目录)：

- `yuanshen_fg+.layout`——`YuanshenBuilding`的1楼模板(5个single+2个row+楼梯+电梯井)。
- `yuanshen_fu+-.layout`——`YuanshenBuilding`的2楼及以上模板(2个single+2个row+楼梯+电梯井)。
- `ys.script`——配合`YuanshenBuilding`/`YuanshenScript`的示例剧情，角色传送全在脚本里；角色头像用`/Yuanshen/<名字>.<名字>`，指向`../UE/Yuanshen`插件里的贴图(`.gitignore`已忽略这个个人脚本)。
- `config_ys.json`——这个剧情的完整`config.json`样例(`story_scripts`只挂`ys.script`，`main_story`为`script_empty`)，按需拷成实际的`config.json`使用。

模板**没有`pedestrianNavigation`/`vehicleNavigation`块**：Core照常加载，但楼内没有行人导航图，居民走不进楼，等以后需要时参照`Forever_UE/Resource/Layouts/preset_tshape_double_fg+-.layout`补。模板的`singles`没有`direction`字段，缺省按0(FACE_WEST)读。
