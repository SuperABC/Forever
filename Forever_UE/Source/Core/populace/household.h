#pragma once

#include "class.h"
#include "common/utility.h"

#include <vector>


// 户：住在同一个房间里、彼此有夫妻或父母子女关系（可经第三人间接相连）的一组人。同房间没有亲属
// 关系的人算不同户。Room持有户，户持有成员，详见household.md。
class Household {
public:
	explicit Household(Room* room);

	Room* GetRoom() const;
	const std::vector<Citizen*>& GetMembers() const;
	std::vector<Citizen*> GetAdults(const Time& now) const;

	bool Contains(const Citizen* citizen) const;
	// citizen和本户任一成员是不是夫妻或父母子女关系。
	bool IsRelatedTo(const Citizen* citizen) const;

	void AddMember(Citizen* citizen);
	// 并入另一户的全部成员（另一户之后为空，由调用方销毁）。
	void Absorb(Household& other);

private:
	Room* room;
	std::vector<Citizen*> members;
};
