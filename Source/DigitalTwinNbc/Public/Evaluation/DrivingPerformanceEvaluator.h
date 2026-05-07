// Copyright NBC, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DrivingEvaluationTypes.h"
#include "DrivingPerformanceEvaluator.generated.h"

class UCameraSensorComponent;
class ULidarSensorComponent;
class USensorSyncComponent;

UCLASS(ClassGroup = (Evaluation), meta = (BlueprintSpawnableComponent), BlueprintType)
class DIGITALTWINNBC_API UDrivingPerformanceEvaluator : public UActorComponent
{
	GENERATED_BODY()

public:
	UDrivingPerformanceEvaluator();

	UFUNCTION(BlueprintCallable, Category = "Sensor Evaluation")
	void ApplyEvaluationPreset(
		const FSensorEvaluationWeights& InWeights,
		const FSensorEvaluationLimits& InLimits,
		ECameraSensorPreset InCameraPreset,
		float InLidarNoiseStdDev,
		FName InExperimentRow
	);

	UFUNCTION(BlueprintCallable, Category = "Sensor Evaluation")
	void SetEvaluationInput(const FSensorEvaluationInput& InInput);

	UFUNCTION(BlueprintCallable, Category = "Sensor Evaluation")
	FSensorEvaluationScore EvaluateNow();

	UFUNCTION(BlueprintCallable, Category = "Sensor Evaluation")
	void ResetEpisode();

	UFUNCTION(BlueprintPure, Category = "Sensor Evaluation")
	const FSensorEvaluationScore& GetLatestScore() const { return LatestScore; }

	UFUNCTION(BlueprintPure, Category = "Sensor Evaluation")
	const FSensorEvaluationInput& GetLatestInput() const { return LatestInput; }

	UFUNCTION(BlueprintPure, Category = "Sensor Evaluation")
	FName GetActiveExperimentRow() const { return ActiveExperimentRow; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction
	) override;

private:
	void AutoResolveComponents();
	void UpdateEvaluationInputFromRuntime();

	float ScoreLidarObstacleDetection(const FSensorEvaluationInput& Input) const;
	float ScoreLidarNoiseStability(const FSensorEvaluationInput& Input) const;
	float ScoreCameraPresetQuality(const FSensorEvaluationInput& Input) const;
	float ScoreSensorSync(const FSensorEvaluationInput& Input) const;
	float CalcWeightedTotal(const FSensorEvaluationScore& Score) const;

	static float ClampScore(float Value);

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Runtime", meta = (AllowPrivateAccess = "true"))
	bool bEvaluateEveryTick = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Runtime", meta = (ClampMin = "0.1", ClampMax = "100.0", Units = "Hz", AllowPrivateAccess = "true"))
	float EvaluationFrequencyHz = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Sensors", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraSensorComponent> CameraSensor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Sensors", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULidarSensorComponent> LidarSensor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Sensors", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USensorSyncComponent> SensorSync;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Score", meta = (AllowPrivateAccess = "true"))
	FSensorEvaluationWeights Weights;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Evaluation|Score", meta = (AllowPrivateAccess = "true"))
	FSensorEvaluationLimits Limits;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensor Evaluation|Score", meta = (AllowPrivateAccess = "true"))
	FSensorEvaluationInput LatestInput;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensor Evaluation|Score", meta = (AllowPrivateAccess = "true"))
	FSensorEvaluationScore LatestScore;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensor Evaluation|Preset", meta = (AllowPrivateAccess = "true"))
	FName ActiveExperimentRow = NAME_None;

	ECameraSensorPreset ActiveCameraPreset = ECameraSensorPreset::TeslaHW3_Wide;

	float ActiveLidarNoiseStdDev = 0.0f;
	float TimeSinceLastEvaluation = 0.0f;
};