// Copyright NBC, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "Sensor/CameraSensorTypes.h"
#include "Evaluation/DrivingEvaluationTypes.h"
#include "SensorExperimentComponent.generated.h"

class UCameraSensorComponent;
class ULidarSensorComponent;
class UDrivingPerformanceEvaluator;

USTRUCT(BlueprintType)
struct FVehicleExperimentPlan
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Experiment")
	TArray<FName> ExperimentRows;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Experiment")
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Experiment", meta = (ClampMin = "0.0"))
	float DelayBetweenExperiments = 2.0f;
};

UCLASS(ClassGroup = (Sensor), meta = (BlueprintSpawnableComponent))
class DIGITALTWINNBC_API USensorExperimentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USensorExperimentComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	UFUNCTION(BlueprintCallable, Category = "Sensor Experiment")
	void StartExperimentSequence();

	UFUNCTION(BlueprintCallable, Category = "Sensor Experiment")
	void StopExperimentSequence();

	UFUNCTION(BlueprintCallable, Category = "Sensor Experiment")
	bool ApplyExperimentByRowName(FName InRowName);

	UFUNCTION(BlueprintPure, Category = "Sensor Experiment")
	FName GetActiveExperimentRowName() const { return ActiveExperimentRowName; }

	// AgentDataLogger가 CSV Header 생성 시 호출해서 센서 실험 컬럼을 붙입니다.
	FString BuildCsvHeaderColumns() const;

	// AgentDataLogger가 CSV Row 생성 시 호출해서 센서 실험 값을 붙입니다.
	FString BuildCsvRowColumns() const;

private:
	void ResolveComponents();
	void StartCurrentExperiment();
	void FinishCurrentExperiment();

	FName GetCurrentRowName() const;
	const FSensorExperimentPresetRow* FindRow(FName RowName) const;
	FString MakeVehicleLabel() const;

	FString GetEnumDisplayName(const TCHAR* EnumPath, int64 Value) const;
	FString EscapeCsvText(const FString& InText) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Experiment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDataTable> ExperimentPresetTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Experiment", meta = (AllowPrivateAccess = "true"))
	FName ExperimentRowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Experiment", meta = (AllowPrivateAccess = "true"))
	FString VehicleLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Experiment", meta = (AllowPrivateAccess = "true"))
	bool bAutoStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Experiment", meta = (AllowPrivateAccess = "true"))
	bool bApplyOnlyOnce = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Experiment", meta = (AllowPrivateAccess = "true"))
	FVehicleExperimentPlan ExperimentPlan;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensor Experiment", meta = (AllowPrivateAccess = "true"))
	FName ActiveExperimentRowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensor Experiment", meta = (ClampMin = "0.1", AllowPrivateAccess = "true"))
	float DefaultEvaluationDurationSeconds = 30.0f;

	UPROPERTY()
	TObjectPtr<UCameraSensorComponent> CameraSensor;

	UPROPERTY()
	TObjectPtr<ULidarSensorComponent> LidarSensor;

	UPROPERTY()
	TObjectPtr<UDrivingPerformanceEvaluator> DrivingEvaluator;

	FTimerHandle ExperimentTimerHandle;

	int32 CurrentExperimentIndex = 0;
	bool bRunning = false;
	bool bApplied = false;
};