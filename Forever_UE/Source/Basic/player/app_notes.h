#pragma once

#include "player/app_mod.h"

#include <string>
#include <vector>


class NotesApp : public AppMod {
public:
	NotesApp();

	static const char* GetId() { return "app_notes"; }
	virtual const char* GetType() const override { return "app_notes"; }
	virtual const char* GetName() override;

	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual void Loop(Canvas* canvas, int ms, PostHandle* post) override;
	virtual void Back(Canvas* canvas, PostHandle* post) override; // 详情->列表；已经在列表则不做任何事
	virtual void Refresh(Canvas* canvas, PostHandle* post) override; // 停在离开前的页面，不重置selection

private:
	enum class Screen { List, Detail };

	struct Note {
		std::string title;
		std::string body;
	};

	void RenderList(Canvas* canvas) const;
	void RenderDetail(Canvas* canvas) const;
	int HitRow(int x, int y) const;

	std::vector<Note> notes;
	Screen screen = Screen::List;
	int selection = 0;
	int rowHeight = 0;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
