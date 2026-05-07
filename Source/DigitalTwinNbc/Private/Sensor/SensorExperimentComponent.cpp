// Copyright NBC, Inc. All Rights Reserved.

#include "Sensor/SensorExperimentComponent.h"

#include "Evaluation/DrivingPerformanceEvaluator.h"
#include "GameFramework/Actor.h"
#include "Sensor/CameraSensorComponent.h"
#include "Sensor/LidarSensorComponent.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogSensorExperimentComponent, Log, All);

USensorExperimentComponent::USensorExperimentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USensorExperimentComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveComponents();

	if (bAutoStart)
	{
		StartExperimentSequence();
	}
}

void USensorExperimentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopExperimentSequence();

	Super::EndPlay(EndPlayReason);
}

void USensorExperimentComponent::ResolveComponents()
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

	if (!DrivingEvaluator)
	{
		DrivingEvaluator = Owner->FindComponentByClass<UDrivingPerformanceEvaluator>();
	}
}

void USensorExperimentComponent::StartExperimentSequence()
{
	if (bRunning)
	{
		return;
	}

	ResolveComponents();

	CurrentExperimentIndex = 0;
	bRunning = true;

	StartCurrentExperiment();
}

void USensorExperimentComponent::StopExperimentSequence()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExperimentTimerHandle);
	}

	if (LidarSensor)
	{
		LidarSensor->StopScan();
	}

	bRunning = false;
}

FName USensorExperimentComponent::GetCurrentRowName() const
{
	if (ExperimentPlan.ExperimentRows.IsValidIndex(CurrentExperimentIndex))
	{
		return ExperimentPlan.ExperimentRows[CurrentExperimentIndex];
	}

	return ExperimentRowName;
}

const FSensorExperimentPresetRow* USensorExperimentComponent::FindRow(FName RowName) const
{
	if (!ExperimentPresetTable || RowName == NAME_None)
	{
		return nullptr;
	}

	return ExperimentPresetTable->FindRow<FSensorExperimentPresetRow>(
		RowName,
		TEXT("SensorExperimentComponent")
	);
}

void USensorExperimentComponent::StartCurrentExperiment()
{
	const FName RowName = GetCurrentRowName();

	if (RowName == NAME_None)
	{
		UE_LOG(
			LogSensorExperimentComponent,
			Warning,
			TEXT("[%s] Experiment row name is None."),
			*GetNameSafe(GetOwner())
		);

		bRunning = false;
		return;
	}

	const FSensorExperimentPresetRow* Row = FindRow(RowName);
	if (!Row)
	{
		UE_LOG(
			LogSensorExperimentComponent,
			Warning,
			TEXT("[%s] Row not found: %s"),
			*GetNameSafe(GetOwner()),
			*RowName.ToString()
		);

		bRunning = false;
		return;
	}

	if (!ApplyExperimentByRowName(RowName))
	{
		bRunning = false;
		return;
	}

	if (LidarSensor)
	{
		LidarSensor->StartScan();
	}

	const float Duration = FMath::Max(DefaultEvaluationDurationSeconds, 0.1f);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ExperimentTimerHandle,
			this,
			&USensorExperimentComponent::FinishCurrentExperiment,
			Duration,
			false
		);
	}

	UE_LOG(
		LogSensorExperimentComponent,
		Log,
		TEXT("[%s] Started experiment: %s"),
		*GetNameSafe(GetOwner()),
		*RowName.ToString()
	);
}

void USensorExperimentComponent::FinishCurrentExperiment()
{
	if (LidarSensor)
	{
		LidarSensor->StopScan();
	}

	const bool bHasPlan = ExperimentPlan.ExperimentRows.Num() > 0;
	if (!bHasPlan)
	{
		bRunning = false;
		return;
	}

	++CurrentExperimentIndex;

	if (CurrentExperimentIndex >= ExperimentPlan.ExperimentRows.Num())
	{
		if (!ExperimentPlan.bLoop)
		{
			bRunning = false;

			UE_LOG(
				LogSensorExperimentComponent,
				Log,
				TEXT("[%s] Experiment sequence finished."),
				*GetNameSafe(GetOwner())
			);

			return;
		}

		CurrentExperimentIndex = 0;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ExperimentTimerHandle,
			this,
			&USensorExperimentComponent::StartCurrentExperiment,
			FMath::Max(ExperimentPlan.DelayBetweenExperiments, 0.0f),
			false
		);
	}
}

bool USensorExperimentComponent::ApplyExperimentByRowName(FName InRowName)
{
	if (bApplyOnlyOnce && bApplied && ActiveExperimentRowName == InRowName)
	{
		return true;
	}

	ResolveComponents();

	const FSensorExperimentPresetRow* Row = FindRow(InRowName);
	if (!Row)
	{
		UE_LOG(
			LogSensorExperimentComponent,
			Warning,
			TEXT("[%s] Experiment row not found or table invalid: %s"),
			*GetNameSafe(GetOwner()),
			*InRowName.ToString()
		);

		return false;
	}

	if (CameraSensor)
	{
		CameraSensor->ApplyPreset(Row->CameraPreset);

		if (Row->bOverrideCameraDistortion)
		{
			CameraSensor->SetDistortionParams(Row->CameraDistortionOverride);
		}

		CameraSensor->RefreshSettings();
	}
	else
	{
		UE_LOG(
			LogSensorExperimentComponent,
			Warning,
			TEXT("[%s] CameraSensor not found."),
			*GetNameSafe(GetOwner())
		);
	}

	if (LidarSensor)
	{
		LidarSensor->ApplyPreset(Row->LidarPreset);
		LidarSensor->SetRangeNoiseStdDev(Row->LidarRangeNoiseStdDev);
		LidarSensor->SetObstacleDistanceThreshold(Row->ObstacleDistanceThreshold);
		LidarSensor->RefreshSettings();
	}
	else
	{
		UE_LOG(
			LogSensorExperimentComponent,
			Warning,
			TEXT("[%s] LidarSensor not found."),
			*GetNameSafe(GetOwner())
		);
	}

	if (DrivingEvaluator)
	{
		DrivingEvaluator->ApplyEvaluationPreset(
			Row->SensorScoreWeights,
			Row->SensorScoreLimits,
			Row->CameraPreset,
			Row->LidarRangeNoiseStdDev,
			InRowName
		);
	}
	else
	{
		UE_LOG(
			LogSensorExperimentComponent,
			Warning,
			TEXT("[%s] DrivingEvaluator not found."),
			*GetNameSafe(GetOwner())
		);
	}

	ActiveExperimentRowName = InRowName;
	bApplied = true;

	UE_LOG(
		LogSensorExperimentComponent,
		Log,
		TEXT("[%s] Applied experiment row: %s"),
		*GetNameSafe(GetOwner()),
		*InRowName.ToString()
	);

	return true;
}

FString USensorExperimentComponent::BuildCsvHeaderColumns() const
{
	return FString(
		TEXT(",Vehicle")
		TEXT(",ExperimentRow")
		TEXT(",ExperimentLabel")
		TEXT(",CameraPreset")
		TEXT(",LidarPreset")
		TEXT(",LidarNoiseStdDev")
		TEXT(",ObstacleThresholdCm")
		TEXT(",ClosestObstacleDistanceCm")
		TEXT(",ObstaclePointCount")
		TEXT(",ExpectedObstaclePointCount")
		TEXT(",SensorSyncDeltaSeconds")
		TEXT(",bCameraLidarSynced")
		TEXT(",LidarObstacleScore")
		TEXT(",LidarNoiseScore")
		TEXT(",CameraPresetScore")
		TEXT(",SensorSyncScore")
		TEXT(",TotalScore")
	);
}

FString USensorExperimentComponent::BuildCsvRowColumns() const
{
	const FSensorExperimentPresetRow* Row = FindRow(ActiveExperimentRowName);

	FString ExperimentLabel = TEXT("Experiment");
	ECameraSensorPreset CurrentCameraPreset = ECameraSensorPreset::TeslaHW3_Wide;
	ELidarSensorPreset CurrentLidarPreset = ELidarSensorPreset::VelodyneVLP16;
	float LidarNoiseStdDev = 0.0f;
	float ObstacleThresholdCm = 0.0f;

	if (Row)
	{
		ExperimentLabel = Row->Description.ToString();
		CurrentCameraPreset = Row->CameraPreset;
		CurrentLidarPreset = Row->LidarPreset;
		LidarNoiseStdDev = Row->LidarRangeNoiseStdDev;
		ObstacleThresholdCm = Row->ObstacleDistanceThreshold;
	}

	const FString CameraPresetName = GetEnumDisplayName(
		TEXT("/Script/DigitalTwinNbc.ECameraSensorPreset"),
		static_cast<int64>(CurrentCameraPreset)
	);

	const FString LidarPresetName = GetEnumDisplayName(
		TEXT("/Script/DigitalTwinNbc.ELidarSensorPreset"),
		static_cast<int64>(CurrentLidarPreset)
	);

	float ClosestObstacleDistance = 0.0f;
	int32 ObstaclePointCount = 0;
	int32 ExpectedObstaclePointCount = 0;
	float SensorSyncDeltaSeconds = 0.0f;
	int32 bCameraLidarSynced = 0;

	float LidarObstacleScore = 0.0f;
	float LidarNoiseScore = 0.0f;
	float CameraPresetScore = 0.0f;
	float SensorSyncScore = 0.0f;
	float TotalScore = 0.0f;

	if (DrivingEvaluator)
	{
		const FSensorEvaluationInput& Input = DrivingEvaluator->GetLatestInput();
		const FSensorEvaluationScore& Score = DrivingEvaluator->GetLatestScore();

		ClosestObstacleDistance = Input.ClosestObstacleDistance;
		ObstaclePointCount = Input.ObstaclePointCount;
		ExpectedObstaclePointCount = Input.ExpectedObstaclePointCount;
		SensorSyncDeltaSeconds = Input.SensorSyncDeltaSeconds;
		bCameraLidarSynced = Input.bCameraLidarSynced ? 1 : 0;

		LidarObstacleScore = Score.LidarObstacleDetectionScore;
		LidarNoiseScore = Score.LidarNoiseStabilityScore;
		CameraPresetScore = Score.CameraPresetQualityScore;
		SensorSyncScore = Score.SensorSyncScore;
		TotalScore = Score.TotalScore;
	}

	return FString::Printf(
		TEXT(",%s,%s,%s,%s,%s,%.3f,%.3f")
		TEXT(",%.3f,%d,%d,%.5f,%d")
		TEXT(",%.3f,%.3f,%.3f,%.3f,%.3f"),
		*EscapeCsvText(MakeVehicleLabel()),
		*EscapeCsvText(ActiveExperimentRowName.ToString()),
		*EscapeCsvText(ExperimentLabel),
		*EscapeCsvText(CameraPresetName),
		*EscapeCsvText(LidarPresetName),
		LidarNoiseStdDev,
		ObstacleThresholdCm,
		ClosestObstacleDistance,
		ObstaclePointCount,
		ExpectedObstaclePointCount,
		SensorSyncDeltaSeconds,
		bCameraLidarSynced,
		LidarObstacleScore,
		LidarNoiseScore,
		CameraPresetScore,
		SensorSyncScore,
		TotalScore
	);
}

FString USensorExperimentComponent::MakeVehicleLabel() const
{
	if (!VehicleLabel.IsEmpty())
	{
		return VehicleLabel;
	}

	return GetNameSafe(GetOwner());
}

FString USensorExperimentComponent::GetEnumDisplayName(const TCHAR* EnumPath, int64 Value) const
{
	const UEnum* Enum = FindObject<UEnum>(nullptr, EnumPath);
	if (!Enum)
	{
		return FString::FromInt(Value);
	}

	return Enum->GetDisplayNameTextByValue(Value).ToString();
}

FString USensorExperimentComponent::EscapeCsvText(const FString& InText) const
{
	FString Result = InText;
	Result.ReplaceInline(TEXT("\""), TEXT("\"\""));

	if (Result.Contains(TEXT(",")) || Result.Contains(TEXT("\"")) || Result.Contains(TEXT("\n")))
	{
		Result = FString::Printf(TEXT("\"%s\""), *Result);
	}

	return Result;
}