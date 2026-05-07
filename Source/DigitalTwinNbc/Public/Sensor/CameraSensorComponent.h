// Copyright NBC, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "CameraSensorTypes.h"
#include "CameraSensorComponent.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS(ClassGroup = (Sensor), meta = (BlueprintSpawnableComponent), BlueprintType)
class DIGITALTWINNBC_API UCameraSensorComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UCameraSensorComponent();

	UFUNCTION(BlueprintPure, Category = "CameraSensor")
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }
	
	UFUNCTION(BlueprintPure, Category = "CameraSensor")
	double GetLastCaptureTimestamp() const { return LastCaptureTimestamp; }

	UFUNCTION(BlueprintCallable, Category = "CameraSensor")
	void ApplyPreset(ECameraSensorPreset NewPreset);

	UFUNCTION(BlueprintCallable, Category = "CameraSensor")
	void SetCaptureRate(float Hz);

	UFUNCTION(BlueprintCallable, Category = "CameraSensor")
	void CaptureOnce();

	UFUNCTION(BlueprintCallable, Category = "CameraSensor")
	void RefreshSettings();

	UFUNCTION(BlueprintCallable, Category = "CameraSensor")
	void SetDistortionParams(const FLensDistortionParams& InDistortion);

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void InitializeCapture();
	void CreateRenderTarget();
	void ConfigureSceneCapture();
	void ApplyPostProcessSettings();
	void ApplyLensDistortion();
	void StartCaptureTimer();
	void StopCaptureTimer();
	void OnCaptureTimer();
	void SaveCameraImage();

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraSensor|Config", meta = (AllowPrivateAccess = "true"))
	ECameraSensorPreset Preset = ECameraSensorPreset::TeslaHW3_Wide;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraSensor|Config", meta = (AllowPrivateAccess = "true"))
	bool bSensorEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraSensor|Intrinsics", meta = (AllowPrivateAccess = "true"))
	FCameraSensorIntrinsics Intrinsics;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraSensor|Distortion", meta = (AllowPrivateAccess = "true"))
	FLensDistortionParams Distortion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraSensor|Noise", meta = (AllowPrivateAccess = "true"))
	FSensorNoiseParams Noise;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraSensor|PostProcess", meta = (AllowPrivateAccess = "true"))
	FCameraPostProcessEffects PostProcess;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraSensor|Exposure", meta = (AllowPrivateAccess = "true"))
	FAutoExposureParams Exposure;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraSensor|DataSave", meta = (AllowPrivateAccess = "true"))
	bool bIsDataSaving = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraSensor|DataSave", meta = (AllowPrivateAccess = "true"))
	FSensorDataSaveConfig DataSaveConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraSensor|Distortion", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> LensDistortionMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CameraSensor|Output", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CameraSensor|Output", meta = (AllowPrivateAccess = "true"))
	int64 FrameCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CameraSensor|Output", meta = (AllowPrivateAccess = "true"))
	double LastCaptureTimestamp = 0.0;

	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> SceneCapture;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DistortionMID;

	FTimerHandle CaptureTimerHandle;
};