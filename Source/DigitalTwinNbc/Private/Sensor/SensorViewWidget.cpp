// Copyright NBC, Inc. All Rights Reserved.

#include "Sensor/SensorViewWidget.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"

void USensorViewWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SensorBorder)
		SensorBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (LidarBorder)
		LidarBorder->SetVisibility(ESlateVisibility::Collapsed);
}

void USensorViewWidget::SetRenderTarget(UTextureRenderTarget2D* InRenderTarget)
{
	if (!InRenderTarget || !SensorImage)
		return;
	
	SensorImage->SetBrushResourceObject(InRenderTarget);
}

void USensorViewWidget::SetLidarRenderTarget(UTexture2D* InRenderTarget)
{
	if (!InRenderTarget || !LidarImage)
		return;
	
	LidarImage->SetBrushResourceObject(InRenderTarget);
}

void USensorViewWidget::ToggleCameraView()
{
	if (!SensorBorder)
		return;

	const ESlateVisibility NewVis = IsCameraViewVisible()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible;

	SensorBorder->SetVisibility(NewVis);
}

void USensorViewWidget::ToggleLidarView()
{
	if (!LidarBorder)
		return;

	const ESlateVisibility NewVis = IsLidarViewVisible()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible;

	LidarBorder->SetVisibility(NewVis);
}

bool USensorViewWidget::IsCameraViewVisible() const
{
	return SensorBorder && SensorBorder->GetVisibility() != ESlateVisibility::Collapsed;
}

bool USensorViewWidget::IsLidarViewVisible() const
{
	return LidarBorder && LidarBorder->GetVisibility() != ESlateVisibility::Collapsed;
}
