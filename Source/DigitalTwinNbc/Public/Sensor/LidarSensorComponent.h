// Copyright NBC, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "CameraSensorTypes.h"
#include "LidarSensorComponent.generated.h"

class ULidarBevRenderer;
class UTexture2D;

UCLASS(ClassGroup = (Sensor), meta = (BlueprintSpawnableComponent), BlueprintType)
class DIGITALTWINNBC_API ULidarSensorComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	ULidarSensorComponent();

	UFUNCTION(BlueprintCallable, Category = "LidarSensor")
	void StartScan();

	UFUNCTION(BlueprintCallable, Category = "LidarSensor")
	void StopScan();

	UFUNCTION(BlueprintCallable, Category = "LidarSensor")
	void ApplyPreset(ELidarSensorPreset NewPreset);

	UFUNCTION(BlueprintCallable, Category = "LidarSensor")
	void SetScanRate(float Hz);

	UFUNCTION(BlueprintCallable, Category = "LidarSensor")
	void RefreshSettings();

	UFUNCTION(BlueprintCallable, Category = "LidarSensor")
	void SetRangeNoiseStdDev(float InNoiseStdDev);

	UFUNCTION(BlueprintCallable, Category = "LidarSensor")
	void SetObstacleDistanceThreshold(float InThresholdCm);

	UFUNCTION(BlueprintPure, Category = "LidarSensor")
	UTexture2D* GetBevRenderTarget() const;

	UFUNCTION(BlueprintPure, Category = "LidarSensor")
	const FLidarPointCloudData& GetPointCloud() const { return LastPointCloud; }

	UFUNCTION(BlueprintPure, Category = "LidarSensor")
	double GetLastScanTimestamp() const { return LastScanTimestamp; }

	const FLidarPointCloudData& GetLastPointCloud() const { return LastPointCloud; }
	int32 GetObstaclePointCount() const;
	float GetClosestObstacleDistanceCm() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void InitializeSensor();
	void StartScanTimer();
	void StopScanTimer();

	void OnScanTimer();
	void FireAsyncTraces();
	void CollectAsyncResults();
	void SavePointCloudData();

	void RebuildDirectionCache();
	void ApplyDefaultElevationAngles();
	void ReserveBuffers(int32 InReserveCount);

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LidarSensor|Config", meta = (AllowPrivateAccess = "true"))
	ELidarSensorPreset Preset = ELidarSensorPreset::VelodyneVLP16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LidarSensor|Config", meta = (AllowPrivateAccess = "true"))
	bool bSensorEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LidarSensor|Config", meta = (AllowPrivateAccess = "true"))
	FLidarSensorConfig Config;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LidarSensor|Performance", meta = (ClampMin = "1", AllowPrivateAccess = "true"))
	int32 MaxTracesPerScan = 2048;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LidarSensor|BEV", meta = (AllowPrivateAccess = "true"))
	FBevRenderConfig BevConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LidarSensor|BEV", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ObstacleDistanceThreshold = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LidarSensor|DataSave", meta = (AllowPrivateAccess = "true"))
	bool bIsDataSaving = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LidarSensor|DataSave", meta = (AllowPrivateAccess = "true"))
	FSensorDataSaveConfig DataSaveConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LidarSensor|Output", meta = (AllowPrivateAccess = "true"))
	FLidarPointCloudData LastPointCloud;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LidarSensor|Output", meta = (AllowPrivateAccess = "true"))
	int64 FrameCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LidarSensor|Output", meta = (AllowPrivateAccess = "true"))
	double LastScanTimestamp = 0.0;

	UPROPERTY()
	TObjectPtr<ULidarBevRenderer> BevRenderer;

private:
	FTimerHandle ScanTimerHandle;

	TArray<FTraceHandle> PendingHandles;
	TArray<FVector> PendingWorldDirs;
	FTransform PendingTransform;

	bool bHasPendingTraces = false;
	uint64 FireFrameNumber = 0;

	TArray<FVector> CachedLocalDirections;
	bool bDirectionsDirty = true;
	int32 NextRayIndex = 0;

	TArray<FVector> ScanPoints;
	TArray<float> ScanIntensities;
	TArray<uint8> ScanObstacleFlags;
};