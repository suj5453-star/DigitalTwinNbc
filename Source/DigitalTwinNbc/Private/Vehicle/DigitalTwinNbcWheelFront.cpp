// Copyright NBC, Inc. All Rights Reserved.

#include "Vehicle/DigitalTwinNbcWheelFront.h"
#include "UObject/ConstructorHelpers.h"

UDigitalTwinNbcWheelFront::UDigitalTwinNbcWheelFront()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	MaxSteerAngle = 40.f;
}