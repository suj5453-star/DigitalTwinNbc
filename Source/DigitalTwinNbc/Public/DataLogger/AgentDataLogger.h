// Copyright NBC, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AgentDataLogger.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DIGITALTWINNBC_API UAgentDataLogger : public UActorComponent
{
	GENERATED_BODY()

public:
	UAgentDataLogger();
	
	// 레이어 4 (SplineFollower) 에서 조향값 주입
	UFUNCTION(BlueprintCallable, Category="Data Logger")
	void SetSteeringInput(float InSteering) { CurrentSteeringInput = InSteering; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
private:
	static int32 GetUtmZone(double Longitude);
	static void LatLonToUtm(double Lat, double Lon, int32 Zone, double& OutEasting, double& OutNorthing);
	void WorldToUtm(const FVector& WorldLocation, double& OutEasting, double& OutNorthing) const;
	void CreateCsvFile();
	void AppendRow();

	// 속도 → 색상 변환
	FLinearColor SpeedToColor(float SpeedCmS) const;
	
	UFUNCTION(BlueprintCallable, Category="Data Logger")
	void StartRecording();

	UFUNCTION(BlueprintCallable, Category="Data Logger")
	void StopRecording();

	UFUNCTION(BlueprintPure, Category="Data Logger")
	bool IsRecording() const { return bIsRecording; }
	
private:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Data Logger",
		meta=(AllowPrivateAccess="true"))
	bool bEnableLogging = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Data Logger",
		meta=(ClampMin="0.1", ClampMax="100.0", Units="Hz", AllowPrivateAccess="true"))
	float SaveFrequencyHz = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Data Logger|UTM Reference",
		meta=(ClampMin="-90.0", ClampMax="90.0", Units="deg", AllowPrivateAccess="true"))
	double OriginLatitude = 36.4800;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Data Logger|UTM Reference",
		meta=(ClampMin="-180.0", ClampMax="180.0", Units="deg", AllowPrivateAccess="true"))
	double OriginLongitude = 127.0000;

private:
	// 시각화 설정
	// 궤적 선 두께 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Data Logger|Visualization",
		meta=(AllowPrivateAccess="true"))
	float TrailThickness = 3.f;
 
	// 급감속 판정 임계값(cm/s)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Data Logger|Visualization",
		meta=(AllowPrivateAccess="true"))
	float HardBrakeThreshold = 500.f;
 
	// 급감속 마커 크기(cm) 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Data Logger|Visualization",
		meta=(AllowPrivateAccess="true"))
	float BrakeMarkerSize = 40.f;
 
	// DrawDebugString 라벨 최소 이동 간격(cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Data Logger|Visualization",
		meta=(AllowPrivateAccess="true"))
	float LabelInterval = 3000.f;
 
	// 속도 색상 최솟값(cm/s) → 파랑 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Data Logger|Visualization",
		meta=(AllowPrivateAccess="true"))
	float SpeedColorMin = 0.f;
 
	// 속도 색상 최댓값(cm/s) → 빨강
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Data Logger|Visualization",
		meta=(AllowPrivateAccess="true"))
	float SpeedColorMax = 2000.f;
	
private:
	double OriginUtmEasting = 0.0;
	double OriginUtmNorthing = 0.0;
	int32 OriginUtmZone = 0;
	FString CsvFilePath;
	bool bIsRecording = false;
	float TimeSinceLastSave = 0.0f;
	float ElapsedRecordingTime = 0.0f;
	
	// 시각화용 상태 변수
	FVector PrevLocation = FVector::ZeroVector;
	float PrevSpeedCmS = 0.f;
	bool bHasFirstSample = false;
	FVector LastLabelLocation = FVector::ZeroVector;
	float CurrentSteeringInput = 0.f;
};
