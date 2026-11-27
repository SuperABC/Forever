#include "populace/kinship.h"

#include "common/utility.h"
#include "populace/citizen.h"

#include <algorithm>


using namespace std;

// 四类人际关系Relation随机初始化——数值量级参考，均值/方差实现后可再调。人际关系强度
// 约定俗成非负，clamp到[0,1]，这次不引入负值关系。
void RandomizeKinshipRelation(Citizen* citizen, const string& otherName) {
	citizen->SetAcquaintanceValue(otherName, RELATION_FAMILIARITY, clamp(GetRandomNormal(0.8f, 0.15f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_RESPECT, clamp(GetRandomNormal(0.6f, 0.2f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_FAVOUR, clamp(GetRandomNormal(0.7f, 0.2f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_TRUST, clamp(GetRandomNormal(0.7f, 0.2f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_COMPETING, clamp(GetRandomNormal(0.2f, 0.15f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_RELIABILITY, clamp(GetRandomNormal(0.7f, 0.2f), 0.0f, 1.0f));
}

// 按年龄递减的"这一年开始一段新恋情"基础年概率——年轻人恋爱更频繁，年龄越大频率越低，
// 参考现实约会频率随年龄下降的趋势。恋爱史生成、当前情人/婚外情判定、运行时配对都基于这同一个
// 函数，不能用和年龄无关的常数（否则会让单身时间越长的人反而分配到越多段恋爱史，或者
// 老年人和年轻人一样容易有婚外情）。
float DatingHazardForAge(int age) {
	if (age < 20) return 0.10f;
	if (age < 30) return 0.14f;
	if (age < 40) return 0.07f;
	if (age < 50) return 0.03f;
	if (age < 60) return 0.012f;
	return 0.005f;
}

// 两个citizen是否已经是亲属(用来避免恋爱/情人对象选中近亲)——扫描一方的experiences找
// 是否有一条KinshipExperience指向另一方，见populace.md"四类人际关系生成"一节。
bool IsCloseRelative(const Citizen* a, const Citizen* b) {
	for (Experience* experience : a->GetExperiences()) {
		if (experience->GetCategory() != RELATIONSHIP_KINSHIP) continue;
		if (static_cast<KinshipExperience*>(experience)->GetOther() == b) return true;
	}
	return false;
}

void LinkKin(Citizen* a, Citizen* b, RELATIVE_TYPE aSeesB, RELATIVE_TYPE bSeesA, int beginYear) {
	if (!a || !b || a == b) return;
	a->AddAcquaintance(b->GetName(), RELATIONSHIP_KINSHIP);
	RandomizeKinshipRelation(a, b->GetName());
	a->AddExperience(new KinshipExperience(b, aSeesB, beginYear));
	b->AddAcquaintance(a->GetName(), RELATIONSHIP_KINSHIP);
	RandomizeKinshipRelation(b, a->GetName());
	b->AddExperience(new KinshipExperience(a, bSeesA, beginYear));
}
