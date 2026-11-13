#pragma once

#include "player/puzzle_mod.h"

#include <string>


class AnyKeyPuzzle : public PuzzleMod {
public:
	AnyKeyPuzzle();

	static const char* GetId() { return "puzzle_anykey"; }
	virtual const char* GetType() const override { return "puzzle_anykey"; }
	virtual const char* GetName() override;

	virtual void SetParam(const std::string& param) override;
	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual JsonValue* Loop(Canvas* canvas, int ms, PostHandle* post) override;

private:
	JsonValue result; // 结束时返回{}，没有任何字段
	static int count;
	int id;
	std::string name;
	std::string message;
	Bitmap image;
	bool hasImage = false;
};
