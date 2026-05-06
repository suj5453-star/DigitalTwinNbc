// Copyright NBC, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DigitalTwinNbcPawn.h"
#include "DigitalTwinNbcOffroadCar.generated.h"

UCLASS(abstract)
class DIGITALTWINNBC_API ADigitalTwinNbcOffroadCar : public ADigitalTwinNbcPawn
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Meshes, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Chassis;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Meshes, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* TireFrontLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Meshes, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* TireFrontRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Meshes, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* TireRearLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Meshes, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* TireRearRight;

public:

	ADigitalTwinNbcOffroadCar();
};
