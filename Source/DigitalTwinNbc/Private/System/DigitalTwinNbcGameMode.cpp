// Copyright NBC, Inc. All Rights Reserved.

#include "System/DigitalTwinNbcGameMode.h"
#include "System/DigitalTwinNbcPlayerController.h"

ADigitalTwinNbcGameMode::ADigitalTwinNbcGameMode()
{
	PlayerControllerClass = ADigitalTwinNbcPlayerController::StaticClass();
}
