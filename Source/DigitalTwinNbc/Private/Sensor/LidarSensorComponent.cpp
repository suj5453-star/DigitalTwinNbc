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
	{
		ApplyPreset(Preset);
	}

	bDirectionsDirty = true;
}
#endif

void ULidarSensorComponent::InitializeSensor()
{
	BevConfig.ViewRange = Config.MaxRange;

	BevRenderer = NewObject<ULidarBevRenderer>(this, TEXT("BevRenderer"));
	if (BevRenderer)
	{
		BevRenderer->Initialize(BevConfig);
	}

	ReserveBuffers(FMath::Min(Config.GetTotalPoints(), MaxTracesPerScan));

	bDirectionsDirty = true;
	NextRayIndex = 0;

	if (bSensorEnabled)
	{
		StartScanTimer();
	}

	UE_LOG(LogLidarSensor, Log,
		TEXT("LidarSensor initialized: %d ch x %d pts @ %.0f Hz, range %.0f m, max traces per scan %d"),
		Config.NumChannels,
		Config.PointsPerChannel,
		Config.RotationRate,
		Config.MaxRange / 100.0f,
		MaxTracesPerScan
	);
}

void ULidarSensorComponent::ReserveBuffers(int32 InReserveCount)
{
	const int32 ReserveCount = FMath::Max(InReserveCount, 1);

	PendingHandles.Reserve(ReserveCount);
	PendingWorldDirs.Reserve(ReserveCount);

	ScanPoints.Reserve(ReserveCount);
	ScanIntensities.Reserve(ReserveCount);
	ScanObstacleFlags.Reserve(ReserveCount);

	LastPointCloud.Points.Reserve(ReserveCount);
	LastPointCloud.Intensities.Reserve(ReserveCount);
	LastPointCloud.ObstacleFlags.Reserve(ReserveCount);
}

void ULidarSensorComponent::ApplyDefaultElevationAngles()
{
	Config.ElevationAngles.Reset();

	const int32 NumCh = FMath::Max(Config.NumChannels, 1);
	const float VertLow = Config.VerticalFOVLower;
	const float VertRng = Config.VerticalFOVUpper - VertLow;

	for (int32 Ch = 0; Ch < NumCh; ++Ch)
	{
		const float Alpha = (NumCh > 1) ? static_cast<float>(Ch) / static_cast<float>(NumCh - 1) : 0.5f;
		Config.ElevationAngles.Add(VertLow + VertRng * Alpha);
	}
}

void ULidarSensorComponent::RebuildDirectionCache()
{
	const int32 NumCh = Config.NumChannels;
	const int32 NumPts = Config.PointsPerChannel;
	const float HorizFOV = Config.HorizontalFOV;

	CachedLocalDirections.SetNum(NumCh * NumPts, EAllowShrinking::No);

	for (int32 Ch = 0; Ch < NumCh; ++Ch)
	{
		float VertDeg = 0.0f;

		if (Config.ElevationAngles.IsValidIndex(Ch))
		{
			VertDeg = Config.ElevationAngles[Ch];
		}
		else
		{
			const float VertLow = Config.VerticalFOVLower;
			const float VertRng = Config.VerticalFOVUpper - VertLow;
			VertDeg = (NumCh > 1) ? VertLow + VertRng * (static_cast<float>(Ch) / static_cast<float>(NumCh - 1)) : 0.0f;
		}

		const float CosVert = FMath::Cos(FMath::DegreesToRadians(VertDeg));
		const float SinVert = FMath::Sin(FMath::DegreesToRadians(VertDeg));

		for (int32 Pt = 0; Pt < NumPts; ++Pt)
		{
			const float HorizRad = FMath::DegreesToRadians((static_cast<float>(Pt) / static_cast<float>(NumPts)) * HorizFOV);
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
	if (GetWorld() == nullptr)
	{
		return;
	}

	const float Interval = 1.0f / FMath::Max(Config.RotationRate, 1.0f);
	GetWorld()->GetTimerManager().SetTimer(
		ScanTimerHandle,
		this,
		&ULidarSensorComponent::OnScanTimer,
		Interval,
		true
	);
}

void ULidarSensorComponent::StopScanTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ScanTimerHandle);
	}

	bHasPendingTraces = false;
	SetComponentTickEnabled(false);
}

void ULidarSensorComponent::OnScanTimer()
{
	if (!bSensorEnabled)
	{
		return;
	}

	if (bDirectionsDirty)
	{
		RebuildDirectionCache();
	}

	FireAsyncTraces();
	++FrameCount;
}

void ULidarSensorComponent::FireAsyncTraces()
{
	UWorld* World = GetWorld();
	if (!World || CachedLocalDirections.IsEmpty())
	{
		return;
	}

	if (bHasPendingTraces)
	{
		return;
	}

	PendingHandles.Reset();
	PendingWorldDirs.Reset();

	const FTransform SensorTransform = GetComponentTransform();
	const FVector SensorLoc = SensorTransform.GetLocation();
	const FQuat SensorQuat = SensorTransform.GetRotation();

	PendingTransform = SensorTransform;
	LastScanTimestamp = World->GetTimeSeconds();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LidarAsyncTrace), false);
	Params.AddIgnoredActor(GetOwner());
	Params.bReturnPhysicalMaterial = false;

	const int32 TotalRays = CachedLocalDirections.Num();
	const int32 TraceCount = FMath::Clamp(MaxTracesPerScan, 1, TotalRays);
	const float MaxRange = Config.MaxRange;

	for (int32 Step = 0; Step < TraceCount; ++Step)
	{
		const int32 RayIndex = (NextRayIndex + Step) % TotalRays;
		const FVector& LocalDir = CachedLocalDirections[RayIndex];

		const FVector WorldDir = SensorQuat.RotateVector(LocalDir);
		const FVector End = SensorLoc + WorldDir * MaxRange;

		const FTraceHandle Handle = World->AsyncLineTraceByChannel(
			EAsyncTraceType::Single,
			SensorLoc,
			End,
			ECC_Visibility,
			Params
		);

		PendingHandles.Add(Handle);
		PendingWorldDirs.Add(WorldDir);
	}

	NextRayIndex = (NextRayIndex + TraceCount) % TotalRays;

	FireFrameNumber = GFrameCounter;
	bHasPendingTraces = true;
	SetComponentTickEnabled(true);
}

void ULidarSensorComponent::CollectAsyncResults()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		bHasPendingTraces = false;
		return;
	}

	ScanPoints.Reset();
	ScanIntensities.Reset();
	ScanObstacleFlags.Reset();

	const float MaxRange = FMath::Max(Config.MaxRange, 1.0f);
	const float MinRange = FMath::Max(Config.MinRange, 0.0f);
	const float NoiseStd = FMath::Max(Config.NoiseStdDev, 0.0f);

	for (int32 i = 0; i < PendingHandles.Num(); ++i)
	{
		FTraceDatum Data;
		if (!World->QueryTraceData(PendingHandles[i], Data))
		{
			continue;
		}

		if (Data.OutHits.IsEmpty())
		{
			continue;
		}

		const FHitResult& Hit = Data.OutHits[0];
		if (!Hit.bBlockingHit || Hit.Distance < MinRange)
		{
			continue;
		}

		FVector HitPoint = Hit.ImpactPoint;

		if (NoiseStd > 0.0f && PendingWorldDirs.IsValidIndex(i))
		{
			HitPoint += PendingWorldDirs[i] * FMath::FRandRange(-NoiseStd, NoiseStd);
		}

		const bool bObstacle = Hit.Distance <= ObstacleDistanceThreshold;

		ScanPoints.Add(HitPoint);
		ScanIntensities.Add(FMath::Clamp(1.0f - Hit.Distance / MaxRange, 0.0f, 1.0f));
		ScanObstacleFlags.Add(bObstacle ? 1 : 0);
	}

	LastPointCloud.Points = MoveTemp(ScanPoints);
	LastPointCloud.Intensities = MoveTemp(ScanIntensities);
	LastPointCloud.ObstacleFlags = MoveTemp(ScanObstacleFlags);
	LastPointCloud.PointCount = LastPointCloud.Points.Num();
	LastPointCloud.FrameNumber = FrameCount;
	LastPointCloud.Timestamp = LastScanTimestamp;

	ReserveBuffers(FMath::Min(Config.GetTotalPoints(), MaxTracesPerScan));

	if (BevRenderer)
	{
		BevRenderer->RenderPointCloud(LastPointCloud, PendingTransform);
	}
	OnLidarScanReady.Broadcast(LastPointCloud.Points);
	
	if (bIsDataSaving && LastPointCloud.PointCount > 0)
	{
		SavePointCloudData();
	}

	bHasPendingTraces = false;

	UE_LOG(LogLidarSensor, Verbose,
		TEXT("LidarSensor frame %lld: %d points, timestamp %.3f"),
		FrameCount,
		LastPointCloud.PointCount,
		LastPointCloud.Timestamp
	);
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
	{
		StartScanTimer();
	}
}

void ULidarSensorComponent::RefreshSettings()
{
	bDirectionsDirty = true;
	NextRayIndex = 0;

	BevConfig.ViewRange = Config.MaxRange;

	if (BevRenderer)
	{
		BevRenderer->UpdateConfig(BevConfig);
	}

	ReserveBuffers(FMath::Min(Config.GetTotalPoints(), MaxTracesPerScan));

	StopScanTimer();

	if (bSensorEnabled)
	{
		StartScanTimer();
	}
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
		Config.ElevationAngles = {
			-15.0f, 1.0f, -13.0f, 3.0f,
			-11.0f, 5.0f, -9.0f, 7.0f,
			-7.0f, 9.0f, -5.0f, 11.0f,
			-3.0f, 13.0f, -1.0f, 15.0f
		};
		break;

	case ELidarSensorPreset::VelodyneVLP32:
		Config = { 32, 60, 10.0f, 20000.0f, 50.0f, 15.0f, -25.0f, 360.0f, 2.0f };
		ApplyDefaultElevationAngles();
		break;

	case ELidarSensorPreset::OusterOS1_64:
		Config = { 64, 45, 10.0f, 12000.0f, 50.0f, 22.5f, -22.5f, 360.0f, 1.5f };
		ApplyDefaultElevationAngles();
		break;

	case ELidarSensorPreset::Livox_Mid360:
		Config = { 8, 45, 10.0f, 7000.0f, 100.0f, 52.0f, -7.0f, 360.0f, 3.0f };
		ApplyDefaultElevationAngles();
		break;

	case ELidarSensorPreset::Custom:
	default:
		if (Config.ElevationAngles.Num() != Config.NumChannels)
		{
			ApplyDefaultElevationAngles();
		}
		break;
	}

	BevConfig.ViewRange = Config.MaxRange;

	if (BevRenderer)
	{
		BevRenderer->UpdateConfig(BevConfig);
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

	struct FKittiPoint
	{
		float X;
		float Y;
		float Z;
		float Intensity;
	};

	TArray<FKittiPoint> Buffer;
	Buffer.SetNumUninitialized(NumPoints);

	for (int32 i = 0; i < NumPoints; ++i)
	{
		const FVector Local = InvSensor.TransformPosition(LastPointCloud.Points[i]);
		Buffer[i].X = static_cast<float>(Local.X * 0.01);
		Buffer[i].Y = -static_cast<float>(Local.Y * 0.01);
		Buffer[i].Z = static_cast<float>(Local.Z * 0.01);
		Buffer[i].Intensity = LastPointCloud.Intensities.IsValidIndex(i) ? LastPointCloud.Intensities[i] : 0.0f;
	}

	File->Write(
		reinterpret_cast<const uint8*>(Buffer.GetData()),
		NumPoints * sizeof(FKittiPoint)
	);

	UE_LOG(LogLidarSensor, Verbose, TEXT("Saved %d points → %s"), NumPoints, *FilePath);
}

void ULidarSensorComponent::SetRangeNoiseStdDev(float InNoiseStdDev)
{
	Config.NoiseStdDev = FMath::Clamp(InNoiseStdDev, 0.0f, 50.0f);
}

void ULidarSensorComponent::SetObstacleDistanceThreshold(float InThresholdCm)
{
	ObstacleDistanceThreshold = FMath::Max(InThresholdCm, 0.0f);
}

int32 ULidarSensorComponent::GetObstaclePointCount() const
{
	int32 Count = 0;

	for (int32 Flag : LastPointCloud.ObstacleFlags)
	{
		if (Flag != 0)
		{
			++Count;
		}
	}

	return Count;
}

float ULidarSensorComponent::GetClosestObstacleDistanceCm() const
{
	const FVector SensorLocation = GetComponentLocation();

	float ClosestDistance = TNumericLimits<float>::Max();

	for (int32 i = 0; i < LastPointCloud.Points.Num(); ++i)
	{
		if (!LastPointCloud.ObstacleFlags.IsValidIndex(i) || LastPointCloud.ObstacleFlags[i] == 0)
		{
			continue;
		}

		const float Distance = FVector::Dist(SensorLocation, LastPointCloud.Points[i]);
		ClosestDistance = FMath::Min(ClosestDistance, Distance);
	}

	return ClosestDistance == TNumericLimits<float>::Max()
		? -1.0f
		: ClosestDistance;
}