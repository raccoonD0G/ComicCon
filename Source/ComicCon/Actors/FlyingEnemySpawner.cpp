// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/FlyingEnemySpawner.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Characters/EnemyBase.h"
#include "Characters/FlyingEnemy.h"
#include "Utils.h"

FTransform AFlyingEnemySpawner::GetSpawnTransform_Implementation()
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC)
    {
        // 실패시 안전망
        bCachedPoints = true;
        CachedStart = GetActorLocation();
        CachedEnd = CachedStart + FVector(0, 800, 0);
        return FTransform(GetActorRotation(), CachedStart);
    }

    int32 W = 0, H = 0;
    PC->GetViewportSize(W, H);
    if (W <= 0 || H <= 0)
    {
        // 실패시 안전망
        bCachedPoints = true;
        CachedStart = GetActorLocation();
        CachedEnd = CachedStart + FVector(0, 800, 0);
        return FTransform(GetActorRotation(), CachedStart);
    }

    // 화면 가장자리(위/아래) 너무 붙지 않게 여유를 둠
    const int32 YMarginPx = FMath::Max(8, MarginPx);
    auto RandY = [&]() -> float {
        return (float)FMath::RandRange(YMarginPx, H - YMarginPx);
        };

    // 시작/끝 각각 독립적인 Y 위치
    const float YStart = RandY();
    const float YEnd = RandY();

    // 왼/오 화면 밖의 스크린 좌표
    const FVector2D LeftStartScreen = FVector2D(-MarginPx, YStart);
    const FVector2D RightEndScreen = FVector2D(W + MarginPx, YEnd);
    const FVector2D RightStartScreen = FVector2D(W + MarginPx, YStart);
    const FVector2D LeftEndScreen = FVector2D(-MarginPx, YEnd);

    FVector L_Start3D, R_End3D, R_Start3D, L_End3D;
    const bool bLStartOK = DeprojectToXPlane(PC, LeftStartScreen, DepthX, L_Start3D);
    const bool bREndOK = DeprojectToXPlane(PC, RightEndScreen, DepthX, R_End3D);
    const bool bRStartOK = DeprojectToXPlane(PC, RightStartScreen, DepthX, R_Start3D);
    const bool bLEndOK = DeprojectToXPlane(PC, LeftEndScreen, DepthX, L_End3D);

    // 두 방향 모두 시도 가능하도록 준비
    const bool bLeftToRightOK = bLStartOK && bREndOK;
    const bool bRightToLeftOK = bRStartOK && bLEndOK;

    if (bLeftToRightOK || bRightToLeftOK)
    {
        const bool bLeftToRight = bLeftToRightOK && bRightToLeftOK
            ? FMath::RandBool()        // 둘 다 가능 → 랜덤 선택
            : bLeftToRightOK;          // 한쪽만 가능 → 그쪽으로

        if (bLeftToRight)
        {
            CachedStart = L_Start3D;   // 왼쪽 변 @ YStart
            CachedEnd = R_End3D;     // 오른쪽 변 @ YEnd
        }
        else
        {
            CachedStart = R_Start3D;   // 오른쪽 변 @ YStart
            CachedEnd = L_End3D;     // 왼쪽 변 @ YEnd
        }

        bCachedPoints = true;
        return FTransform(FRotator::ZeroRotator, CachedStart);
    }

    // 실패시 안전망(가운데로 대체)
    bCachedPoints = true;
    CachedStart = GetActorLocation();
    CachedEnd = CachedStart + FVector(0, 800, 0);
    return FTransform(GetActorRotation(), CachedStart);
}

void AFlyingEnemySpawner::ConfigureSpawnedEnemy_Implementation(AEnemyBase* NewEnemy)
{
	if (!NewEnemy) return;

	// 혹시 캐시가 비었으면 한 번 더 계산 시도
	if (!bCachedPoints)
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
		FVector L, R;
		if (ComputeEdgePointsAtX(PC, DepthX, MarginPx, L, R))
		{
			const bool bLeftToRight = FMath::RandBool();
			CachedStart = bLeftToRight ? L : R;
			CachedEnd = bLeftToRight ? R : L;
			bCachedPoints = true;
		}
		else
		{
			CachedStart = GetActorLocation();
			CachedEnd = CachedStart + FVector(0, 800, 0);
			bCachedPoints = true;
		}
	}

	// 스폰 위치를 보정(만약 Deferred 시 넘긴 Transform과 다르면)
	NewEnemy->SetActorLocation(CachedStart);

	// AFlyingEnemy라면 경로/속도 Init
	if (AFlyingEnemy* FE = Cast<AFlyingEnemy>(NewEnemy))
	{
		FE->Init(CachedStart, CachedEnd);
	}
	// 그 외 타입이면 필요 시 공통 Init 인터페이스를 추가해도 됨.
}
