# Test/Resource

`Test`组mod用到的`.layout`资源文件(`Forever_UE/Resource/Config/config.json`的`layout_paths`已收录
这个目录)：

- `yuanshen_fg+.layout`——`YuanshenBuilding`的1楼模板(5个single+2个row+楼梯+电梯井)。
- `yuanshen_fu+-.layout`——`YuanshenBuilding`的2楼及以上模板(2个single+2个row+楼梯+电梯井)。

两个文件内容原样来自旧工程`E:\Projects\Forever_Mods\Test\Resource\pengzhan_f^+.layout`/
`pengzhan_f+-.layout`，只改了文件名(`^+`->`fg+`，`+-`->`fu+-`，对应新工程的命名规则)。**还没有
`pedestrianNavigation`/`vehicleNavigation`块**(旧模板没有)：Core照常加载，但楼内没有行人导航图，
居民走不进楼，等以后需要时参照`Forever_UE/Resource/Layouts/preset_tshape_double_fg+-.layout`补。
旧模板的`singles`也没有`direction`字段，缺省按0(FACE_WEST)读。
