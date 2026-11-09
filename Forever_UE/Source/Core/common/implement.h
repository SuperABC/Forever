#pragma once

#include "class.h"

#include "common/handle.h"
#include "common/json.h"

#include <functional>
#include <string>


class PostImplement : public PostHandle {
public:
	PostImplement(Map* map, Populace* populace, Society* society, Story* story,
		Industry* industry, Traffic* traffic, Player* player);

	virtual void Post(const JsonValue& request) override;
	virtual const JsonValue& GetResult() const override;

	// "screenshot"请求的实际执行者：Core看不到UE，由Forever层注入。参数=文件名前缀，返回保存路径(空=失败)。
	using ScreenshotHandler = std::function<std::string(const std::string& name)>;
	void SetScreenshotHandler(ScreenshotHandler handler) { screenshotHandler = std::move(handler); }

private:
	Map* map;
	Populace* populace;
	Society* society;
	Story* story;
	Industry* industry;
	Traffic* traffic;
	Player* player;

	JsonValue result;
	ScreenshotHandler screenshotHandler;
};
