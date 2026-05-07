// Copyright NBC, Inc. All Rights Reserved.

#include "DataLogger/AgentDataLogger.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sensor/SensorExperimentComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

UAgentDataLogger::UAgentDataLogger()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UAgentDataLogger::BeginPlay()
{
	Super::BeginPlay();
	
	// SensorExperimentComponent는 별도 CSV를 만들지 않고,
	// AgentDataLogger가 하나의 CSV에 값을 합칠 수 있도록 컬럼/값 문자열만 제공합니다.
	if (AActor* Owner = GetOwner())
	{
		SensorExperimentComponent = Owner->FindComponentByClass<USensorExperimentComponent>();
	}

	OriginUtmZone = GetUtmZone(OriginLongitude);
	LatLonToUtm(OriginLatitude, OriginLongitude, OriginUtmZone, OriginUtmEasting, OriginUtmNorthing);

	if (GetOwner())
		LastLabelLocation = GetOwner()->GetActorLocation();
	
	if (bEnableLogging)
	{
		StartRecording();
	}
}

void UAgentDataLogger::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	StopRecording();
	Super::EndPlay(EndPlayReason);
}

void UAgentDataLogger::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsRecording)
	{
		return;
	}

	ElapsedRecordingTime += DeltaTime;
	TimeSinceLastSave += DeltaTime;

	const float SaveInterval = 1.0f / FMath::Max(SaveFrequencyHz, 0.1f);
	if (TimeSinceLastSave >= SaveInterval)
	{
		AppendRow();
		TimeSinceLastSave -= SaveInterval;
	}
}

void UAgentDataLogger::StartRecording()
{
	if (bIsRecording)
	{
		return;
	}

	CreateCsvFile();
	bIsRecording = true;
	TimeSinceLastSave = 0.0f;
	ElapsedRecordingTime = 0.0f;
}

void UAgentDataLogger::StopRecording()
{
	bIsRecording = false;
}

void UAgentDataLogger::CreateCsvFile()
{
	const FString OutputDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Output"));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*OutputDir);

	const FDateTime Now = FDateTime::Now();
	const FString FileName = FString::Printf(
		TEXT("AgentData-%04d_%02d-%02d-%02d-%02d-%02d.csv"),
		Now.GetYear(), Now.GetMonth(), Now.GetDay(),
		Now.GetHour(), Now.GetMinute(), Now.GetSecond()
	);

	CsvFilePath = FPaths::Combine(OutputDir, FileName);

	FString Header =
		TEXT("Timestamp,World_X,World_Y,World_Z,UTM_Easting,UTM_Northing,UTM_Zone,Velocity_kmh,Yaw,Accel_ms2,SteeringInput\n");
  	// SensorExperimentComponent가 있으면 센서 실험 관련 컬럼을 뒤에 붙임
	if (SensorExperimentComponent)
	{
		Header += SensorExperimentComponent->BuildCsvHeaderColumns();
	}
	Header += LINE_TERMINATOR;
	FFileHelper::SaveStringToFile(Header, *CsvFilePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM
	);

	UE_LOG(LogTemp, Log, TEXT("[AgentDataLogger] Recording to: %s  (%.1f Hz)"), *CsvFilePath, SaveFrequencyHz);
}

void UAgentDataLogger::AppendRow()
{
	const AActor* Owner = GetOwner();
	if (!Owner)
		return;

	const FVector WorldLoc = Owner->GetActorLocation();        // cm
	const FRotator WorldRot = Owner->GetActorRotation();
	const FVector Velocity = Owner->GetVelocity();             // cm/s
	const float    SpeedCmS  = Velocity.Size();
	const double SpeedKmh = Velocity.Size() * 0.01 * 3.6;
	const double Yaw = WorldRot.Yaw;

	const float SaveInterval = 1.0f / FMath::Max(SaveFrequencyHz, 0.1f);
	float AccelMs2 = 0.f;
	if (bHasFirstSample)
		AccelMs2 = ((SpeedCmS - PrevSpeedCmS) / SaveInterval) * 0.01f;
	
	double UtmEasting = 0.0;
	double UtmNorthing = 0.0;
	WorldToUtm(WorldLoc, UtmEasting, UtmNorthing);

	FString Row = FString::Printf(
		TEXT("%.3f,%.2f,%.2f,%.2f,%.4f,%.4f,%d,%.2f,%.4f,%.4f,%.4f"),
		ElapsedRecordingTime,
		WorldLoc.X, WorldLoc.Y, WorldLoc.Z,
		UtmEasting, UtmNorthing, OriginUtmZone,
		SpeedKmh,
		Yaw,
		AccelMs2,
		CurrentSteeringInput
	);
	
	// 센서 현재 실험 프리셋/센서 평가값만 CSV 컬럼 문자열로 제공
	if (SensorExperimentComponent)
	{
		Row += SensorExperimentComponent->BuildCsvRowColumns();
	}
	Row += LINE_TERMINATOR;

	FFileHelper::SaveStringToFile(Row, *CsvFilePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(),
		EFileWrite::FILEWRITE_Append
	);
	
	if (bHasFirstSample)
	{
		// DrawDebugLine: 속도 기반 색상 궤적
		DrawDebugLine(GetWorld(), PrevLocation, WorldLoc,
			SpeedToColor(SpeedCmS).ToFColor(true),
			true, -1.f, 0, TrailThickness);
 
		// DrawDebugPoint: 급감속 마커
		if ((PrevSpeedCmS - SpeedCmS) > HardBrakeThreshold)
		{
			DrawDebugPoint(GetWorld(),
				WorldLoc + FVector(0, 0, 50.f),
				BrakeMarkerSize, FColor::Orange, true, -1.f);
		}
	}
	
	// DrawDebugString: 일정 거리마다 속도/Yaw 수치 표시
	if (FVector::Dist(WorldLoc, LastLabelLocation) >= LabelInterval)
	{
		DrawDebugString(GetWorld(),
			WorldLoc + FVector(0, 0, 100.f),
			FString::Printf(TEXT("%.1f km/h\nYaw: %.1f deg"), SpeedKmh, Yaw),
			nullptr, FColor::White, -1.f, true);
		LastLabelLocation = WorldLoc;
	}
 
	// 상태 갱신
	PrevLocation    = WorldLoc;
	PrevSpeedCmS    = SpeedCmS;
	bHasFirstSample = true;
}

FLinearColor UAgentDataLogger::SpeedToColor(float SpeedCmS) const
{
	const float Alpha = FMath::Clamp(
		(SpeedCmS - SpeedColorMin) / FMath::Max(SpeedColorMax - SpeedColorMin, 1.f),
		0.f, 1.f);
	return FLinearColor::LerpUsingHSV(FLinearColor::Blue, FLinearColor::Red, Alpha);
}

void UAgentDataLogger::WorldToUtm(const FVector& WorldLocation,
                                  double& OutEasting, double& OutNorthing) const
{
	const double OffsetEastM  =  WorldLocation.X * 0.01;
	const double OffsetNorthM = -WorldLocation.Y * 0.01;

	OutEasting  = OriginUtmEasting  + OffsetEastM;
	OutNorthing = OriginUtmNorthing + OffsetNorthM;
}

int32 UAgentDataLogger::GetUtmZone(double Longitude)
{
	return FMath::FloorToInt((Longitude + 180.0) / 6.0) + 1;
}

void UAgentDataLogger::LatLonToUtm(double Lat, double Lon, int32 Zone, double& OutEasting, double& OutNorthing)
{
	// WGS-84 ellipsoid constants
	constexpr double a  = 6378137.0;            // semi-major axis (m)
	constexpr double f  = 1.0 / 298.257223563;  // flattening
	constexpr double k0 = 0.9996;               // UTM scale factor

	const double e2 = 2.0 * f - f * f;          // first eccentricity squared
	const double ep2 = e2 / (1.0 - e2);         // second eccentricity squared

	const double LatRad = FMath::DegreesToRadians(Lat);
	const double CentralMeridian = (Zone - 1) * 6.0 - 180.0 + 3.0;
	const double DeltaLon = FMath::DegreesToRadians(Lon - CentralMeridian);

	const double SinLat = FMath::Sin(LatRad);
	const double CosLat = FMath::Cos(LatRad);
	const double TanLat = FMath::Tan(LatRad);

	const double N = a / FMath::Sqrt(1.0 - e2 * SinLat * SinLat);
	const double T = TanLat * TanLat;
	const double C = ep2 * CosLat * CosLat;
	const double A = CosLat * DeltaLon;

	// Meridional arc (M) — series expansion
	const double e4 = e2 * e2;
	const double e6 = e4 * e2;
	const double M = a * (
		(1.0 - e2 / 4.0 - 3.0 * e4 / 64.0  - 5.0 * e6 / 256.0) * LatRad
		- (3.0 * e2 / 8.0 + 3.0 * e4 / 32.0 + 45.0 * e6 / 1024.0) * FMath::Sin(2.0 * LatRad)
		+ (15.0 * e4 / 256.0 + 45.0 * e6 / 1024.0) * FMath::Sin(4.0 * LatRad)
		- (35.0 * e6 / 3072.0) * FMath::Sin(6.0 * LatRad));

	const double A2 = A * A;
	const double A4 = A2 * A2;
	const double A6 = A4 * A2;

	OutEasting = k0 * N * (
		A
		+ (1.0 - T + C) * A2 * A / 6.0
		+ (5.0 - 18.0 * T + T * T + 72.0 * C - 58.0 * ep2) * A4 * A / 120.0
	) + 500000.0;   // false easting

	OutNorthing = k0 * (M + N * TanLat * (
		A2 / 2.0
		+ (5.0 - T + 9.0 * C + 4.0 * C * C) * A4 / 24.0
		+ (61.0 - 58.0 * T + T * T + 600.0 * C - 330.0 * ep2) * A6 / 720.0
	));

	// Southern hemisphere offset
	if (Lat < 0.0)
		OutNorthing += 10000000.0;
}
