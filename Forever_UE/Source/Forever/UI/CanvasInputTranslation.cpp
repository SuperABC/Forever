#include "UI/CanvasInputTranslation.h"

#include "player/canvas.h"


int32 CanvasInputTranslation::TranslateKey(const FKey& key) {
	static const TMap<FKey, int32> table = {
		{ EKeys::A, 'a' }, { EKeys::B, 'b' }, { EKeys::C, 'c' }, { EKeys::D, 'd' }, { EKeys::E, 'e' },
		{ EKeys::F, 'f' }, { EKeys::G, 'g' }, { EKeys::H, 'h' }, { EKeys::I, 'i' }, { EKeys::J, 'j' },
		{ EKeys::K, 'k' }, { EKeys::L, 'l' }, { EKeys::M, 'm' }, { EKeys::N, 'n' }, { EKeys::O, 'o' },
		{ EKeys::P, 'p' }, { EKeys::Q, 'q' }, { EKeys::R, 'r' }, { EKeys::S, 's' }, { EKeys::T, 't' },
		{ EKeys::U, 'u' }, { EKeys::V, 'v' }, { EKeys::W, 'w' }, { EKeys::X, 'x' }, { EKeys::Y, 'y' },
		{ EKeys::Z, 'z' },
		{ EKeys::Zero, '0' }, { EKeys::One, '1' }, { EKeys::Two, '2' }, { EKeys::Three, '3' },
		{ EKeys::Four, '4' }, { EKeys::Five, '5' }, { EKeys::Six, '6' }, { EKeys::Seven, '7' },
		{ EKeys::Eight, '8' }, { EKeys::Nine, '9' },
		{ EKeys::SpaceBar, ' ' },
		{ EKeys::Enter, KEY_ENTER }, { EKeys::Escape, KEY_ESCAPE }, { EKeys::Tab, KEY_TAB },
		{ EKeys::BackSpace, KEY_BACKSPACE },
		{ EKeys::Left, KEY_LEFT }, { EKeys::Up, KEY_UP }, { EKeys::Right, KEY_RIGHT }, { EKeys::Down, KEY_DOWN },
		{ EKeys::F1, KEY_F1 }, { EKeys::F2, KEY_F2 }, { EKeys::F3, KEY_F3 }, { EKeys::F4, KEY_F4 },
		{ EKeys::F5, KEY_F5 }, { EKeys::F6, KEY_F6 }, { EKeys::F7, KEY_F7 }, { EKeys::F8, KEY_F8 },
		{ EKeys::F9, KEY_F9 }, { EKeys::F10, KEY_F10 }, { EKeys::F11, KEY_F11 }, { EKeys::F12, KEY_F12 },
	};

	if (const int32* found = table.Find(key)) return *found;
	return 0;
}

int32 CanvasInputTranslation::TranslateMouseButton(const FKey& key) {
	if (key == EKeys::LeftMouseButton) return MOUSE_LEFT;
	if (key == EKeys::RightMouseButton) return MOUSE_RIGHT;
	if (key == EKeys::MiddleMouseButton) return MOUSE_MIDDLE;
	return -1;
}
