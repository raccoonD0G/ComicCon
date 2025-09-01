// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/MonsterSpawner.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"

AMonsterSpawner::AMonsterSpawner()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AMonsterSpawner::BeginPlay()
{
    Super::BeginPlay();

    // 0.1초마다 초기화 시도
    GetWorldTimerManager().SetTimer(InitRetryTimer, this, &AMonsterSpawner::TryInitFromViewport, 0.1f, true);
    ScheduleNextSpawn();     // 타이머 시작
}

void AMonsterSpawner::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AMonsterSpawner::SpawnAtViewportEdges()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC || !ActorToSpawn) return;

    PC->GetViewportSize(CachedViewX, CachedViewY);
    if (CachedViewX <= 0 || CachedViewY <= 0) return;

    FVector CenterLoc, CenterDir;
    PC->DeprojectScreenPositionToWorld(CachedViewX / 2.f, CachedViewY / 2.f, CenterLoc, CenterDir);
    CachedCenterTarget = CenterLoc + CenterDir * SpawnDistance;

    BuildScreenPoints();

    // 슬롯 초기화
    SlotOccupants.Init(nullptr, ScreenPoints.Num());
}

void AMonsterSpawner::BuildScreenPoints()
{
    ScreenPoints.Reset(); ScreenPoints.Reserve(8);
    const int32 yTop = 0, yMid = CachedViewY / 2, yBot = CachedViewY, xMid = CachedViewX / 2;

    // 좌/우 3개씩 + 상/하 중앙
    ScreenPoints.Add({ 0,          yTop });
    ScreenPoints.Add({ 0,          yMid });
    ScreenPoints.Add({ 0,          yBot });
    ScreenPoints.Add({ CachedViewX,yTop });
    ScreenPoints.Add({ CachedViewX,yMid });
    ScreenPoints.Add({ CachedViewX,yBot });
    ScreenPoints.Add({ xMid,       yTop });
    ScreenPoints.Add({ xMid,       yBot });
}

void AMonsterSpawner::TryInitFromViewport()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC || !ActorToSpawn) return;

    int32 X = 0, Y = 0;
    PC->GetViewportSize(X, Y);
    if (X <= 0 || Y <= 0) return;

    FVector CenterLoc, CenterDir;
    if (!PC->DeprojectScreenPositionToWorld(X / 2.f, Y / 2.f, CenterLoc, CenterDir))
        return;

    // 초기화 성공
    CachedViewX = X;
    CachedViewY = Y;
    CachedCenterTarget = CenterLoc + CenterDir * SpawnDistance;

    BuildScreenPoints();
    SlotOccupants.Init(nullptr, ScreenPoints.Num());

    UE_LOG(LogTemp, Log, TEXT("MonsterSpawner init success: %dx%d"), X, Y);

    // 타이머 정지 → 더 이상 재시도 안 함
    GetWorldTimerManager().ClearTimer(InitRetryTimer);
}

void AMonsterSpawner::ScheduleNextSpawn()
{
    // EMA(LifeEMA)가 짧을수록 interval을 짧게.
    // t = 0..1 로 정규화(EMA가 ShortLife일 때 t≈1, LongLife일 때 t≈0)
    const float t = 1.f - FMath::Clamp((static_cast<float>(LifeEMA) - ShortLifeSeconds) / (LongLifeSeconds - ShortLifeSeconds), 0.f, 1.f);
    const float NextInterval = FMath::Lerp(MaxSpawnInterval, MinSpawnInterval, t);

    GetWorldTimerManager().ClearTimer(SpawnTimer);
    GetWorldTimerManager().SetTimer(SpawnTimer, this, &AMonsterSpawner::SpawnOne, NextInterval, false);
}

void AMonsterSpawner::SpawnOne()
{
    if (!ActorToSpawn || ScreenPoints.Num() == 0) { ScheduleNextSpawn(); return; }

    // 1) 비어있는 슬롯 찾기
    TArray<int32> FreeSlots;
    for (int32 i = 0; i < SlotOccupants.Num(); i++)
    {
        if (!SlotOccupants[i].IsValid()) // 죽었거나 비어있음
        {
            FreeSlots.Add(i);
        }
    }

    if (FreeSlots.Num() == 0)
    {
        // 모든 슬롯이 차 있음 → 그냥 다음 스폰 예약만
        ScheduleNextSpawn();
        return;
    }

    // 2) 랜덤 슬롯 선택
    const int32 SlotIdx = FreeSlots[FMath::RandRange(0, FreeSlots.Num() - 1)];
    const FIntPoint& P = ScreenPoints[SlotIdx];

    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    FVector WorldLoc, WorldDir;
    if (!PC->DeprojectScreenPositionToWorld((float)P.X, (float)P.Y, WorldLoc, WorldDir))
    {
        ScheduleNextSpawn();
        return;
    }

    const FVector SpawnPos = WorldLoc + WorldDir * SpawnDistance;

    // 머리(Z축)가 중앙 타깃을 향하도록 회전
    const FVector DesiredZ = (CachedCenterTarget - SpawnPos).GetSafeNormal();
    const FVector RefX = FVector::ForwardVector;
    FVector SafeX = (FMath::Abs(FVector::DotProduct(DesiredZ, RefX)) > 0.999f) ? FVector::RightVector : RefX;
    const FRotator SpawnRot = UKismetMathLibrary::MakeRotFromZX(DesiredZ, -SafeX);

    // 스폰
    AActor* Spawned = GetWorld()->SpawnActor<AActor>(ActorToSpawn, SpawnPos, SpawnRot);
    if (Spawned)
    {
        // 슬롯에 등록
        SlotOccupants[SlotIdx] = Spawned;

        const double Now = FPlatformTime::Seconds();
        SpawnTimeByActor.Add(Spawned, Now);

        Spawned->OnDestroyed.AddDynamic(this, &AMonsterSpawner::HandleMonsterDestroyed);
    }

    ScheduleNextSpawn();
}

void AMonsterSpawner::HandleMonsterDestroyed(AActor* DestroyedActor)
{
    // 생존시간 기록
    double* StartPtr = SpawnTimeByActor.Find(DestroyedActor);
    if (StartPtr)
    {
        const double Life = FPlatformTime::Seconds() - *StartPtr;
        SpawnTimeByActor.Remove(DestroyedActor);
        LifeEMA = EMAlpha * Life + (1.0 - EMAlpha) * LifeEMA;
    }

    // 슬롯 비우기
    for (int32 i = 0; i < SlotOccupants.Num(); i++)
    {
        if (SlotOccupants[i].Get() == DestroyedActor)
        {
            SlotOccupants[i] = nullptr;
            break;
        }
    }

    ScheduleNextSpawn();
}