// Copyright NBC, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SensorViewWidget.generated.h"

class UImage;
class UBorder;
class UTextureRenderTarget2D;

UCLASS()
class DIGITALTWINNBC_API USensorViewWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CameraSensor")
	void SetRenderTarget(UTextureRenderTarget2D* InRenderTarget);

	UFUNCTION(BlueprintCallable, Category = "CameraSensor")
	void SetLidarRenderTarget(UTexture2D* InRenderTarget);

	UFUNCTION(BlueprintCallable, Category = "CameraSensor")
	void ToggleCameraView();

	UFUNCTION(BlueprintCallable, Category = "CameraSensor")
	void ToggleLidarView();

	UFUNCTION(BlueprintPure, Category = "CameraSensor")
	bool IsCameraViewVisible() const;

	UFUNCTION(BlueprintPure, Category = "CameraSensor")
	bool IsLidarViewVisible() const;

protected:
	virtual void NativeConstruct() override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> SensorBorder;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> SensorImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> LidarBorder;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> LidarImage;
};
