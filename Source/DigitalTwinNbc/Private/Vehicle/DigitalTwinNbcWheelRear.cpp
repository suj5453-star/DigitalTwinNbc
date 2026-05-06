// Copyright NBC, Inc. All Rights Reserved.

#include "Vehicle/DigitalTwinNbcWheelRear.h"
#include "UObject/ConstructorHelpers.h"

UDigitalTwinNbcWheelRear::UDigitalTwinNbcWheelRear()
{
	AxleType = EAxleType::Rear;
	bAffectedByHandbrake = true;
	bAffectedByEngine = true;
}