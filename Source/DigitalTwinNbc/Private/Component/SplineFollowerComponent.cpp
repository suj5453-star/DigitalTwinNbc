// Copyright NBC, Inc. All Rights Reserved.

#include "Component/SplineFollowerComponent.h"
#include "DigitalTwinNbc.h"
#include "DigitalTwinNbcPawn.h"
#include "EngineUtils.h"
#include "LandscapeSplineActor.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplineSegment.h"
#include "LandscapeSplinesComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogPathFollowingComponent, Log, All);

USplineFollowerComponent::USplineFollowerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void USplineFollowerComponent::BuildPath()
{
	if (OwnerPawn.IsValid() == false)
	{
		UE_LOG(LogPathFollowingComponent, Error, TEXT("Failed to cache OwnerPawn."));
		return;
	}
	
	PathPoints.Reset();
	bClosedLoop = false;

	// Find the single ALandscapeSplineActor
	ULandscapeSplinesComponent* SplinesComp = nullptr;
	for (TActorIterator<ALandscapeSplineActor> It(GetWorld()); It; ++It)
	{
		SplinesComp = It->GetSplinesComponent();
		if (SplinesComp) break;
	}
	if (!SplinesComp)
	{
		UE_LOG(LogDigitalTwinNbc, Warning, TEXT("SplineFollower: No ALandscapeSplineActor found"));
		return;
	}

	const FTransform ToWorld = SplinesComp->GetComponentTransform();
	const FVector VehicleLoc = OwnerPawn->GetActorLocation();

	// Find nearest control point
	float BestDist = SearchRadius;
	ULandscapeSplineControlPoint* NearestCP = nullptr;
	for (ULandscapeSplineControlPoint* CP : SplinesComp->GetControlPoints())
	{
		const float D = FVector::Dist(ToWorld.TransformPosition(CP->Location), VehicleLoc);
		if (D < BestDist)
		{
			BestDist = D;
			NearestCP = CP;
		}
	}
	if (!NearestCP)
	{
		UE_LOG(LogDigitalTwinNbc, Warning, TEXT("SplineFollower: No control point within SearchRadius"));
		return;
	}

	// Walk backward to chain start (or detect loop)
	TSet<ULandscapeSplineSegment*> Visited;
	ULandscapeSplineControlPoint* StartCP = NearestCP;
	while (true)
	{
		ULandscapeSplineSegment* Seg = nullptr;
		for (const auto& Conn : StartCP->ConnectedSegments)
		{
			if (!Visited.Contains(Conn.Segment))
			{
				Seg = Conn.Segment;
				break;
			}
		}
		if (!Seg) break;
		Visited.Add(Seg);

		ULandscapeSplineControlPoint* Other =
			(Seg->Connections[0].ControlPoint == StartCP)
			? Seg->Connections[1].ControlPoint
			: Seg->Connections[0].ControlPoint;

		if (Other == NearestCP) { StartCP = NearestCP; break; }
		StartCP = Other;
	}

	// Forward traversal: flatten GetPoints().Center
	Visited.Reset();
	ULandscapeSplineControlPoint* CurCP = StartCP;
	while (true)
	{
		ULandscapeSplineSegment* Seg = nullptr;
		for (const auto& Conn : CurCP->ConnectedSegments)
		{
			if (!Visited.Contains(Conn.Segment)) { Seg = Conn.Segment; break; }
		}
		if (!Seg) break;
		Visited.Add(Seg);

		const bool bReversed = (Seg->Connections[1].ControlPoint == CurCP);
		const TArray<FLandscapeSplineInterpPoint>& Pts = Seg->GetPoints();

		if (Pts.Num() >= 2)
		{
			const bool bSkipFirst = PathPoints.Num() > 0;
			if (!bReversed)
				for (int32 i = (bSkipFirst ? 1 : 0); i < Pts.Num(); ++i)
					PathPoints.Add(ToWorld.TransformPosition(Pts[i].Center));
			else
				for (int32 i = Pts.Num() - 1 - (bSkipFirst ? 1 : 0); i >= 0; --i)
					PathPoints.Add(ToWorld.TransformPosition(Pts[i].Center));
		}

		ULandscapeSplineControlPoint* NextCP = bReversed
			? Seg->Connections[0].ControlPoint : Seg->Connections[1].ControlPoint;
		if (NextCP == StartCP) { bClosedLoop = true; break; }
		CurCP = NextCP;
	}

	if (PathPoints.Num() < 3)
	{
		UE_LOG(LogDigitalTwinNbc, Warning, TEXT("SplineFollower: Too few path points (%d)"), PathPoints.Num());
		return;
	}

	ResampleCatmullRom();

	// Starting index closest to vehicle
	float BestSq = TNumericLimits<float>::Max();
	CurrentPointIndex = 0;
	for (int32 i = 0; i < PathPoints.Num(); ++i)
	{
		const float Sq = FVector::DistSquared(PathPoints[i], VehicleLoc);
		if (Sq < BestSq) { BestSq = Sq; CurrentPointIndex = i; }
	}

	SmoothedTargetSpeed = MaxSpeed;
	SetComponentTickEnabled(true);

	UE_LOG(LogDigitalTwinNbc, Log, TEXT("SplineFollower: %d pts, loop=%s, start=%d"),
		PathPoints.Num(), bClosedLoop ? TEXT("Y") : TEXT("N"), CurrentPointIndex);
}

static FVector EvalCatmullRom(const FVector& P0, const FVector& P1,
                               const FVector& P2, const FVector& P3, float T)
{
	const float T2 = T * T, T3 = T2 * T;
	return 0.5f * (
		(2.f * P1) +
		(-P0 + P2) * T +
		(2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * T2 +
		(-P0 + 3.f * P1 - 3.f * P2 + P3) * T3);
}

void USplineFollowerComponent::ResampleCatmullRom()
{
	const int32 N = PathPoints.Num();
	if (N < 3) return;

	auto Idx = [&](int32 i) -> int32
	{
		return bClosedLoop ? ((i % N) + N) % N : FMath::Clamp(i, 0, N - 1);
	};

	TArray<FVector> Out;
	Out.Reserve(N * 12);

	for (int32 i = 0; i < N - 1; ++i)
	{
		const FVector& P0 = PathPoints[Idx(i - 1)];
		const FVector& P1 = PathPoints[i];
		const FVector& P2 = PathPoints[i + 1];
		const FVector& P3 = PathPoints[Idx(i + 2)];
		const int32 Steps = FMath::Max(
			1,
			FMath::CeilToInt32(FVector::Dist(P1, P2) / ResampleSpacing)
		);

		for (int32 s = 0; s < Steps; ++s)
		{
			FVector CatmullRom = EvalCatmullRom(P0, P1, P2, P3, (float)s / (float)Steps);
			Out.Add(CatmullRom);
		}
	}
	Out.Add(PathPoints.Last());

	if (bClosedLoop)
	{
		const FVector& P0 = PathPoints[N - 2]; const FVector& P1 = PathPoints[N - 1];
		const FVector& P2 = PathPoints[0];     const FVector& P3 = PathPoints[1];
		const int32 Steps = FMath::Max(1, FMath::CeilToInt32(FVector::Dist(P1, P2) / ResampleSpacing));
		for (int32 s = 1; s < Steps; ++s)
			Out.Add(EvalCatmullRom(P0, P1, P2, P3, (float)s / (float)Steps));
	}

	UE_LOG(LogDigitalTwinNbc, Log, TEXT("SplineFollower: Resampled %d -> %d pts"), N, Out.Num());
	PathPoints = MoveTemp(Out);
}

FVector USplineFollowerComponent::GetPointAhead(FVector& OutDir, float Distance) const
{
	if (OwnerPawn.IsValid() == false)
	{
		UE_LOG(LogPathFollowingComponent, Error, TEXT("Failed to cache OwnerPawn."));
		return FVector::ZeroVector;
	}
	
	const int32 Num = PathPoints.Num();
	const FVector Loc = OwnerPawn->GetActorLocation();

	// Project vehicle onto current segment
	const int32 Nxt = bClosedLoop ? (CurrentPointIndex + 1) % Num
	                              : FMath::Min(CurrentPointIndex + 1, Num - 1);
	const FVector A = PathPoints[CurrentPointIndex], B = PathPoints[Nxt];
	const FVector AB = B - A;
	const float Len = AB.Size();
	const float T = (Len > KINDA_SMALL_NUMBER)
		? FMath::Clamp(FVector::DotProduct(Loc - A, AB) / (Len * Len), 0.f, 1.f) : 0.f;
	const float Remain = (1.f - T) * Len;

	// Look-ahead within current segment
	if (Distance <= Remain && Len > KINDA_SMALL_NUMBER)
	{
		OutDir = AB.GetSafeNormal();
		return FMath::Lerp(A, B, FMath::Min(T + Distance / Len, 1.f));
	}

	// Walk forward
	float Left = Distance - Remain;
	int32 Idx = Nxt;
	while (Left > 0.f)
	{
		const int32 Next = bClosedLoop ? (Idx + 1) % Num : Idx + 1;
		if (!bClosedLoop && Next >= Num)
		{
			OutDir = (PathPoints[Idx] - PathPoints[FMath::Max(0, Idx - 1)]).GetSafeNormal();
			return PathPoints[Idx];
		}
		const FVector Seg = PathPoints[Next] - PathPoints[Idx];
		const float SLen = Seg.Size();
		if (Left <= SLen && SLen > KINDA_SMALL_NUMBER)
		{
			OutDir = Seg.GetSafeNormal();
			return FMath::Lerp(PathPoints[Idx], PathPoints[Next], Left / SLen);
		}
		Left -= SLen;
		Idx = Next;
		if (Idx == CurrentPointIndex) break;
	}

	const int32 Prev = bClosedLoop ? ((Idx - 1 + Num) % Num) : FMath::Max(0, Idx - 1);
	OutDir = (PathPoints[Idx] - PathPoints[Prev]).GetSafeNormal();
	return PathPoints[Idx];
}

float USplineFollowerComponent::EstimateCurvature(float AheadOffset) const
{
	FVector D1, D2;
	GetPointAhead(D1, AheadOffset);
	GetPointAhead(D2, AheadOffset + CurvatureSampleSpan);
	return FMath::Acos(FMath::Clamp(FVector::DotProduct(D1, D2), -1.f, 1.f));
}

float USplineFollowerComponent::ComputeCurveSpeedLimit(float Curvature) const
{
	if (Curvature <= KINDA_SMALL_NUMBER)
		return MaxSpeed;
	const float Radius = CurvatureSampleSpan / Curvature;
	return FMath::Clamp(
		FMath::Sqrt(LateralFriction * 980.f * Radius),
		MinSpeed, MaxSpeed
	);
}

void USplineFollowerComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerPawn = Cast<ADigitalTwinNbcPawn>(GetOwner());
	BuildPath();
}

void USplineFollowerComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (OwnerPawn.IsValid() == false)
	{
		UE_LOG(LogPathFollowingComponent, Error, TEXT("Failed to cache OwnerPawn."));
		return;
	}
	if (PathPoints.Num() < 2)
	{
		UE_LOG(LogPathFollowingComponent, Error, TEXT("PathPoints has single point."));
		return;
	}

	// Get advanced point index
	const FVector Location = OwnerPawn->GetActorLocation();
	const float Velocity = OwnerPawn->GetVelocity().Size();
	const int32 Max = bClosedLoop ? PathPoints.Num() : PathPoints.Num() - 2;
	const int32 MaxSteps = FMath::CeilToInt32(
		Velocity / (60.f * FMath::Max(ResampleSpacing, 1.f))
	) + 1;

	for (int32 S = 0; S < MaxSteps && CurrentPointIndex < Max; ++S)
	{
		const int32 Next = (CurrentPointIndex + 1) % PathPoints.Num();
		const FVector Seg = PathPoints[Next] - PathPoints[CurrentPointIndex];
		if (FVector::DotProduct(Location - PathPoints[CurrentPointIndex], Seg) >= Seg.SizeSquared())
			CurrentPointIndex = bClosedLoop ? Next : CurrentPointIndex + 1;
		else
			break;
	}

	if (!bClosedLoop && CurrentPointIndex >= PathPoints.Num() - 2)
	{
		OwnerPawn->DoThrottle(0.f);
		OwnerPawn->DoBrakeStart();
		return;
	}

	const float Yaw   = OwnerPawn->GetActorRotation().Yaw;

	// ---- Curvature at current position and ahead ----
	const float CurvHere  = EstimateCurvature(0.f);
	const float CurvAhead = EstimateCurvature(BrakePreviewDist);

	// ---- Steering ----
	// Look-ahead: speed-proportional, shortened on curves
	const float CurvScale = FMath::Lerp(
		1.f,
		0.5f,
		FMath::Clamp(CurvHere * 3.f, 0.f, 1.f)
	);
	const float LADist = (LookAheadBase + Velocity * LookAheadSpeedFactor) * CurvScale;

	FVector PathDir;
	const FVector LAPos = GetPointAhead(PathDir, LADist);

	// Yaw error to look-ahead position (crosstrack + path following)
	const float PosDelta = FMath::FindDeltaAngleDegrees(
		Yaw,
		FMath::Atan2(
			(LAPos - Location).GetSafeNormal().Y,
			(LAPos - Location).GetSafeNormal().X
		) * (180.f / PI)
	);

	// Yaw error to path heading (curve anticipation)
	const float HdgDelta = FMath::FindDeltaAngleDegrees(
		Yaw, FMath::Atan2(PathDir.Y, PathDir.X) * (180.f / PI));

	// Crosstrack: signed perpendicular distance to current path segment
	const int32 Nxt = bClosedLoop ?
		(CurrentPointIndex + 1) % PathPoints.Num()
	     : FMath::Min(CurrentPointIndex + 1, PathPoints.Num() - 1);
	const FVector SegDir = (PathPoints[Nxt] - PathPoints[CurrentPointIndex]).GetSafeNormal();
	const FVector Offset = Location - PathPoints[CurrentPointIndex];
	const float CrossErr = FVector::DotProduct(
		Offset - SegDir * FVector::DotProduct(Offset, SegDir),
		FVector::CrossProduct(FVector::UpVector, SegDir)
	);

	// Blend:  heading (anticipation) + position (tracking) + crosstrack (centerline)
	const float HdgW = FMath::Lerp(
		HeadingWeight,
		0.3f,
		FMath::Clamp(CurvHere * 3.f, 0.f, 1.f)
	);
	const float YawCmd = PosDelta * (1.f - HdgW) + HdgDelta * HdgW;
	const float Steer = FMath::Clamp(
		YawCmd / MaxYawDelta - CrossErr * CrosstrackGain,
		-1.f, 1.f
	);
	OwnerPawn->DoSteering(Steer);

	// ---- Speed ----
	const float SpeedLimit = FMath::Min(
		ComputeCurveSpeedLimit(CurvHere),
		ComputeCurveSpeedLimit(CurvAhead)
	);

	const float Rate = (SpeedLimit < SmoothedTargetSpeed) ? DecelRate : AccelRate;
	SmoothedTargetSpeed = FMath::FInterpTo(
		SmoothedTargetSpeed,
		SpeedLimit, DeltaTime,
		Rate
	);

	const float Cmd = FMath::Clamp(
		(SmoothedTargetSpeed - Velocity) * ThrottleGain,
		-1.f, 1.f
	);
	if (Cmd > 0.05f)
		OwnerPawn->DoThrottle(Cmd);
	else if (Cmd < -0.05f)
		OwnerPawn->DoBrake(-Cmd);
	else
	{
		OwnerPawn->DoThrottle(0.f);
		OwnerPawn->DoBrake(0.f);
	}
}
