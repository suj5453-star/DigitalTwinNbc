// Copyright NBC, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "DigitalTwinNbcPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputAction;
class UChaosWheeledVehicleMovementComponent;
class UCameraSensorComponent;
class ULidarSensorComponent;
class USplineFollowerComponent;
class UAgentDataLogger;
struct FInputActionValue;

UCLASS(abstract)
class DIGITALTWINNBC_API ADigitalTwinNbcPawn : public AWheeledVehiclePawn
{
	GENERATED_BODY()
public:
	ADigitalTwinNbcPawn();
	
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoSteering(float SteeringValue);

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoThrottle(float ThrottleValue);
	
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoBrake(float BrakeValue);

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoBrakeStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoBrakeStop();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoHandbrakeStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoHandbrakeStop();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoLookAround(float YawDelta);
	
	FORCEINLINE USpringArmComponent* GetFrontSpringArm() const { return FrontSpringArm; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FrontCamera; }
	FORCEINLINE USpringArmComponent* GetBackSpringArm() const { return BackSpringArm; }
	FORCEINLINE UCameraComponent* GetBackCamera() const { return BackCamera; }
	FORCEINLINE UChaosWheeledVehicleMovementComponent* GetChaosVehicleMovement() const { return ChaosVehicleMovement; }
	FORCEINLINE UCameraSensorComponent* GetCameraSensor() const { return CameraSensor; }
	FORCEINLINE ULidarSensorComponent* GetLidarSensor() const { return LidarSensor; }
	FORCEINLINE UAgentDataLogger* GetDataLogger() const { return DataLogger; }
	
protected:
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float Delta) override;

private:
	void LookAround(const FInputActionValue& Value);
	void ToggleCamera(const FInputActionValue& Value);
	void ResetVehicle(const FInputActionValue& Value);
	void ToggleSensorView(const FInputActionValue& Value);
	void ToggleLidarView(const FInputActionValue& Value);

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoToggleCamera();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoResetVehicle();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoToggleSensorView();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoToggleLidarView();

protected:

	UFUNCTION(BlueprintImplementableEvent, Category="Vehicle")
	void BrakeLights(bool bBraking);

	UFUNCTION()
	void FlippedCheck();
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> FrontSpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FrontCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> BackSpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> BackCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraSensorComponent> CameraSensor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULidarSensorComponent> LidarSensor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAgentDataLogger> DataLogger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USplineFollowerComponent> SplineFollower;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UChaosWheeledVehicleMovementComponent> ChaosVehicleMovement;
	
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> ResetVehicleAction;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> ToggleCameraViewAction;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> ToggleLidarViewAction;

	UPROPERTY(EditAnywhere, Category="Flip Check", meta = (Units = "s"))
	float FlipCheckTime = 3.0f;

	UPROPERTY(EditAnywhere, Category="Flip Check")
	float FlipCheckMinDot = -0.2f;

private:
	bool bFrontCameraActive = false;
	bool bPreviousFlipCheck = false;

	/** Flip check timer */
	FTimerHandle FlipCheckTimer;
};
