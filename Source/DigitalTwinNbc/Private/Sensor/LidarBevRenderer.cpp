// Copyright NBC, Inc. All Rights Reserved.

#include "Sensor/LidarBevRenderer.h"

#include "Engine/Texture2D.h"

void ULidarBevRenderer::Initialize(const FBevRenderConfig& InConfig)
{
	Config = InConfig;
	CreateTexture();
	BuildColorLUT();
}

void ULidarBevRenderer::CreateTexture()
{
	const int32 Size = Config.ImageSize;

	DynamicTexture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
	if (!DynamicTexture)
	{
		return;
	}

	DynamicTexture->Filter = TF_Nearest;
	DynamicTexture->SRGB = true;
	DynamicTexture->UpdateResource();

	PixelBuffer.SetNumUninitialized(Size * Size);
	UpdateRegion = FUpdateTextureRegion2D(0, 0, 0, 0, Size, Size);
}

void ULidarBevRenderer::BuildColorLUT()
{
	const FLinearColor DarkGreen(0.0f, 0.3f, 0.0f, 1.0f);
	const FLinearColor Bright = Config.PointColor;

	for (int32 i = 0; i < 256; ++i)
	{
		ColorLUT[i] = FMath::Lerp(
			DarkGreen,
			Bright,
			static_cast<float>(i) / 255.0f
		).ToFColor(true);
	}
}

void ULidarBevRenderer::UpdateConfig(const FBevRenderConfig& InConfig)
{
	const bool bSizeChanged = Config.ImageSize != InConfig.ImageSize;

	Config = InConfig;
	BuildColorLUT();

	if (bSizeChanged)
	{
		CreateTexture();
	}
}

void ULidarBevRenderer::DrawPoint(
	int32 CenterX,
	int32 CenterY,
	const FColor& Color,
	int32 PointSize
)
{
	const int32 ImgSize = Config.ImageSize;
	const int32 PtSize = FMath::Max(PointSize, 1);
	const int32 PtHalf = PtSize / 2;

	if (
		CenterX < PtHalf ||
		CenterX >= ImgSize - PtHalf ||
		CenterY < PtHalf ||
		CenterY >= ImgSize - PtHalf
	)
	{
		return;
	}

	FColor* RESTRICT Pixels = PixelBuffer.GetData();
	if (!Pixels)
	{
		return;
	}

	if (PtSize == 1)
	{
		Pixels[CenterY * ImgSize + CenterX] = Color;
		return;
	}

	for (int32 Dy = -PtHalf; Dy < PtSize - PtHalf; ++Dy)
	{
		const int32 Row = (CenterY + Dy) * ImgSize;

		for (int32 Dx = -PtHalf; Dx < PtSize - PtHalf; ++Dx)
		{
			Pixels[Row + CenterX + Dx] = Color;
		}
	}
}

void ULidarBevRenderer::DrawGrid()
{
	const int32 ImgSize = Config.ImageSize;
	const float HalfSize = static_cast<float>(ImgSize) * 0.5f;
	const float Scale = HalfSize / FMath::Max(Config.ViewRange, 1.0f);

	FColor* RESTRICT Pixels = PixelBuffer.GetData();
	if (!Pixels)
	{
		return;
	}

	const FColor GridLineColor(60, 60, 60, 255);

	// Unreal 단위 기준: 100cm = 1m, 10m = 1000cm
	const float GridWorldStepCm = 1000.0f;
	const int32 GridPixelStep = FMath::RoundToInt32(GridWorldStepCm * Scale);

	if (GridPixelStep <= 0)
	{
		return;
	}

	const int32 Center = ImgSize / 2;

	for (int32 Offset = 0; Offset < ImgSize / 2; Offset += GridPixelStep)
	{
		const int32 X1 = Center + Offset;
		const int32 X2 = Center - Offset;
		const int32 Y1 = Center + Offset;
		const int32 Y2 = Center - Offset;

		for (int32 i = 0; i < ImgSize; ++i)
		{
			if (X1 >= 0 && X1 < ImgSize)
			{
				Pixels[i * ImgSize + X1] = GridLineColor;
			}

			if (X2 >= 0 && X2 < ImgSize)
			{
				Pixels[i * ImgSize + X2] = GridLineColor;
			}

			if (Y1 >= 0 && Y1 < ImgSize)
			{
				Pixels[Y1 * ImgSize + i] = GridLineColor;
			}

			if (Y2 >= 0 && Y2 < ImgSize)
			{
				Pixels[Y2 * ImgSize + i] = GridLineColor;
			}
		}
	}
}

void ULidarBevRenderer::DrawVehicleArrow()
{
	const int32 ImgSize = Config.ImageSize;
	const int32 Center = ImgSize / 2;

	FColor* RESTRICT Pixels = PixelBuffer.GetData();
	if (!Pixels)
	{
		return;
	}

	const FColor ArrowColor(255, 255, 255, 255);

	// 차량 중심 몸통: 위쪽이 차량 전방
	for (int32 Y = -18; Y <= 8; ++Y)
	{
		const int32 Width = Y < -8 ? 1 : 4;

		for (int32 X = -Width; X <= Width; ++X)
		{
			const int32 PX = Center + X;
			const int32 PY = Center + Y;

			if (PX >= 0 && PX < ImgSize && PY >= 0 && PY < ImgSize)
			{
				Pixels[PY * ImgSize + PX] = ArrowColor;
			}
		}
	}

	// 화살표 머리
	for (int32 Y = -24; Y <= -16; ++Y)
	{
		const int32 Width = FMath::Abs(Y + 24);

		for (int32 X = -Width; X <= Width; ++X)
		{
			const int32 PX = Center + X;
			const int32 PY = Center + Y;

			if (PX >= 0 && PX < ImgSize && PY >= 0 && PY < ImgSize)
			{
				Pixels[PY * ImgSize + PX] = ArrowColor;
			}
		}
	}
}

FColor ULidarBevRenderer::GetHeightColor(float LocalZ) const
{
	// 높이 기반 색상 범위.
	// 필요하면 이후 FBevRenderConfig로 빼서 에디터에서 조절 가능하게 만들 수 있음.
	constexpr float MinZ = -200.0f;
	constexpr float MaxZ = 300.0f;

	const float ZAlpha = FMath::GetMappedRangeValueClamped(
		FVector2D(MinZ, MaxZ),
		FVector2D(0.0f, 1.0f),
		LocalZ
	);

	const uint8 ColorIndex = static_cast<uint8>(
		FMath::Clamp(ZAlpha * 255.0f, 0.0f, 255.0f)
	);

	return ColorLUT[ColorIndex];
}

void ULidarBevRenderer::RenderPointCloud(
	const FLidarPointCloudData& PointCloud,
	const FTransform& SensorTransform
)
{
	if (!DynamicTexture)
	{
		return;
	}

	const int32 ImgSize = Config.ImageSize;
	const int32 TotalPixels = ImgSize * ImgSize;
	const float HalfSize = static_cast<float>(ImgSize) * 0.5f;
	const float Scale = HalfSize / FMath::Max(Config.ViewRange, 1.0f);

	const int32 NormalPointSize = FMath::Max(FMath::RoundToInt32(Config.PointSize), 1);
	const int32 ObstaclePointSize = NormalPointSize + 1;

	const FColor BgColor = Config.BackgroundColor.ToFColor(true);
	FColor* RESTRICT Pixels = PixelBuffer.GetData();

	if (!Pixels)
	{
		return;
	}

	// 1. 배경 초기화
	for (int32 i = 0; i < TotalPixels; ++i)
	{
		Pixels[i] = BgColor;
	}

	// 2. 10m 간격 그리드 오버레이
	DrawGrid();

	const FTransform InvSensor = SensorTransform.Inverse();
	const int32 PointCount = PointCloud.PointCount;
	const FVector* RESTRICT Points = PointCloud.Points.GetData();

	if (!Points || PointCount <= 0)
	{
		DrawVehicleArrow();

		DynamicTexture->UpdateTextureRegions(
			0,
			1,
			&UpdateRegion,
			ImgSize * sizeof(FColor),
			sizeof(FColor),
			reinterpret_cast<uint8*>(Pixels)
		);

		return;
	}

	// 3. LiDAR 포인트 렌더링
	for (int32 i = 0; i < PointCount; ++i)
	{
		const FVector LocalPt = InvSensor.TransformPosition(Points[i]);

		const int32 CX = FMath::RoundToInt32(HalfSize + LocalPt.Y * Scale);
		const int32 CY = FMath::RoundToInt32(HalfSize - LocalPt.X * Scale);

		if (CX < 0 || CX >= ImgSize || CY < 0 || CY >= ImgSize)
		{
			continue;
		}

		const bool bObstacle =
			Config.bDrawObstacles &&
			PointCloud.ObstacleFlags.IsValidIndex(i) &&
			PointCloud.ObstacleFlags[i] != 0;

		const FColor Color = bObstacle
			? Config.ObstacleColor.ToFColor(true)
			: GetHeightColor(LocalPt.Z);

		DrawPoint(
			CX,
			CY,
			Color,
			bObstacle ? ObstaclePointSize : NormalPointSize
		);
	}

	// 4. 차량 방향 화살표
	DrawVehicleArrow();

	// 5. 텍스처 갱신
	DynamicTexture->UpdateTextureRegions(
		0,
		1,
		&UpdateRegion,
		ImgSize * sizeof(FColor),
		sizeof(FColor),
		reinterpret_cast<uint8*>(Pixels)
	);
}