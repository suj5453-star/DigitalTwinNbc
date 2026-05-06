// Copyright NBC, Inc. All Rights Reserved.

#include "Sensor/LidarSensorComponent.h"
#include "Sensor/LidarBevRenderer.h"
#include "Engine/World.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogLidarSensor, Log, All);

ULidarSensorComponent::ULidarSensorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void ULidarSensorComponent::BeginPlay()
{
	Super::BeginPlay();
	ApplyPreset(Preset);
	InitializeSensor();
}

void ULidarSensorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopScanTimer();
	Super::EndPlay(EndPlayReason);
}

void ULidarSensorComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bHasPendingTraces && GFrameCounter > FireFrameNumber)
	{
		CollectAsyncResults();
		SetComponentTickEnabled(false);
	}
}

#if WITH_EDITOR
void ULidarSensorComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropName = PropertyChangedEvent.GetPropertyName();
	if (PropName == GET_MEMBER_NAME_CHECKED(ULidarSensorComponent, Preset))
		ApplyPreset(Preset);
	bDirectionsDirty = true;
}
#endif

void ULidarSensorComponent::InitializeSensor()
{
	BevConfig.ViewRange = Config.MaxRange;

	BevRenderer = NewObject<ULidarBevRenderer>(this, TEXT("BevRenderer"));
	BevRenderer->Initialize(BevConfig);

	const int32 TotalPts = Config.GetTotalPoints();
	PendingHandles.Reserve(TotalPts);
	PendingWorldDirs.Reserve(TotalPts);
	ScanPoints.Reserve(TotalPts);
	ScanIntensities.Reserve(TotalPts);
	LastPointCloud.Points.Reserve(TotalPts);
	LastPointCloud.Intensities.Reserve(TotalPts);

	bDirectionsDirty = true;
	bSensorEnabled = false;

	UE_LOG(LogLidarSensor, Log,
		TEXT("LidarSensor initialized: %d ch x %d pts @ %.0f Hz, range %.0f m  [AsyncTrace]"),
		Config.NumChannels, Config.PointsPerChannel,
		Config.RotationRate, Config.MaxRange / 100.0f
	);
}

void ULidarSensorComponent::RebuildDirectionCache()
{
	const int32 NumCh    = Config.NumChannels;
	const int32 NumPts   = Config.PointsPerChannel;
	const float VertLow  = Config.VerticalFOVLower;
	const float VertRng  = Config.VerticalFOVUpper - VertLow;
	const float HorizFOV = Config.HorizontalFOV;

	CachedLocalDirections.SetNum(NumCh * NumPts, EAllowShrinking::No);

	for (int32 Ch = 0; Ch < NumCh; ++Ch)
	{
		const float VertDeg  = (NumCh > 1) ? VertLow + VertRng * (float(Ch) / (NumCh - 1)) : 0.f;
		const float CosVert  = FMath::Cos(FMath::DegreesToRadians(VertDeg));
		const float SinVert  = FMath::Sin(FMath::DegreesToRadians(VertDeg));

		for (int32 Pt = 0; Pt < NumPts; ++Pt)
		{
			const float HorizRad = FMath::DegreesToRadians((float(Pt) / NumPts) * HorizFOV);
			CachedLocalDirections[Ch * NumPts + Pt] = FVector(
				CosVert * FMath::Cos(HorizRad),
				CosVert * FMath::Sin(HorizRad),
				SinVert
			);
		}
	}

	bDirectionsDirty = false;
}

void ULidarSensorComponent::StartScanTimer()
{
	if (GetWorld() == nullptr) return;
	
	const float Interval = 1.0f / FMath::Max(Config.RotationRate, 1.0f);
	GetWorld()->GetTimerManager().SetTimer(
		ScanTimerHandle,
		this,
		&ULidarSensorComponent::OnScanTimer,
		Interval,
		true);
}

void ULidarSensorComponent::StopScanTimer()
{
	if (GetWorld())
		GetWorld()->GetTimerManager().ClearTimer(ScanTimerHandle);
	
	bHasPendingTraces = false;
	SetComponentTickEnabled(false);
}

void ULidarSensorComponent::OnScanTimer()
{
	if (!bSensorEnabled)
		return;

	if (bDirectionsDirty)
		RebuildDirectionCache();

	FireAsyncTraces();

	++FrameCount;
}

void ULidarSensorComponent::FireAsyncTraces()
{
	UWorld* World = GetWorld();
	if (!World || CachedLocalDirections.IsEmpty())
		return;

	PendingHandles.Reset();
	PendingWorldDirs.Reset();

	const FTransform SensorTransform = GetComponentTransform();
	const FVector    SensorLoc       = SensorTransform.GetLocation();
	const FQuat      SensorQuat      = SensorTransform.GetRotation();
	PendingTransform = SensorTransform;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LidarAsyncTrace), false);
	Params.AddIgnoredActor(GetOwner());
	Params.bReturnPhysicalMaterial = false;

	const float MaxRange = Config.MaxRange;

	for (const FVector& LocalDir : CachedLocalDirections)
	{
		const FVector WorldDir = SensorQuat.RotateVector(LocalDir);
		const FVector End      = SensorLoc + WorldDir * MaxRange;

		FTraceHandle Handle = World->AsyncLineTraceByChannel(
			EAsyncTraceType::Single,
			SensorLoc, End,
			ECC_Visibility,
			Params
		);

		PendingHandles.Add(Handle);
		PendingWorldDirs.Add(WorldDir);
	}

	FireFrameNumber   = GFrameCounter;
	bHasPendingTraces = true;

	SetComponentTickEnabled(true);
}

void ULidarSensorComponent::CollectAsyncResults()
{
	UWorld* World = GetWorld();
	if (!World) return;

	ScanPoints.Reset();
	ScanIntensities.Reset();

	const float MaxRange = Config.MaxRange;
	const float MinRange = Config.MinRange;
	const float NoiseStd = Config.NoiseStdDev;

	for (int32 i = 0; i < PendingHandles.Num(); ++i)
	{
		FTraceDatum Data;
		if (!World->QueryTraceData(PendingHandles[i], Data)) continue;
		if (Data.OutHits.IsEmpty()) continue;

		const FHitResult& Hit = Data.OutHits[0];
		if (!Hit.bBlockingHit || Hit.Distance < MinRange) continue;

		FVector HitPoint = Hit.ImpactPoint;
		if (NoiseStd > 0.f && PendingWorldDirs.IsValidIndex(i))
		{
			HitPoint += PendingWorldDirs[i] * FMath::RandRange(-NoiseStd, NoiseStd);
		}

		ScanPoints.Add(HitPoint);
		ScanIntensities.Add(FMath::Clamp(1.f - (Hit.Distance / MaxRange), 0.f, 1.f));
	}

	LastPointCloud.Points      = MoveTemp(ScanPoints);
	LastPointCloud.Intensities = MoveTemp(ScanIntensities);
	LastPointCloud.PointCount  = LastPointCloud.Points.Num();
	LastPointCloud.FrameNumber = FrameCount;

	ScanPoints.Reserve(Config.GetTotalPoints());
	ScanIntensities.Reserve(Config.GetTotalPoints());

	if (BevRenderer)
		BevRenderer->RenderPointCloud(LastPointCloud, PendingTransform);

	if (bIsDataSaving && LastPointCloud.PointCount > 0)
		SavePointCloudData();

	bHasPendingTraces = false;

	UE_LOG(LogLidarSensor, Verbose,
		TEXT("LidarSensor frame %lld: %d points"), FrameCount, LastPointCloud.PointCount);
}

void ULidarSensorComponent::StartScan()
{
	bSensorEnabled = true;
	StartScanTimer();
}

void ULidarSensorComponent::StopScan()
{
	bSensorEnabled = false;
	StopScanTimer();
}

void ULidarSensorComponent::SetScanRate(float Hz)
{
	Config.RotationRate = FMath::Clamp(Hz, 1.0f, 30.0f);
	StopScanTimer();
	if (bSensorEnabled)
		StartScanTimer();
}

void ULidarSensorComponent::RefreshSettings()
{
	bDirectionsDirty = true;
	BevConfig.ViewRange = Config.MaxRange;
	
	if (BevRenderer)
		BevRenderer->UpdateConfig(BevConfig);
	
	StopScanTimer();
	
	if (bSensorEnabled)
		StartScanTimer();
}

UTexture2D* ULidarSensorComponent::GetBevRenderTarget() const
{
	return BevRenderer ? BevRenderer->GetRenderTarget() : nullptr;
}

void ULidarSensorComponent::ApplyPreset(ELidarSensorPreset NewPreset)
{
	Preset = NewPreset;
	bDirectionsDirty = true;

	switch (NewPreset)
	{
	case ELidarSensorPreset::VelodyneVLP16:
		Config = { 16, 1800, 10.0f, 10000.0f, 50.0f, 15.0f, -15.0f, 360.0f, 2.0f };
		break;

	case ELidarSensorPreset::VelodyneVLP32:
		Config = { 32, 60, 10.0f, 20000.0f, 50.0f, 15.0f, -25.0f, 360.0f, 2.0f };
		break;

	case ELidarSensorPreset::OusterOS1_64:
		Config = { 64, 45, 10.0f, 12000.0f, 50.0f, 22.5f, -22.5f, 360.0f, 1.5f };
		break;

	case ELidarSensorPreset::Livox_Mid360:
		Config = { 8, 45, 10.0f, 7000.0f, 100.0f, 52.0f, -7.0f, 360.0f, 3.0f };
		break;

	case ELidarSensorPreset::Custom:
	default:
		break;
	}
}

void ULidarSensorComponent::SavePointCloudData()
{
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("SensorData") / DataSaveConfig.SensorLabel;
	IFileManager::Get().MakeDirectory(*Dir, true);

	const FString FilePath = Dir / FString::Printf(TEXT("%06lld.bin"), FrameCount);

	TUniquePtr<IFileHandle> File(FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*FilePath));
	if (!File)
	{
		UE_LOG(LogLidarSensor, Warning, TEXT("Failed to open file for writing: %s"), *FilePath);
		return;
	}

	const FTransform InvSensor = PendingTransform.Inverse();
	const int32 NumPoints = LastPointCloud.PointCount;

	struct FKittiPoint { float X, Y, Z, Intensity; };
	TArray<FKittiPoint> Buffer;
	Buffer.SetNumUninitialized(NumPoints);

	for (int32 i = 0; i < NumPoints; ++i)
	{
		const FVector Local = InvSensor.TransformPosition(LastPointCloud.Points[i]);
		Buffer[i].X         =  static_cast<float>(Local.X * 0.01);
		Buffer[i].Y         = -static_cast<float>(Local.Y * 0.01);
		Buffer[i].Z         =  static_cast<float>(Local.Z * 0.01);
		Buffer[i].Intensity = LastPointCloud.Intensities[i];
	}

	File->Write(
		reinterpret_cast<const uint8*>(Buffer.GetData()),
		NumPoints * sizeof(FKittiPoint)
	);

	UE_LOG(LogLidarSensor, Verbose, TEXT("Saved %d points → %s"), NumPoints, *FilePath);
}
