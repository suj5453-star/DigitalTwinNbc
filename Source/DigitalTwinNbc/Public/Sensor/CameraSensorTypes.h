// Copyright NBC, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CameraSensorTypes.generated.h"

UENUM(BlueprintType)
enum class ELidarSensorPreset : uint8
{
	Custom           UMETA(DisplayName = "Custom"),
	VelodyneVLP16    UMETA(DisplayName = "Velodyne VLP-16"),
	VelodyneVLP32    UMETA(DisplayName = "Velodyne VLP-32C"),
	OusterOS1_64     UMETA(DisplayName = "Ouster OS1-64"),
	Livox_Mid360     UMETA(DisplayName = "Livox Mid-360")
};

USTRUCT(BlueprintType)
struct FLidarSensorConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LidarConfig", meta = (ClampMin = "1", ClampMax = "128"))
	int32 NumChannels = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LidarConfig", meta = (ClampMin = "10", ClampMax = "3600"))
	int32 PointsPerChannel = 450;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LidarConfig", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float RotationRate = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LidarConfig", meta = (ClampMin = "100.0"))
	float MaxRange = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LidarConfig", meta = (ClampMin = "0.0"))
	float MinRange = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LidarConfig", meta = (ClampMin = "-90.0", ClampMax = "90.0"))
	float VerticalFOVUpper = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LidarConfig", meta = (ClampMin = "-90.0", ClampMax = "90.0"))
	float VerticalFOVLower = -15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LidarConfig", meta = (ClampMin = "1.0", ClampMax = "360.0"))
	float HorizontalFOV = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LidarConfig", meta = (ClampMin = "0.0", ClampMax = "50.0"))
	float NoiseStdDev = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LidarConfig")
	TArray<float> ElevationAngles;

	int32 GetTotalPoints() const { return NumChannels * PointsPerChannel; }
};

USTRUCT(BlueprintType)
struct FLidarPointCloudData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PointCloud")
	TArray<FVector> Points;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PointCloud")
	TArray<float> Intensities;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PointCloud")
	TArray<uint8> ObstacleFlags;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PointCloud")
	int32 PointCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PointCloud")
	int64 FrameNumber = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PointCloud")
	double Timestamp = 0.0;

	void Reset()
	{
		Points.Reset();
		Intensities.Reset();
		ObstacleFlags.Reset();
		PointCount = 0;
		FrameNumber = 0;
		Timestamp = 0.0;
	}
};

USTRUCT(BlueprintType)
struct FBevRenderConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BEV", meta = (ClampMin = "64", ClampMax = "2048"))
	int32 ImageSize = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BEV", meta = (ClampMin = "100.0"))
	float ViewRange = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BEV")
	FLinearColor PointColor = FLinearColor(0.0f, 1.0f, 0.2f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BEV")
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BEV", meta = (ClampMin = "1.0", ClampMax = "8.0"))
	float PointSize = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BEV")
	bool bDrawObstacles = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BEV")
	FLinearColor ObstacleColor = FLinearColor(1.0f, 0.1f, 0.05f, 1.0f);
};

UENUM(BlueprintType)
enum class ECameraSensorPreset : uint8
{
	Custom           UMETA(DisplayName = "Custom"),
	TeslaHW3_Wide    UMETA(DisplayName = "Tesla HW3 Wide (120°)"),
	TeslaHW3_Main    UMETA(DisplayName = "Tesla HW3 Main (50°)"),
	TeslaHW3_Narrow  UMETA(DisplayName = "Tesla HW3 Narrow (35°)"),
	TeslaHW4         UMETA(DisplayName = "Tesla HW4 (5MP)"),
	DroneFPV         UMETA(DisplayName = "Drone FPV (150°)"),
	Waymo            UMETA(DisplayName = "Waymo 5th Gen")
};

USTRUCT(BlueprintType)
struct FCameraSensorIntrinsics
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intrinsics", meta = (ClampMin = "1", ClampMax = "7680"))
	int32 ImageWidth = 1280;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intrinsics", meta = (ClampMin = "1", ClampMax = "4320"))
	int32 ImageHeight = 960;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intrinsics", meta = (ClampMin = "1.0", ClampMax = "120.0"))
	float FrameRate = 36.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intrinsics", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float FOVDegrees = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intrinsics", meta = (ClampMin = "0.1"))
	float FocalLengthMM = 1.5f;
};

USTRUCT(BlueprintType)
struct FLensDistortionParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distortion")
	float K1 = -0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distortion")
	float K2 = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distortion")
	float K3 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distortion")
	float P1 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distortion")
	float P2 = 0.0f;

	bool HasDistortion() const
	{
		return !FMath::IsNearlyZero(K1) || !FMath::IsNearlyZero(K2) || !FMath::IsNearlyZero(K3)
			|| !FMath::IsNearlyZero(P1) || !FMath::IsNearlyZero(P2);
	}
};

USTRUCT(BlueprintType)
struct FSensorNoiseParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	bool bEnableNoise = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (EditCondition = "bEnableNoise"))
	float GaussianMean = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (EditCondition = "bEnableNoise", ClampMin = "0.0", ClampMax = "0.5"))
	float GaussianStdDev = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (EditCondition = "bEnableNoise", ClampMin = "0.0", ClampMax = "1.0"))
	float ShotNoiseIntensity = 0.01f;
};

USTRUCT(BlueprintType)
struct FCameraPostProcessEffects
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float VignetteIntensity = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float LensFlareIntensity = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float ChromaticAberration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MotionBlurAmount = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float BloomIntensity = 0.0f;
};

USTRUCT(BlueprintType)
struct FSensorDataSaveConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataSave")
	FString SensorLabel = TEXT("Sensor");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataSave", meta = (ClampMin = "1", ClampMax = "100"))
	int32 JPGQuality = 95;
};

USTRUCT(BlueprintType)
struct FAutoExposureParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure")
	bool bEnableAutoExposure = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure", meta = (EditCondition = "bEnableAutoExposure"))
	float MinEV = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure", meta = (EditCondition = "bEnableAutoExposure"))
	float MaxEV = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure", meta = (EditCondition = "bEnableAutoExposure", ClampMin = "0.1", ClampMax = "20.0"))
	float SpeedUp = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure", meta = (EditCondition = "bEnableAutoExposure", ClampMin = "0.1", ClampMax = "20.0"))
	float SpeedDown = 1.0f;
};
