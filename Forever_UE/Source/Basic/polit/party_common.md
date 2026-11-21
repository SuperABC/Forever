# party_common.h / party_common.cpp

## 职责

内置两个党派(`party_republican`/`party_democratic`)共用的小工具：

- `PickRandomMeetingRoom(post)`：发`{"post":"random room","type":"room_meeting"}`，成功返回房间地址，失败返回空串。同一种子下结果可复现(Core先把候选地址排序再用当前随机流抽，见`Core/common/implement.md`)。
- `AssignPolicyPowersRandomly(context, out)`：每个关联了政策(`PowerInfo::policy`非空)的权力随机挑1名党员(一人可兼多项)，没关联政策的权力不指派。先`clear`再填`out`(mod自己的成员容器，Core只读复制，不跨DLL修改)。

## 依赖关系

- 依赖：`Dependence/polit/party_mod.h`、`common/json.h`、`common/utility.h`(`GetRandom`，抽的是Core绑定给本党派实例的随机流)。
- 被谁依赖：`party_republican.cpp`、`party_democratic.cpp`。
