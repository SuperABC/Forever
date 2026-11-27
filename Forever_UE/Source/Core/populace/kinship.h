#pragma once

#include "class.h"
#include "populace/experience.h"

#include <string>


// 亲属/恋爱关系的公共小工具：开局生成(populace.cpp)和运行时人口推进(demography.cpp)共用，见demography.md。

// 亲属关系Relation随机初始化(数值量级参考，clamp到[0,1])。从populace.cpp原样挪出来。
void RandomizeKinshipRelation(Citizen* citizen, const std::string& otherName);

// 按年龄递减的"这一年开始一段新恋情"基础年概率，见populace.md"四类人际关系生成"。
float DatingHazardForAge(int age);

// 两人是否已经是亲属：扫a的experiences里有没有指向b的KinshipExperience。
bool IsCloseRelative(const Citizen* a, const Citizen* b);

// 互相写上亲属关系：双方各加熟人(KINSHIP)、随机关系值、一条KinshipExperience。
// aSeesB=b相对a的身份(比如a是父母时传RELATIVE_CHILD)，bSeesA同理。调用顺序和开局生成写亲属关系时一样(先a后b)。
void LinkKin(Citizen* a, Citizen* b, RELATIVE_TYPE aSeesB, RELATIVE_TYPE bSeesA, int beginYear);
