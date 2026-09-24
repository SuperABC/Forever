#pragma once

#include "player/app_mod.h"

#include <string>
#include <vector>


// Notes——手机系统的第二个测试App：硬编码几条便签的两级页面(列表->详情)。用来验证
// Back(从详情返回列表)、鼠标点击列表项，以及Refresh(从后台切回来时停在离开前的页面/
// 选中项，不会跳回列表)。不做老工程ZheyeApp那种JSON数据文件+配置路径查询的复杂度——
// 数据直接硬编码在.cpp里，够验证Phone整个状态机就行。
//
// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id+GetName里现拼。
class NotesApp : public AppMod {
public:
	NotesApp();

	static const char* GetId() { return "notes"; }
	virtual const char* GetType() const override { return "notes"; }
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

	static int count;
	int id;
	std::string name;
};
