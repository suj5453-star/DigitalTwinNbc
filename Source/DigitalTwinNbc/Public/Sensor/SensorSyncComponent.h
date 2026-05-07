// Copyright NBC, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SensorSyncComponent.generated.h"

class UCameraSensorComponent;
class ULidarSensorComponent;

USTRUCT(BlueprintType)
struct FSensorSyncResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SensorSync")
	bool bSynced = false;

	UPROPERTY(BlueprintReadOnly, Category = "SensorSync")
	double CameraTimestamp = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "SensorSync")
	double LidarTimestamp = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "SensorSync")
	double DeltaSeconds = 0.0;
};

UCLASS(ClassGroup = (Sensor), meta = (BlueprintSpawnableComponent), BlueprintType)
class DIGITALTWINNBC_API USensorSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USensorSyncComponent();

	UFUNCTION(BlueprintCallable, Category = "SensorSync")
	void InitializeSensors(UCameraSensorComponent* InCameraSensor, ULidarSensorComponent* InLidarSensor);

	UFUNCTION(BlueprintPure, Category = "SensorSync")
	FSensorSyncResult CheckSync() const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SensorSync", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float MaxSyncDeltaSeconds = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SensorSync", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraSensorComponent> CameraSensor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SensorSync", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULidarSensorComponent> LidarSensor;
};
