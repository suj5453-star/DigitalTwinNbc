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
		ColorLUT[i] = FMath::Lerp(DarkGreen, Bright, static_cast<float>(i) / 255.0f).ToFColor(true);
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

void ULidarBevRenderer::DrawPoint(int32 CenterX, int32 CenterY, const FColor& Color, int32 PointSize)
{
	const int32 ImgSize = Config.ImageSize;
	const int32 PtSize = FMath::Max(PointSize, 1);
	const int32 PtHalf = PtSize / 2;

	if (CenterX < PtHalf || CenterX >= ImgSize - PtHalf || CenterY < PtHalf || CenterY >= ImgSize - PtHalf)
	{
		return;
	}

	FColor* RESTRICT Pixels = PixelBuffer.GetData();

	if (PtSize == 1)
	{
		Pixels[CenterY * ImgSize + CenterX] = Color;
		return;
	}

	for (int32 dy = -PtHalf; dy < PtSize - PtHalf; ++dy)
	{
		const int32 Row = (CenterY + dy) * ImgSize;
		for (int32 dx = -PtHalf; dx < PtSize - PtHalf; ++dx)
		{
			Pixels[Row + CenterX + dx] = Color;
		}
	}
}

void ULidarBevRenderer::RenderPointCloud(const FLidarPointCloudData& PointCloud, const FTransform& SensorTransform)
{
	if (!DynamicTexture)
	{
		return;
	}

	const int32 ImgSize = Config.ImageSize;
	const int32 TotalPixels = ImgSize * ImgSize;
	const float HalfSize = static_cast<float>(ImgSize) * 0.5f;
	const float Scale = HalfSize / Config.ViewRange;
	const int32 NormalPointSize = FMath::Max(FMath::RoundToInt32(Config.PointSize), 1);
	const int32 ObstaclePointSize = NormalPointSize + 1;

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

		const float Intensity = (i < IntensityCount) ? Intensities[i] : 0.5f;
		const bool bObstacle = Config.bDrawObstacles
			&& PointCloud.ObstacleFlags.IsValidIndex(i)
			&& PointCloud.ObstacleFlags[i] != 0;

		const FColor Color = bObstacle
			? Config.ObstacleColor.ToFColor(true)
			: ColorLUT[static_cast<uint8>(FMath::Clamp(Intensity * 255.0f, 0.0f, 255.0f))];

		DrawPoint(CX, CY, Color, bObstacle ? ObstaclePointSize : NormalPointSize);
	}

	const int32 Center = FMath::RoundToInt32(HalfSize);
	const FColor White(255, 255, 255, 255);
	DrawPoint(Center, Center, White, 6);

	DynamicTexture->UpdateTextureRegions(
		0,
		1,
		&UpdateRegion,
		ImgSize * sizeof(FColor),
		sizeof(FColor),
		reinterpret_cast<uint8*>(Pixels)
	);
}
