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
        // ���н� ������
        bCachedPoints = true;
        CachedStart = GetActorLocation();
        CachedEnd = CachedStart + FVector(0, 800, 0);
        return FTransform(GetActorRotation(), CachedStart);
    }

    int32 W = 0, H = 0;
    PC->GetViewportSize(W, H);
    if (W <= 0 || H <= 0)
    {
        // ���н� ������
        bCachedPoints = true;
        CachedStart = GetActorLocation();
        CachedEnd = CachedStart + FVector(0, 800, 0);
        return FTransform(GetActorRotation(), CachedStart);
    }

    // ȭ�� �����ڸ�(��/�Ʒ�) �ʹ� ���� �ʰ� ������ ��
    const int32 YMarginPx = FMath::Max(8, MarginPx);
    auto RandY = [&]() -> float {
        return (float)FMath::RandRange(YMarginPx, H - YMarginPx);
        };

    // ����/�� ���� �������� Y ��ġ
    const float YStart = RandY();
    const float YEnd = RandY();

    // ��/�� ȭ�� ���� ��ũ�� ��ǥ
    const FVector2D LeftStartScreen = FVector2D(-MarginPx, YStart);
    const FVector2D RightEndScreen = FVector2D(W + MarginPx, YEnd);
    const FVector2D RightStartScreen = FVector2D(W + MarginPx, YStart);
    const FVector2D LeftEndScreen = FVector2D(-MarginPx, YEnd);

    FVector L_Start3D, R_End3D, R_Start3D, L_End3D;
    const bool bLStartOK = DeprojectToXPlane(PC, LeftStartScreen, DepthX, L_Start3D);
    const bool bREndOK = DeprojectToXPlane(PC, RightEndScreen, DepthX, R_End3D);
    const bool bRStartOK = DeprojectToXPlane(PC, RightStartScreen, DepthX, R_Start3D);
    const bool bLEndOK = DeprojectToXPlane(PC, LeftEndScreen, DepthX, L_End3D);

    // �� ���� ��� �õ� �����ϵ��� �غ�
    const bool bLeftToRightOK = bLStartOK && bREndOK;
    const bool bRightToLeftOK = bRStartOK && bLEndOK;

    if (bLeftToRightOK || bRightToLeftOK)
    {
        const bool bLeftToRight = bLeftToRightOK && bRightToLeftOK
            ? FMath::RandBool()        // �� �� ���� �� ���� ����
            : bLeftToRightOK;          // ���ʸ� ���� �� ��������

        if (bLeftToRight)
        {
            CachedStart = L_Start3D;   // ���� �� @ YStart
            CachedEnd = R_End3D;     // ������ �� @ YEnd
        }
        else
        {
            CachedStart = R_Start3D;   // ������ �� @ YStart
            CachedEnd = L_End3D;     // ���� �� @ YEnd
        }

        bCachedPoints = true;
        return FTransform(FRotator::ZeroRotator, CachedStart);
    }

    // ���н� ������(����� ��ü)
    bCachedPoints = true;
    CachedStart = GetActorLocation();
    CachedEnd = CachedStart + FVector(0, 800, 0);
    return FTransform(GetActorRotation(), CachedStart);
}

void AFlyingEnemySpawner::ConfigureSpawnedEnemy_Implementation(AEnemyBase* NewEnemy)
{
	if (!NewEnemy) return;

	// Ȥ�� ĳ�ð� ������� �� �� �� ��� �õ�
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

	// ���� ��ġ�� ����(���� Deferred �� �ѱ� Transform�� �ٸ���)
	NewEnemy->SetActorLocation(CachedStart);

	// AFlyingEnemy��� ���/�ӵ� Init
	if (AFlyingEnemy* FE = Cast<AFlyingEnemy>(NewEnemy))
	{
		FE->Init(CachedStart, CachedEnd);
	}
	// �� �� Ÿ���̸� �ʿ� �� ���� Init �������̽��� �߰��ص� ��.
}
