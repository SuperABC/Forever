#include "puzzle_anykey.h"

#include "common/utility.h"

#include <algorithm>
#include <cstring>


using namespace std;

int AnyKeyPuzzle::count = 0;

AnyKeyPuzzle::AnyKeyPuzzle() : id(count++) {

}

const char* AnyKeyPuzzle::GetName() {
	name = "AnyKey" + to_string(id);
	return name.data();
}

void AnyKeyPuzzle::SetParam(const std::string& param) {
	message = param;
}

void AnyKeyPuzzle::Init(Canvas* canvas, PostHandle* post) {
	canvas->ClearKeyBuffer(); // 打开瞬间还按着的键(比如点选项的那一下)不能算退出
	canvas->ClearMouseBuffer();

	// 配图：从Resource/Mod/Basic/下按文件名加载(文件名带mod名前缀保证全局唯一)，加载不到就只显示文字。
	hasImage = LoadModImage(post, "puzzle_anykey_icon.png", image);
}

JsonValue* AnyKeyPuzzle::Loop(Canvas* canvas, int ms, PostHandle* post) {
	bool pressed = false;
	while (canvas->HasPendingKey()) {
		int raw = canvas->PopKey();
		if ((raw & KEY_RELEASED_FLAG) == 0) pressed = true;
	}
	while (canvas->HasPendingMouseEvent()) {
		if (canvas->PopMouseEvent().down) pressed = true; // 点击/触摸任意处也算按键
	}
	if (pressed) {
		result = JsonValue(DATA_OBJECT);
		return &result;
	}

	canvas->SetColor(0, 0, 0);
	canvas->ClearScreen();
	canvas->SetColor(255, 255, 255);
	if (hasImage) {
		int side = min(canvas->GetWidth(), canvas->GetHeight()) / 3;
		int imgW = side, imgH = side * image.height / max(1, image.width);
		canvas->SetAlpha(1.f);
		canvas->PutBitmap(image, (canvas->GetWidth() - imgW) / 2, canvas->GetHeight() / 2 - 12 - imgH - 20, imgW, imgH);
	}
	canvas->SetFontSize(24);
	string text = message.empty() ? string("Press any key") : message;
	canvas->PutString(text, (canvas->GetWidth() - canvas->StringWidth(text)) / 2, canvas->GetHeight() / 2 - 12);
	canvas->SetFontSize(14);
	string hint = "Press any key or tap";
	canvas->PutString(hint, (canvas->GetWidth() - canvas->StringWidth(hint)) / 2, canvas->GetHeight() / 2 + 30);
	return nullptr;
}
