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
			static_cast<float>(i) / 255.f
		).ToFColor(true);
	}
}

void ULidarBevRenderer::UpdateConfig(const FBevRenderConfig& InConfig)
{
	const bool bSizeChanged = (Config.ImageSize != InConfig.ImageSize);
	Config = InConfig;
	BuildColorLUT();

	if (bSizeChanged)
	{
		CreateTexture();
	}
}

void ULidarBevRenderer::RenderPointCloud(const FLidarPointCloudData& PointCloud, const FTransform& SensorTransform)
{
	if (!DynamicTexture) return;

	const int32 ImgSize = Config.ImageSize;
	const int32 TotalPixels = ImgSize * ImgSize;
	const float HalfSize = static_cast<float>(ImgSize) * 0.5f;
	const float Scale = HalfSize / Config.ViewRange;
	const int32 PtSize = FMath::Max(Config.PointSize, 1);
	const int32 PtHalf = PtSize / 2;

	FColor* RESTRICT Pixels = PixelBuffer.GetData();
	if (!Pixels) return;

	// 1) 배경 칠하기
	const FColor BgColor = Config.BackgroundColor.ToFColor(true);
	for (int32 i = 0; i < TotalPixels; ++i)
	{
		Pixels[i] = BgColor;
	}

	// 2) 그리드 먼저 그리기
	DrawGrid(Pixels, ImgSize, Scale);

	const FTransform InvSensor = SensorTransform.Inverse();
	const int32 PointCount = PointCloud.PointCount;
	const FVector* RESTRICT Points = PointCloud.Points.GetData();

	for (int32 i = 0; i < PointCount; ++i)
	{
		const FVector LocalPt = InvSensor.TransformPosition(Points[i]);

		const int32 CX = FMath::RoundToInt32(HalfSize + LocalPt.Y * Scale);
		const int32 CY = FMath::RoundToInt32(HalfSize - LocalPt.X * Scale);

		if (CX < PtHalf || CX >= ImgSize - PtHalf || CY < PtHalf || CY >= ImgSize - PtHalf)
		{
			continue;
		}

		// 3) 높이(Z) 기반 컬러맵
		const float MinZ = -200.0f;
		const float MaxZ = 300.0f;

		const float ZAlpha = FMath::GetMappedRangeValueClamped(
			FVector2D(MinZ, MaxZ),
			FVector2D(0.0f, 1.0f),
			LocalPt.Z
		);

		const FColor Color = ColorLUT[
			static_cast<uint8>(FMath::Clamp(ZAlpha * 255.f, 0.f, 255.f))
		];

		// 4) 포인트 찍기
		if (PtSize == 1)
		{
			Pixels[CY * ImgSize + CX] = Color;
		}
		else
		{
			for (int32 dy = -PtHalf; dy < PtSize - PtHalf; ++dy)
			{
				const int32 Row = (CY + dy) * ImgSize;
				for (int32 dx = -PtHalf; dx < PtSize - PtHalf; ++dx)
				{
					Pixels[Row + CX + dx] = Color;
				}
			}
		}
	}

	// 5) 차량 방향 화살표는 맨 마지막
	DrawVehicleArrow(Pixels, ImgSize);

	// 6) 텍스처 갱신
	DynamicTexture->UpdateTextureRegions(
		0, 1, &UpdateRegion,
		ImgSize * sizeof(FColor),
		sizeof(FColor),
		reinterpret_cast<uint8*>(Pixels)
	);
}

void ULidarBevRenderer::DrawGrid(FColor* Pixels, int32 ImgSize, float Scale)
{
	const FColor GridColor(60, 60, 60, 255);

	// Unreal 단위: 100cm = 1m
	// 10m = 1000cm
	const float GridWorldStep = 1000.0f;
	const int32 GridPixelStep = FMath::RoundToInt32(GridWorldStep * Scale);

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
			if (X1 >= 0 && X1 < ImgSize) Pixels[i * ImgSize + X1] = GridColor;
			if (X2 >= 0 && X2 < ImgSize) Pixels[i * ImgSize + X2] = GridColor;
			if (Y1 >= 0 && Y1 < ImgSize) Pixels[Y1 * ImgSize + i] = GridColor;
			if (Y2 >= 0 && Y2 < ImgSize) Pixels[Y2 * ImgSize + i] = GridColor;
		}
	}
}

void ULidarBevRenderer::DrawVehicleArrow(FColor* Pixels, int32 ImgSize)
{
	const int32 C = ImgSize / 2;
	const FColor ArrowColor(255, 255, 255, 255);


	for (int32 y = -18; y <= 8; ++y)
	{
		const int32 Width = y < -8 ? 1 : 4;

		for (int32 x = -Width; x <= Width; ++x)
		{
			const int32 PX = C + x;
			const int32 PY = C + y;

			if (PX >= 0 && PX < ImgSize && PY >= 0 && PY < ImgSize)
			{
				Pixels[PY * ImgSize + PX] = ArrowColor;
			}
		}
	}

	// 화살표 머리
	for (int32 y = -24; y <= -16; ++y)
	{
		const int32 Width = FMath::Abs(y + 24);

		for (int32 x = -Width; x <= Width; ++x)
		{
			const int32 PX = C + x;
			const int32 PY = C + y;

			if (PX >= 0 && PX < ImgSize && PY >= 0 && PY < ImgSize)
			{
				Pixels[PY * ImgSize + PX] = ArrowColor;
			}
		}
	}
}