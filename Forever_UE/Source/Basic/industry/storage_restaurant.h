#pragma once

#include "industry/storage_mod.h"

#include <string>


class RestaurantStorage : public StorageMod {
public:
	RestaurantStorage();

	static const char* GetId() { return "storage_restaurant"; }
	virtual const char* GetType() const override { return "storage_restaurant"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
