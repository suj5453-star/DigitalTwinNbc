// Copyright NBC, Inc. All Rights Reserved.


#include "System/DigitalTwinNbcPlayerController.h"
#include "DigitalTwinNbcPawn.h"
#include "Sensor/SensorViewWidget.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"

void ADigitalTwinNbcPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	bAttachToPawn = true;

	if (SensorViewWidgetClass)
	{
		SensorViewWidget = CreateWidget<USensorViewWidget>(this, SensorViewWidgetClass);
		if (SensorViewWidget)
		{
			SensorViewWidget->AddToViewport(10);
			SensorViewWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}
}

void ADigitalTwinNbcPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}
		}
	}
}

void ADigitalTwinNbcPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	VehiclePawn = CastChecked<ADigitalTwinNbcPawn>(InPawn);
	VehiclePawn->OnDestroyed.AddDynamic(this, &ADigitalTwinNbcPlayerController::OnPawnDestroyed);
}

void ADigitalTwinNbcPlayerController::OnPawnDestroyed(AActor* DestroyedPawn)
{
	TArray<AActor*> ActorList;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), ActorList);

	if (ActorList.Num() > 0)
	{
		const FTransform SpawnTransform = ActorList[0]->GetActorTransform();

		if (ADigitalTwinNbcPawn* RespawnedVehicle = GetWorld()->SpawnActor<ADigitalTwinNbcPawn>(VehiclePawnClass, SpawnTransform))
		{
			Possess(RespawnedVehicle);
		}
	}
}

void ADigitalTwinNbcPlayerController::ToggleSensorView(UTextureRenderTarget2D* InCameraRT)
{
	if (!SensorViewWidget) return;

	if (InCameraRT)
	{
		SensorViewWidget->SetRenderTarget(InCameraRT);
	}
	SensorViewWidget->ToggleCameraView();
}

void ADigitalTwinNbcPlayerController::ToggleLidarView(UTexture2D* InLidarRT)
{
	if (!SensorViewWidget) return;

	if (InLidarRT)
	{
		SensorViewWidget->SetLidarRenderTarget(InLidarRT);
	}
	SensorViewWidget->ToggleLidarView();
}

bool ADigitalTwinNbcPlayerController::IsLidarViewVisible() const
{
	return SensorViewWidget && SensorViewWidget->IsLidarViewVisible();
}
