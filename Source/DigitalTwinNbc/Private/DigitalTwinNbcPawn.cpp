// Copyright NBC, Inc. All Rights Reserved.

#include "DigitalTwinNbcPawn.h"
#include "DigitalTwinNbc.h"
#include "System/DigitalTwinNbcPlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "TimerManager.h"
#include "Component/SplineFollowerComponent.h"
#include "Sensor/CameraSensorComponent.h"
#include "Sensor/LidarSensorComponent.h"
#include "DataLogger/AgentDataLogger.h"

ADigitalTwinNbcPawn::ADigitalTwinNbcPawn()
{
	FrontSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Front Spring Arm"));
	FrontSpringArm->SetupAttachment(GetMesh());
	FrontSpringArm->TargetArmLength = 0.0f;
	FrontSpringArm->bDoCollisionTest = false;
	FrontSpringArm->bEnableCameraRotationLag = true;
	FrontSpringArm->CameraRotationLagSpeed = 15.0f;
	FrontSpringArm->SetRelativeLocation(FVector(30.0f, 0.0f, 120.0f));

	FrontCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Front Camera"));
	FrontCamera->SetupAttachment(FrontSpringArm);
	FrontCamera->bAutoActivate = false;

	BackSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Back Spring Arm"));
	BackSpringArm->SetupAttachment(GetMesh());
	BackSpringArm->TargetArmLength = 650.0f;
	BackSpringArm->SocketOffset.Z = 150.0f;
	BackSpringArm->bDoCollisionTest = false;
	BackSpringArm->bInheritPitch = false;
	BackSpringArm->bInheritRoll = false;
	BackSpringArm->bEnableCameraRotationLag = true;
	BackSpringArm->CameraRotationLagSpeed = 2.0f;
	BackSpringArm->CameraLagMaxDistance = 50.0f;

	BackCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Back Camera"));
	BackCamera->SetupAttachment(BackSpringArm);

	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionProfileName(FName("Vehicle"));

	ChaosVehicleMovement = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());

	CameraSensor = CreateDefaultSubobject<UCameraSensorComponent>(TEXT("CameraSensor"));
	CameraSensor->SetupAttachment(GetMesh());

	LidarSensor = CreateDefaultSubobject<ULidarSensorComponent>(TEXT("LidarSensor"));
	LidarSensor->SetupAttachment(GetMesh());
	LidarSensor->SetRelativeLocation(FVector(0.0f, 0.0f, 180.0f));

	SplineFollower = CreateDefaultSubobject<USplineFollowerComponent>(TEXT("SplineFollower"));
	
	DataLogger = CreateDefaultSubobject<UAgentDataLogger>(TEXT("DataLogger"));
}

void ADigitalTwinNbcPawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(ResetVehicleAction, ETriggerEvent::Triggered, this, &ADigitalTwinNbcPawn::ResetVehicle);
		EnhancedInputComponent->BindAction(ToggleCameraViewAction, ETriggerEvent::Started, this, &ADigitalTwinNbcPawn::ToggleSensorView);
		EnhancedInputComponent->BindAction(ToggleLidarViewAction, ETriggerEvent::Started, this, &ADigitalTwinNbcPawn::ToggleLidarView);
	}
	else
	{
		UE_LOG(LogDigitalTwinNbc, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ADigitalTwinNbcPawn::BeginPlay()
{
	Super::BeginPlay();

	GetWorld()->GetTimerManager().SetTimer(FlipCheckTimer, this, &ADigitalTwinNbcPawn::FlippedCheck, FlipCheckTime, true);
}

void ADigitalTwinNbcPawn::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(FlipCheckTimer);

	Super::EndPlay(EndPlayReason);
}

void ADigitalTwinNbcPawn::Tick(float Delta)
{
	Super::Tick(Delta);

	bool bMovingOnGround = ChaosVehicleMovement->IsMovingOnGround();
	GetMesh()->SetAngularDamping(bMovingOnGround ? 0.0f : 3.0f);

	float CameraYaw = BackSpringArm->GetRelativeRotation().Yaw;
	CameraYaw = FMath::FInterpTo(CameraYaw, 0.0f, Delta, 1.0f);

	BackSpringArm->SetRelativeRotation(FRotator(0.0f, CameraYaw, 0.0f));
}

void ADigitalTwinNbcPawn::LookAround(const FInputActionValue& Value)
{
	DoLookAround(Value.Get<float>());
}

void ADigitalTwinNbcPawn::ToggleCamera(const FInputActionValue& Value)
{
	DoToggleCamera();
}

void ADigitalTwinNbcPawn::ResetVehicle(const FInputActionValue& Value)
{
	DoResetVehicle();
}

void ADigitalTwinNbcPawn::ToggleSensorView(const FInputActionValue& Value)
{
	DoToggleSensorView();
}

void ADigitalTwinNbcPawn::ToggleLidarView(const FInputActionValue& Value)
{
	DoToggleLidarView();
}

void ADigitalTwinNbcPawn::DoSteering(float SteeringValue)
{
	ChaosVehicleMovement->SetSteeringInput(SteeringValue);
	if (DataLogger) DataLogger->SetSteeringInput(SteeringValue);
}

void ADigitalTwinNbcPawn::DoThrottle(float ThrottleValue)
{
	ChaosVehicleMovement->SetThrottleInput(ThrottleValue);
	ChaosVehicleMovement->SetBrakeInput(0.0f);
	if (DataLogger) DataLogger->SetThrottleInput(ThrottleValue);
}

void ADigitalTwinNbcPawn::DoBrake(float BrakeValue)
{
	ChaosVehicleMovement->SetBrakeInput(BrakeValue);
	ChaosVehicleMovement->SetThrottleInput(0.0f);
}

void ADigitalTwinNbcPawn::DoBrakeStart()
{
	BrakeLights(true);
}

void ADigitalTwinNbcPawn::DoBrakeStop()
{
	BrakeLights(false);
	ChaosVehicleMovement->SetBrakeInput(0.0f);
}

void ADigitalTwinNbcPawn::DoHandbrakeStart()
{
	ChaosVehicleMovement->SetHandbrakeInput(true);
	BrakeLights(true);
}

void ADigitalTwinNbcPawn::DoHandbrakeStop()
{
	ChaosVehicleMovement->SetHandbrakeInput(false);
	BrakeLights(false);
}

void ADigitalTwinNbcPawn::DoLookAround(float YawDelta)
{
	BackSpringArm->AddLocalRotation(FRotator(0.0f, YawDelta, 0.0f));
}

void ADigitalTwinNbcPawn::DoToggleCamera()
{
	bFrontCameraActive = !bFrontCameraActive;

	FrontCamera->SetActive(bFrontCameraActive);
	BackCamera->SetActive(!bFrontCameraActive);
}

void ADigitalTwinNbcPawn::DoResetVehicle()
{
	FVector ResetLocation = GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
	FRotator ResetRotation = GetActorRotation();
	ResetRotation.Pitch = 0.0f;
	ResetRotation.Roll = 0.0f;

	SetActorTransform(FTransform(ResetRotation, ResetLocation, FVector::OneVector), false, nullptr, ETeleportType::TeleportPhysics);

	GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
}

void ADigitalTwinNbcPawn::FlippedCheck()
{
	const float UpDot = FVector::DotProduct(FVector::UpVector, GetMesh()->GetUpVector());

	if (UpDot < FlipCheckMinDot)
	{
		if (bPreviousFlipCheck)
			DoResetVehicle();
		
		bPreviousFlipCheck = true;
	}
	else
	{
		bPreviousFlipCheck = false;
	}
}

void ADigitalTwinNbcPawn::DoToggleSensorView()
{
	ADigitalTwinNbcPlayerController* PC = Cast<ADigitalTwinNbcPlayerController>(GetController());
	if (PC == nullptr)
		return;
	
	UTextureRenderTarget2D* CamRT = CameraSensor ? CameraSensor->GetRenderTarget() : nullptr;
	PC->ToggleSensorView(CamRT);
}

void ADigitalTwinNbcPawn::DoToggleLidarView()
{
	ADigitalTwinNbcPlayerController* PC = Cast<ADigitalTwinNbcPlayerController>(GetController());
	if (PC == nullptr)
		return;
		
	UTexture2D* LidarRT = LidarSensor ? LidarSensor->GetBevRenderTarget() : nullptr;
	PC->ToggleLidarView(LidarRT);

	if (LidarSensor)
	{
		if (PC->IsLidarViewVisible())
			LidarSensor->StartScan();
		else
			LidarSensor->StopScan();
	}
}