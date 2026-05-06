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

	const FColor BgColor = Config.BackgroundColor.ToFColor(true);
	FColor* RESTRICT Pixels = PixelBuffer.GetData();
	for (int32 i = 0; i < TotalPixels; ++i)
	{
		Pixels[i] = BgColor;
	}

	const FTransform InvSensor = SensorTransform.Inverse();
	const int32 PointCount = PointCloud.PointCount;
	const FVector* RESTRICT Points = PointCloud.Points.GetData();
	const float* RESTRICT Intensities = PointCloud.Intensities.GetData();
	const int32 IntensityCount = PointCloud.Intensities.Num();

	for (int32 i = 0; i < PointCount; ++i)
	{
		const FVector LocalPt = InvSensor.TransformPosition(Points[i]);

		const int32 CX = FMath::RoundToInt32(HalfSize + LocalPt.Y * Scale);
		const int32 CY = FMath::RoundToInt32(HalfSize - LocalPt.X * Scale);

		if (CX < PtHalf || CX >= ImgSize - PtHalf || CY < PtHalf || CY >= ImgSize - PtHalf)
		{
			continue;
		}

		const float Intensity = (i < IntensityCount) ? Intensities[i] : 0.5f;
		const FColor Color = ColorLUT[
			static_cast<uint8>(FMath::Clamp(Intensity * 255.f, 0.f, 255.f))
		];

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

	const int32 C = FMath::RoundToInt32(HalfSize);
	const FColor White(255, 255, 255, 255);
	for (int32 dy = -3; dy < 3; ++dy)
	{
		const int32 Row = (C + dy) * ImgSize;
		for (int32 dx = -3; dx < 3; ++dx)
		{
			Pixels[Row + C + dx] = White;
		}
	}

	DynamicTexture->UpdateTextureRegions(
		0, 1, &UpdateRegion,
		ImgSize * sizeof(FColor),
		sizeof(FColor),
		reinterpret_cast<uint8*>(Pixels)
	);
}
