// Copyright NBC, Inc. All Rights Reserved.

#include "Sensor/SensorSyncComponent.h"
#include "Sensor/CameraSensorComponent.h"
#include "Sensor/LidarSensorComponent.h"

USensorSyncComponent::USensorSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USensorSyncComponent::InitializeSensors(UCameraSensorComponent* InCameraSensor, ULidarSensorComponent* InLidarSensor)
{
	CameraSensor = InCameraSensor;
	LidarSensor = InLidarSensor;
}

FSensorSyncResult USensorSyncComponent::CheckSync() const
{
	FSensorSyncResult Result;

	if (!CameraSensor || !LidarSensor)
	{
		return Result;
	}

	Result.CameraTimestamp = CameraSensor->GetLastCaptureTimestamp();
	Result.LidarTimestamp = LidarSensor->GetLastScanTimestamp();
	Result.DeltaSeconds = FMath::Abs(Result.CameraTimestamp - Result.LidarTimestamp);
	Result.bSynced = Result.DeltaSeconds <= MaxSyncDeltaSeconds;

	return Result;
}
