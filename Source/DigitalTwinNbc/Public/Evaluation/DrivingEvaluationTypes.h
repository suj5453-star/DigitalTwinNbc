// Copyright NBC, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Sensor/CameraSensorTypes.h"
#include "DrivingEvaluationTypes.generated.h"

USTRUCT(BlueprintType)
struct FSensorEvaluationWeights
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Weights", meta = (ClampMin = "0.0"))
	float LidarObstacleDetection = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Weights", meta = (ClampMin = "0.0"))
	float LidarNoiseStability = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Weights", meta = (ClampMin = "0.0"))
	float CameraPresetQuality = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Weights", meta = (ClampMin = "0.0"))
	float SensorSync = 1.0f;

	float GetTotalWeight() const
	{
		return FMath::Max(
			LidarObstacleDetection +
			LidarNoiseStability +
			CameraPresetQuality +
			SensorSync,
			KINDA_SMALL_NUMBER
		);
	}
};

USTRUCT(BlueprintType)
struct FSensorEvaluationLimits
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Limits", meta = (ClampMin = "0.0", Units = "cm"))
	float SafeObstacleDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Limits", meta = (ClampMin = "1"))
	int32 ExpectedObstaclePointCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Limits", meta = (ClampMin = "0.0", ClampMax = "50.0"))
	float MaxAcceptableLidarNoiseStdDev = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Limits", meta = (ClampMin = "0.0", Units = "s"))
	float MaxSyncDeltaSeconds = 0.03f;
};

USTRUCT(BlueprintType)
struct FSensorExperimentPresetRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Camera")
	ECameraSensorPreset CameraPreset = ECameraSensorPreset::TeslaHW3_Wide;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Camera")
	bool bOverrideCameraDistortion = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Camera", meta = (EditCondition = "bOverrideCameraDistortion"))
	FLensDistortionParams CameraDistortionOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Lidar")
	ELidarSensorPreset LidarPreset = ELidarSensorPreset::VelodyneVLP16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Lidar", meta = (ClampMin = "0.0", ClampMax = "50.0"))
	float LidarRangeNoiseStdDev = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|BEV", meta = (ClampMin = "0.0", Units = "cm"))
	float ObstacleDistanceThreshold = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Sync", meta = (ClampMin = "0.0", Units = "s"))
	float MaxSyncDeltaSeconds = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Evaluation")
	FSensorEvaluationWeights SensorScoreWeights;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experiment|Evaluation")
	FSensorEvaluationLimits SensorScoreLimits;
};

USTRUCT(BlueprintType)
struct FSensorEvaluationInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Input", meta = (Units = "cm"))
	float ClosestObstacleDistance = 100000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Input")
	int32 ObstaclePointCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Input")
	int32 ExpectedObstaclePointCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Input")
	float LidarRangeNoiseStdDev = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Input")
	ECameraSensorPreset CameraPreset = ECameraSensorPreset::TeslaHW3_Wide;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Input")
	bool bCameraLidarSynced = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Input", meta = (Units = "s"))
	float SensorSyncDeltaSeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct FSensorEvaluationScore
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensor Evaluation|Score")
	float LidarObstacleDetectionScore = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensor Evaluation|Score")
	float LidarNoiseStabilityScore = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensor Evaluation|Score")
	float CameraPresetQualityScore = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensor Evaluation|Score")
	float SensorSyncScore = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensor Evaluation|Score")
	float TotalScore = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensor Evaluation|Score")
	float Timestamp = 0.0f;
};