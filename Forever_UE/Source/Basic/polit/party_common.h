#pragma once

#include "polit/party_mod.h"

#include <string>
#include <vector>


// 内置两个党派(共和党/民主党)共用的小工具，设计见party_common.md。

// 向post要一个room_meeting类型的随机房间(post "random room")，成功返回地址，否则返回空串。
std::string PickRandomMeetingRoom(PostHandle* post);

// 权力指派：每个关联了政策(policy非空)的权力随机挑1名党员担任(一人可兼多项)；没关联政策的权力不指派。
// 先clear out再填(mod自己的容器，Core只读)。
void AssignPolicyPowersRandomly(const PartyContext& context, std::vector<PowerAssignment>& out);
