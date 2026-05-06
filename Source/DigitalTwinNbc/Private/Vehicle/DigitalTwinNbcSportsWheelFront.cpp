// Copyright NBC, Inc. All Rights Reserved.


#include "Vehicle/DigitalTwinNbcSportsWheelFront.h"

UDigitalTwinNbcSportsWheelFront::UDigitalTwinNbcSportsWheelFront()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;

	WheelRadius = 39.0f;
	WheelWidth = 35.0f;
	FrictionForceMultiplier = 3.0f;
	MaxSteerAngle = 40.f;

	MaxBrakeTorque = 4500.0f;
	MaxHandBrakeTorque = 6000.0f;

	SuspensionMaxRaise = 5.0f;
	SuspensionMaxDrop = 5.0f;
	SpringRate = 500.0f;
	SpringPreload = 100.0f;
	SuspensionDampingRatio = 1.5f;
	SuspensionSmoothing = 6;
	RollbarScaling = 0.8f;
}
