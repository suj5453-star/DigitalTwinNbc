// Copyright NBC, Inc. All Rights Reserved.

#include "Evaluation/DrivingPerformanceEvaluator.h"

#include "GameFramework/Actor.h"
#include "Sensor/CameraSensorComponent.h"
#include "Sensor/LidarSensorComponent.h"
#include "Sensor/SensorSyncComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDrivingPerformanceEvaluator, Log, All);

UDrivingPerformanceEvaluator::UDrivingPerformanceEvaluator()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UDrivingPerformanceEvaluator::BeginPlay()
{
	Super::BeginPlay();
	AutoResolveComponents();
}

void UDrivingPerformanceEvaluator::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction
)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEvaluateEveryTick)
	{
		return;
	}

	TimeSinceLastEvaluation += DeltaTime;

	const float Interval = 1.0f / FMath::Max(EvaluationFrequencyHz, 0.1f);
	if (TimeSinceLastEvaluation < Interval)
	{
		return;
	}

	EvaluateNow();
	TimeSinceLastEvaluation -= Interval;
}

void UDrivingPerformanceEvaluator::AutoResolveComponents()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!CameraSensor)
	{
		CameraSensor = Owner->FindComponentByClass<UCameraSensorComponent>();
	}

	if (!LidarSensor)
	{
		LidarSensor = Owner->FindComponentByClass<ULidarSensorComponent>();
	}

	if (!SensorSync)
	{
		SensorSync = Owner->FindComponentByClass<USensorSyncComponent>();
	}
}

void UDrivingPerformanceEvaluator::ApplyEvaluationPreset(
	const FSensorEvaluationWeights& InWeights,
	const FSensorEvaluationLimits& InLimits,
	ECameraSensorPreset InCameraPreset,
	float InLidarNoiseStdDev,
	FName InExperimentRow
)
{
	Weights = InWeights;
	Limits = InLimits;
	ActiveCameraPreset = InCameraPreset;
	ActiveLidarNoiseStdDev = FMath::Max(InLidarNoiseStdDev, 0.0f);
	ActiveExperimentRow = InExperimentRow;

	ResetEpisode();

	UE_LOG(LogDrivingPerformanceEvaluator, Log,
		TEXT("Applied sensor evaluation preset: %s"),
		*InExperimentRow.ToString());
}

void UDrivingPerformanceEvaluator::SetEvaluationInput(const FSensorEvaluationInput& InInput)
{
	LatestInput = InInput;

	if (LatestInput.ExpectedObstaclePointCount <= 0)
	{
		LatestInput.ExpectedObstaclePointCount = FMath::Max(Limits.ExpectedObstaclePointCount, 1);
	}
}

FSensorEvaluationScore UDrivingPerformanceEvaluator::EvaluateNow()
{
	UpdateEvaluationInputFromRuntime();

	LatestScore.LidarObstacleDetectionScore = ScoreLidarObstacleDetection(LatestInput);
	LatestScore.LidarNoiseStabilityScore = ScoreLidarNoiseStability(LatestInput);
	LatestScore.CameraPresetQualityScore = ScoreCameraPresetQuality(LatestInput);
	LatestScore.SensorSyncScore = ScoreSensorSync(LatestInput);
	LatestScore.TotalScore = CalcWeightedTotal(LatestScore);
	LatestScore.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	return LatestScore;
}

void UDrivingPerformanceEvaluator::ResetEpisode()
{
	LatestInput = FSensorEvaluationInput();
	LatestInput.ExpectedObstaclePointCount = FMath::Max(Limits.ExpectedObstaclePointCount, 1);
	LatestInput.CameraPreset = ActiveCameraPreset;
	LatestInput.LidarRangeNoiseStdDev = ActiveLidarNoiseStdDev;

	LatestScore = FSensorEvaluationScore();
	TimeSinceLastEvaluation = 0.0f;
}

void UDrivingPerformanceEvaluator::UpdateEvaluationInputFromRuntime()
{
	AutoResolveComponents();

	LatestInput.ExpectedObstaclePointCount = FMath::Max(Limits.ExpectedObstaclePointCount, 1);
	LatestInput.CameraPreset = ActiveCameraPreset;
	LatestInput.LidarRangeNoiseStdDev = ActiveLidarNoiseStdDev;

	if (LidarSensor)
	{
		const int32 ObstacleCount = LidarSensor->GetObstaclePointCount();
		const float ClosestDistance = LidarSensor->GetClosestObstacleDistanceCm();

		LatestInput.ObstaclePointCount = ObstacleCount;
		LatestInput.ClosestObstacleDistance = ClosestDistance > 0.0f
			? ClosestDistance
			: 100000.0f;
	}

	if (SensorSync)
	{
		const FSensorSyncResult SyncResult = SensorSync->CheckSync();

		LatestInput.bCameraLidarSynced = SyncResult.bSynced;
		LatestInput.SensorSyncDeltaSeconds = static_cast<float>(SyncResult.DeltaSeconds);
	}
	else
	{
		LatestInput.bCameraLidarSynced = false;
		LatestInput.SensorSyncDeltaSeconds = Limits.MaxSyncDeltaSeconds;
	}
}

float UDrivingPerformanceEvaluator::ScoreLidarObstacleDetection(const FSensorEvaluationInput& Input) const
{
	const int32 ExpectedCount = FMath::Max(Input.ExpectedObstaclePointCount, 1);

	const float CountScore = FMath::Clamp(
		static_cast<float>(Input.ObstaclePointCount) / static_cast<float>(ExpectedCount),
		0.0f,
		1.0f
	);

	const float DistanceScore = FMath::Clamp(
		Input.ClosestObstacleDistance / FMath::Max(Limits.SafeObstacleDistance, 1.0f),
		0.0f,
		1.0f
	);

	const float CombinedScore = CountScore * 0.7f + DistanceScore * 0.3f;

	return ClampScore(CombinedScore * 100.0f);
}

float UDrivingPerformanceEvaluator::ScoreLidarNoiseStability(const FSensorEvaluationInput& Input) const
{
	const float MaxNoise = FMath::Max(Limits.MaxAcceptableLidarNoiseStdDev, 0.01f);
	const float NoiseRatio = FMath::Clamp(Input.LidarRangeNoiseStdDev / MaxNoise, 0.0f, 1.0f);

	return ClampScore((1.0f - NoiseRatio) * 100.0f);
}

float UDrivingPerformanceEvaluator::ScoreCameraPresetQuality(const FSensorEvaluationInput& Input) const
{
	switch (Input.CameraPreset)
	{
	case ECameraSensorPreset::TeslaHW4:
		return 100.0f;

	case ECameraSensorPreset::TeslaHW3_Wide:
		return 95.0f;

	case ECameraSensorPreset::TeslaHW3_Main:
		return 85.0f;

	case ECameraSensorPreset::Waymo:
		return 90.0f;

	case ECameraSensorPreset::TeslaHW3_Narrow:
		return 75.0f;

	case ECameraSensorPreset::DroneFPV:
		return 70.0f;

	case ECameraSensorPreset::Custom:
	default:
		return 80.0f;
	}
}

float UDrivingPerformanceEvaluator::ScoreSensorSync(const FSensorEvaluationInput& Input) const
{
	const float MaxDelta = FMath::Max(Limits.MaxSyncDeltaSeconds, 0.001f);
	const float DeltaRatio = FMath::Clamp(Input.SensorSyncDeltaSeconds / MaxDelta, 0.0f, 1.0f);

	return ClampScore((1.0f - DeltaRatio) * 100.0f);
}

float UDrivingPerformanceEvaluator::CalcWeightedTotal(const FSensorEvaluationScore& Score) const
{
	const float TotalWeight = Weights.GetTotalWeight();
	if (TotalWeight <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float WeightedSum =
		Score.LidarObstacleDetectionScore * Weights.LidarObstacleDetection +
		Score.LidarNoiseStabilityScore * Weights.LidarNoiseStability +
		Score.CameraPresetQualityScore * Weights.CameraPresetQuality +
		Score.SensorSyncScore * Weights.SensorSync;

	return ClampScore(WeightedSum / TotalWeight);
}

float UDrivingPerformanceEvaluator::ClampScore(float Value)
{
	return FMath::Clamp(Value, 0.0f, 100.0f);
}