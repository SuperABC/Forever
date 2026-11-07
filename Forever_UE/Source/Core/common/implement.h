#pragma once

#include "class.h"

#include "common/handle.h"
#include "common/json.h"


class PostImplement : public PostHandle {
public:
	PostImplement(Map* map, Populace* populace, Society* society, Story* story,
		Industry* industry, Traffic* traffic, Player* player);

	virtual void Post(const JsonValue& request) override;
	virtual const JsonValue& GetResult() const override;

private:
	Map* map;
	Populace* populace;
	Society* society;
	Story* story;
	Industry* industry;
	Traffic* traffic;
	Player* player;

	JsonValue result;
};
