// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/EnemySpawner.h"
#include "FlyingEnemySpawner.generated.h"

/**
 * 
 */
UCLASS()
class COMICCON_API AFlyingEnemySpawner : public AEnemySpawner
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Flying")
	float DepthX = 400.f;

	/** 화면 밖 판정을 위한 픽셀 마진 */
	UPROPERTY(EditAnywhere, Category = "Flying")
	int32 MarginPx = 100;

protected:
	virtual FTransform GetSpawnTransform_Implementation() override;
	virtual void ConfigureSpawnedEnemy_Implementation(AEnemyBase* NewEnemy) override;

private:
	// GetSpawnTransform에서 계산 → Configure에서 사용
	uint8 bCachedPoints : 1 = false;
	FVector CachedStart = FVector::ZeroVector;
	FVector CachedEnd = FVector::ZeroVector;
};
