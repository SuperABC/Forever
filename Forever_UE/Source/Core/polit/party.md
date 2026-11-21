# party.h / party.cpp

## 职责
一个党派实例：独占一个`PartyMod`(经`PartyFactory`的deleter释放，同`Zone`)，加上Core维护的运行时状态。

## 状态字段(公开，只有`Polit`和`PolitSaveCodec`读写)
- `members`：党员(`Citizen*`)；`leader`：党首(目前只记录/存档/写日志，没有实际用处，以后再补)；
- `meetingAddress`/`meetingRoom`：会议室地址与解析出的房间(空=没有会议室；读档时由地址重新解析)；
- `ruling`/`terms`：是否执政党、连续执政期数(执政期间>=1)；
- `powers`：(权力id, 担任者)，只在执政期间有内容；
- `congress`：当月党代会进度(`monthKey`、`plan`日程、`started`/`ended`/`skipLogged`)。

## 依赖关系
- 被谁依赖：`Polit`持有`std::vector<Party*>`；mod侧接口见`Dependence/polit/party_mod.md`。
