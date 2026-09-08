#include "ForeverMenuGameMode.h"

#include "ForeverMenuController.h"

AForeverMenuGameMode::AForeverMenuGameMode()
{
	PlayerControllerClass = AForeverMenuController::StaticClass();
	DefaultPawnClass = nullptr;
}
