// Copyright NBC, Inc. All Rights Reserved.

#include "Sensor/CameraSensorComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/PlatformFileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCameraSensor, Log, All);

UCameraSensorComponent::UCameraSensorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SensorSceneCapture"));
	SceneCapture->SetupAttachment(this);
}

void UCameraSensorComponent::OnRegister()
{
	Super::OnRegister();

	if (SceneCapture && SceneCapture->IsRegistered() == false)
	{
		SceneCapture->SetupAttachment(this);
		SceneCapture->RegisterComponent();
	}
}

void UCameraSensorComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeCapture();
	ApplyPreset(Preset);
}

void UCameraSensorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopCaptureTimer();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void UCameraSensorComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UCameraSensorComponent, Preset))
	{
		ApplyPreset(Preset);
	}
}
#endif

void UCameraSensorComponent::InitializeCapture()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogCameraSensor, Error, TEXT("CameraSensorComponent has no owning actor."));
		return;
	}
	
	CreateRenderTarget();
	ConfigureSceneCapture();
	ApplyPostProcessSettings();
	ApplyLensDistortion();

	if (bSensorEnabled)
	{
		StartCaptureTimer();
	}

	UE_LOG(LogCameraSensor, Log, TEXT("CameraSensor initialized: %dx%d @ %.0f Hz, FOV %.0f°"),
		Intrinsics.ImageWidth, Intrinsics.ImageHeight, Intrinsics.FrameRate, Intrinsics.FOVDegrees);
}

void UCameraSensorComponent::CreateRenderTarget()
{
	RenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("SensorRenderTarget"));
	RenderTarget->InitAutoFormat(Intrinsics.ImageWidth, Intrinsics.ImageHeight);
	RenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	RenderTarget->bAutoGenerateMips = false;
	RenderTarget->UpdateResourceImmediate(true);
}

void UCameraSensorComponent::ConfigureSceneCapture()
{
	if (SceneCapture == nullptr || RenderTarget == nullptr)
	{
		UE_LOG(LogCameraSensor, Error, TEXT("SceneCaptureComponent is not ready."));
		return;
	}

	SceneCapture->TextureTarget = RenderTarget;
	SceneCapture->FOVAngle = Intrinsics.FOVDegrees;

	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;
	SceneCapture->bAlwaysPersistRenderingState = true;

	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	SceneCapture->PostProcessBlendWeight = 1.0f;
}

void UCameraSensorComponent::ApplyPostProcessSettings()
{
	if (SceneCapture == nullptr)
	{
		UE_LOG(LogCameraSensor, Error, TEXT("There is no SceneCaptureComponent."));
		return;
	}

	FPostProcessSettings& PP = SceneCapture->PostProcessSettings;

	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = PostProcess.VignetteIntensity;

	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = PostProcess.BloomIntensity;

	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = PostProcess.MotionBlurAmount;

	PP.bOverride_SceneFringeIntensity = true;
	PP.SceneFringeIntensity = PostProcess.ChromaticAberration;

	PP.bOverride_LensFlareIntensity = true;
	PP.LensFlareIntensity = PostProcess.LensFlareIntensity;

	if (Exposure.bEnableAutoExposure)
	{
		PP.bOverride_AutoExposureMethod = true;
		PP.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;

		PP.bOverride_AutoExposureMinBrightness = true;
		PP.AutoExposureMinBrightness = Exposure.MinEV;

		PP.bOverride_AutoExposureMaxBrightness = true;
		PP.AutoExposureMaxBrightness = Exposure.MaxEV;

		PP.bOverride_AutoExposureSpeedUp = true;
		PP.AutoExposureSpeedUp = Exposure.SpeedUp;

		PP.bOverride_AutoExposureSpeedDown = true;
		PP.AutoExposureSpeedDown = Exposure.SpeedDown;
	}
	else
	{
		PP.bOverride_AutoExposureMethod = true;
		PP.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	}
}

void UCameraSensorComponent::ApplyLensDistortion()
{
	if (SceneCapture == nullptr)
	{
		UE_LOG(LogCameraSensor, Error, TEXT("There is no SceneCaptureComponent"));
		return;
	}
	if (Distortion.HasDistortion() == false)
	{
		UE_LOG(LogCameraSensor, Error, TEXT("There is no Distortion"));
		return;
	}
	if (LensDistortionMaterial == nullptr)
	{
		UE_LOG(LogCameraSensor, Error, TEXT("There is no LensDistortionMaterial"));
		return;
	}

	DistortionMID = UMaterialInstanceDynamic::Create(LensDistortionMaterial, this);
	if (DistortionMID == nullptr)
	{
		UE_LOG(LogCameraSensor, Error, TEXT("Failed to create DistortionMID."));
		return;
	}

	DistortionMID->SetScalarParameterValue(TEXT("K1"), Distortion.K1);
	DistortionMID->SetScalarParameterValue(TEXT("K2"), Distortion.K2);
	DistortionMID->SetScalarParameterValue(TEXT("K3"), Distortion.K3);
	DistortionMID->SetScalarParameterValue(TEXT("P1"), Distortion.P1);
	DistortionMID->SetScalarParameterValue(TEXT("P2"), Distortion.P2);

	FWeightedBlendable Blendable;
	Blendable.Object = DistortionMID.Get();
	Blendable.Weight = 1.0f;
	SceneCapture->PostProcessSettings.WeightedBlendables.Array.Add(Blendable);
}

void UCameraSensorComponent::StartCaptureTimer()
{
	if (!GetWorld()) return;

	const float Interval = 1.0f / FMath::Max(Intrinsics.FrameRate, 1.0f);
	GetWorld()->GetTimerManager().SetTimer(
		CaptureTimerHandle, this, &UCameraSensorComponent::OnCaptureTimer,
		Interval, true);
}

void UCameraSensorComponent::StopCaptureTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CaptureTimerHandle);
	}
}

void UCameraSensorComponent::OnCaptureTimer()
{
	if (bSensorEnabled && SceneCapture)
	{
		SceneCapture->CaptureScene();
		FrameCount++;

		if (bIsDataSaving && RenderTarget)
		{
			SaveCameraImage();
		}
	}
}

void UCameraSensorComponent::SetCaptureRate(float Hz)
{
	Intrinsics.FrameRate = FMath::Clamp(Hz, 1.0f, 120.0f);
	StopCaptureTimer();
	if (bSensorEnabled)
	{
		StartCaptureTimer();
	}
}

void UCameraSensorComponent::CaptureOnce()
{
	if (SceneCapture)
	{
		SceneCapture->CaptureScene();
		FrameCount++;
	}
}

void UCameraSensorComponent::RefreshSettings()
{
	if (SceneCapture == nullptr)
	{
		UE_LOG(LogCameraSensor, Error, TEXT("There is no SceneCaptureComponent."));
		return;
	}

	SceneCapture->FOVAngle = Intrinsics.FOVDegrees;

	if (RenderTarget &&
		(RenderTarget->SizeX != Intrinsics.ImageWidth || RenderTarget->SizeY != Intrinsics.ImageHeight))
	{
		RenderTarget->InitAutoFormat(Intrinsics.ImageWidth, Intrinsics.ImageHeight);
		RenderTarget->UpdateResourceImmediate(true);
	}

	SceneCapture->PostProcessSettings.WeightedBlendables.Array.Empty();
	ApplyPostProcessSettings();
	ApplyLensDistortion();

	StopCaptureTimer();
	if (bSensorEnabled)
	{
		StartCaptureTimer();
	}
}

void UCameraSensorComponent::ApplyPreset(ECameraSensorPreset NewPreset)
{
	Preset = NewPreset;

	switch (NewPreset)
	{
	case ECameraSensorPreset::TeslaHW3_Wide:
		Intrinsics = { 1280, 960, 36.0f, 120.0f, 1.5f };
		Distortion = { -0.3f, 0.1f, 0.0f, 0.0f, 0.0f };
		PostProcess.VignetteIntensity = 0.4f;
		PostProcess.ChromaticAberration = 0.5f;
		PostProcess.MotionBlurAmount = 0.5f;
		PostProcess.BloomIntensity = 0.0f;
		PostProcess.LensFlareIntensity = 0.1f;
		Exposure = { true, 2.0f, 14.0f, 3.0f, 1.0f };
		Noise = { true, 0.0f, 0.02f, 0.01f };
		break;

	case ECameraSensorPreset::TeslaHW3_Main:
		Intrinsics = { 1280, 960, 36.0f, 50.0f, 5.0f };
		Distortion = { -0.1f, 0.02f, 0.0f, 0.0f, 0.0f };
		PostProcess.VignetteIntensity = 0.3f;
		PostProcess.ChromaticAberration = 0.3f;
		PostProcess.MotionBlurAmount = 0.5f;
		PostProcess.BloomIntensity = 0.0f;
		PostProcess.LensFlareIntensity = 0.05f;
		Exposure = { true, 2.0f, 14.0f, 3.0f, 1.0f };
		Noise = { true, 0.0f, 0.02f, 0.01f };
		break;

	case ECameraSensorPreset::TeslaHW3_Narrow:
		Intrinsics = { 1280, 960, 36.0f, 35.0f, 8.0f };
		Distortion = { -0.05f, 0.01f, 0.0f, 0.0f, 0.0f };
		PostProcess.VignetteIntensity = 0.2f;
		PostProcess.ChromaticAberration = 0.2f;
		PostProcess.MotionBlurAmount = 0.5f;
		PostProcess.BloomIntensity = 0.0f;
		PostProcess.LensFlareIntensity = 0.05f;
		Exposure = { true, 2.0f, 14.0f, 3.0f, 1.0f };
		Noise = { true, 0.0f, 0.02f, 0.01f };
		break;

	case ECameraSensorPreset::TeslaHW4:
		Intrinsics = { 2560, 1920, 24.0f, 130.0f, 1.3f };
		Distortion = { -0.35f, 0.12f, 0.0f, 0.0f, 0.0f };
		PostProcess.VignetteIntensity = 0.35f;
		PostProcess.ChromaticAberration = 0.4f;
		PostProcess.MotionBlurAmount = 0.4f;
		PostProcess.BloomIntensity = 0.0f;
		PostProcess.LensFlareIntensity = 0.08f;
		Exposure = { true, 1.0f, 15.0f, 3.5f, 1.2f };
		Noise = { true, 0.0f, 0.015f, 0.008f };
		break;

	case ECameraSensorPreset::DroneFPV:
		Intrinsics = { 1920, 1080, 60.0f, 150.0f, 2.5f };
		Distortion = { -0.5f, 0.2f, -0.05f, 0.0f, 0.0f };
		PostProcess.VignetteIntensity = 0.5f;
		PostProcess.ChromaticAberration = 0.7f;
		PostProcess.MotionBlurAmount = 0.3f;
		PostProcess.BloomIntensity = 0.1f;
		PostProcess.LensFlareIntensity = 0.15f;
		Exposure = { true, 3.0f, 16.0f, 4.0f, 2.0f };
		Noise = { true, 0.0f, 0.025f, 0.015f };
		break;

	case ECameraSensorPreset::Waymo:
		Intrinsics = { 1920, 1280, 30.0f, 50.0f, 6.0f };
		Distortion = { -0.08f, 0.015f, 0.0f, 0.0f, 0.0f };
		PostProcess.VignetteIntensity = 0.2f;
		PostProcess.ChromaticAberration = 0.15f;
		PostProcess.MotionBlurAmount = 0.3f;
		PostProcess.BloomIntensity = 0.0f;
		PostProcess.LensFlareIntensity = 0.03f;
		Exposure = { true, 1.0f, 16.0f, 4.0f, 1.5f };
		Noise = { true, 0.0f, 0.01f, 0.005f };
		break;

	case ECameraSensorPreset::Custom:
	default:
		break;
	}
}

void UCameraSensorComponent::SaveCameraImage()
{
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("SensorData") / DataSaveConfig.SensorLabel;
	IFileManager::Get().MakeDirectory(*Dir, true);

	const FString FilePath = Dir / FString::Printf(TEXT("%06lld.jpg"), FrameCount);

	FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!RTResource)
	{
		UE_LOG(LogCameraSensor, Warning, TEXT("RenderTarget resource unavailable for saving."));
		return;
	}

	const int32 Width = RenderTarget->SizeX;
	const int32 Height = RenderTarget->SizeY;

	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Width * Height);
	if (!RTResource->ReadPixels(Pixels))
	{
		UE_LOG(LogCameraSensor, Warning, TEXT("ReadPixels failed for frame %lld"), FrameCount);
		return;
	}

	for (FColor& Pixel : Pixels)
	{
		Pixel.A = 255;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

	if (ImageWrapper.IsValid() == false)
	{
		UE_LOG(LogCameraSensor, Warning, TEXT("Failed to create JPEG ImageWrapper."));
		return;
	}

	ImageWrapper->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8);
	const TArray64<uint8>& CompressedData = ImageWrapper->GetCompressed(DataSaveConfig.JPGQuality);

	FFileHelper::SaveArrayToFile(CompressedData, *FilePath);

	UE_LOG(LogCameraSensor, Verbose, TEXT("Saved %dx%d image → %s (%lld bytes)"),
		Width, Height, *FilePath, CompressedData.Num());
}
