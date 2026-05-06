// Copyright NBC, Inc. All Rights Reserved.


#include "Vehicle/DigitalTwinNbcOffroadCar.h"
#include "Vehicle/DigitalTwinNbcOffroadWheelFront.h"
#include "Vehicle/DigitalTwinNbcOffroadWheelRear.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

ADigitalTwinNbcOffroadCar::ADigitalTwinNbcOffroadCar()
{
	Chassis = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Chassis"));
	Chassis->SetupAttachment(GetMesh());

	TireFrontLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tire Front Left"));
	TireFrontLeft->SetupAttachment(GetMesh(), FName("VisWheel_FL"));
	TireFrontLeft->SetCollisionProfileName(FName("NoCollision"));

	TireFrontRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tire Front Right"));
	TireFrontRight->SetupAttachment(GetMesh(), FName("VisWheel_FR"));
	TireFrontRight->SetCollisionProfileName(FName("NoCollision"));
	TireFrontRight->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));

	TireRearLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tire Rear Left"));
	TireRearLeft->SetupAttachment(GetMesh(), FName("VisWheel_BL"));
	TireRearLeft->SetCollisionProfileName(FName("NoCollision"));

	TireRearRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tire Rear Right"));
	TireRearRight->SetupAttachment(GetMesh(), FName("VisWheel_BR"));
	TireRearRight->SetCollisionProfileName(FName("NoCollision"));
	TireRearRight->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));

	GetFrontSpringArm()->SetRelativeLocation(FVector(-5.0f, -30.0f, 135.0f));
	GetBackSpringArm()->SetRelativeLocation(FVector(0.0f, 0.0f, 75.0f));

	GetChaosVehicleMovement()->ChassisHeight = 160.0f;
	GetChaosVehicleMovement()->DragCoefficient = 0.1f;
	GetChaosVehicleMovement()->DownforceCoefficient = 0.1f;
	GetChaosVehicleMovement()->CenterOfMassOverride = FVector(0.0f, 0.0f, 75.0f);
	GetChaosVehicleMovement()->bEnableCenterOfMassOverride = true;

	GetChaosVehicleMovement()->bLegacyWheelFrictionPosition = true;
	GetChaosVehicleMovement()->WheelSetups.SetNum(4);

	GetChaosVehicleMovement()->WheelSetups[0].WheelClass = UDigitalTwinNbcOffroadWheelFront::StaticClass();
	GetChaosVehicleMovement()->WheelSetups[0].BoneName = FName("PhysWheel_FL");
	GetChaosVehicleMovement()->WheelSetups[0].AdditionalOffset = FVector(0.0f, 0.0f, 0.0f);

	GetChaosVehicleMovement()->WheelSetups[1].WheelClass = UDigitalTwinNbcOffroadWheelFront::StaticClass();
	GetChaosVehicleMovement()->WheelSetups[1].BoneName = FName("PhysWheel_FR");
	GetChaosVehicleMovement()->WheelSetups[1].AdditionalOffset = FVector(0.0f, 0.0f, 0.0f);

	GetChaosVehicleMovement()->WheelSetups[2].WheelClass = UDigitalTwinNbcOffroadWheelRear::StaticClass();
	GetChaosVehicleMovement()->WheelSetups[2].BoneName = FName("PhysWheel_BL");
	GetChaosVehicleMovement()->WheelSetups[2].AdditionalOffset = FVector(0.0f, 0.0f, 0.0f);

	GetChaosVehicleMovement()->WheelSetups[3].WheelClass = UDigitalTwinNbcOffroadWheelRear::StaticClass();
	GetChaosVehicleMovement()->WheelSetups[3].BoneName = FName("PhysWheel_BR");
	GetChaosVehicleMovement()->WheelSetups[3].AdditionalOffset = FVector(0.0f, 0.0f, 0.0f);

	GetChaosVehicleMovement()->EngineSetup.MaxTorque = 600.0f;
	GetChaosVehicleMovement()->EngineSetup.MaxRPM = 5000.0f;
	GetChaosVehicleMovement()->EngineSetup.EngineIdleRPM = 1200.0f;
	GetChaosVehicleMovement()->EngineSetup.EngineBrakeEffect = 0.05f;
	GetChaosVehicleMovement()->EngineSetup.EngineRevUpMOI = 5.0f;
	GetChaosVehicleMovement()->EngineSetup.EngineRevDownRate = 600.0f;

	GetChaosVehicleMovement()->DifferentialSetup.DifferentialType = EVehicleDifferential::AllWheelDrive;
	GetChaosVehicleMovement()->DifferentialSetup.FrontRearSplit = 0.5f;

	GetChaosVehicleMovement()->SteeringSetup.SteeringType = ESteeringType::AngleRatio;
	GetChaosVehicleMovement()->SteeringSetup.AngleRatio = 0.7f;
}