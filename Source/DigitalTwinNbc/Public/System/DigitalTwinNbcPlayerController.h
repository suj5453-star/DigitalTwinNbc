// Copyright NBC, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DigitalTwinNbcPlayerController.generated.h"

class UInputMappingContext;
class ADigitalTwinNbcPawn;
class USensorViewWidget;
class UTextureRenderTarget2D;

UCLASS(abstract, Config="Game")
class DIGITALTWINNBC_API ADigitalTwinNbcPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;

	UFUNCTION()
	void OnPawnDestroyed(AActor* DestroyedPawn);

public:
	void ToggleSensorView(UTextureRenderTarget2D* InCameraRT);
	void ToggleLidarView(UTexture2D* InLidarRT);
	bool IsLidarViewVisible() const;

private:
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	UPROPERTY(EditAnywhere, Category = "Input|Steering Wheel Controls")
	bool bUseSteeringWheelControls = false;

	UPROPERTY(EditAnywhere, Category = "Input|Steering Wheel Controls", meta = (EditCondition = "bUseSteeringWheelControls"))
	UInputMappingContext* SteeringWheelInputMappingContext;

	UPROPERTY(EditAnywhere, Category="Vehicle|Respawn")
	TSubclassOf<ADigitalTwinNbcPawn> VehiclePawnClass;

	UPROPERTY(EditAnywhere, Category="Vehicle|UI")
	TSubclassOf<USensorViewWidget> SensorViewWidgetClass;

	UPROPERTY()
	TObjectPtr<ADigitalTwinNbcPawn> VehiclePawn;

	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	UPROPERTY()
	TObjectPtr<USensorViewWidget> SensorViewWidget;
};
