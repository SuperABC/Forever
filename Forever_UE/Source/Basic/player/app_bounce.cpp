#include "app_bounce.h"

#include "player/canvas.h"
#include "common/utility.h"

#include <algorithm>
#include <cmath>


using namespace std;

namespace {
	constexpr float kSpeed = 240.f; // 像素/秒，按画布尺寸动态定坐标，但速度直接用像素/秒定值——
									 // 反正这个App只是验证Init/Loop/Refresh三个生命周期，不追求
									 // 在不同分辨率下观感一致。
}

int BounceApp::count = 0;

BounceApp::BounceApp() : id(count++) {

}

const char* BounceApp::GetName() {
	name = "Bounce" + to_string(id);
	return name.data();
}

void BounceApp::Init(Canvas* canvas, PostHandle* post) {
	boxSize = max(10, canvas->GetHeight() / 20);

	posX = static_cast<float>(GetRandom(max(1, canvas->GetWidth() - boxSize)));
	posY = static_cast<float>(GetRandom(max(1, canvas->GetHeight() - boxSize)));

	// 随机选一个45度倍数的方向，避免卡在纯水平/竖直反弹的死循环观感。
	float angle = static_cast<float>(GetRandom(8)) * 3.14159265f / 4.f;
	velX = cosf(angle) * kSpeed;
	velY = sinf(angle) * kSpeed;
	if (fabsf(velX) < 1.f) velX = kSpeed;
	if (fabsf(velY) < 1.f) velY = kSpeed;
}

void BounceApp::Loop(Canvas* canvas, int ms, PostHandle* post) {
	float delta = ms / 1000.f;
	int width = canvas->GetWidth(), height = canvas->GetHeight();

	posX += velX * delta;
	posY += velY * delta;

	if (posX < 0) { posX = 0; velX = -velX; }
	if (posX + boxSize > width) { posX = static_cast<float>(width - boxSize); velX = -velX; }
	if (posY < 0) { posY = 0; velY = -velY; }
	if (posY + boxSize > height) { posY = static_cast<float>(height - boxSize); velY = -velY; }

	Render(canvas);
}

void BounceApp::Refresh(Canvas* canvas, PostHandle* post) {
	// 从后台恢复：不推进物理、不重新Init，只按当前已有的位置重画一次。
	Render(canvas);
}

void BounceApp::Render(Canvas* canvas) const {
	canvas->SetColor(20, 30, 20);
	canvas->ClearScreen();

	canvas->SetColor(80, 220, 120);
	canvas->PutRect(static_cast<int>(posX), static_cast<int>(posY),
		static_cast<int>(posX) + boxSize, static_cast<int>(posY) + boxSize, true);

	// 标题字号跟NotesApp的"NOTES"/Phone的"HOME"/"TASKS"统一用height/17这一档，避免几个
	// App各写各的除数导致标题大小看着不一致。
	canvas->SetFontSize(max(26, canvas->GetHeight() / 17));
	canvas->SetColor(255, 255, 255);
	canvas->PutString("BOUNCE", canvas->GetWidth() / 40, canvas->GetHeight() / 40);
}
