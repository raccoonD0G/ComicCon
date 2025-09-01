// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MonsterSpawner.generated.h"

UCLASS()
class COMICCON_API AMonsterSpawner : public AActor
{
    GENERATED_BODY()

public:
    AMonsterSpawner();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UFUNCTION()
    void SpawnAtViewportEdges(); // 초기 8점 계산

    // === 신규: 한 마리만 스폰하는 함수(타이머에서 호출) ===
    void SpawnOne();
    void ScheduleNextSpawn();

    // === 몬스터 파괴시 콜백 ===
	UFUNCTION()
    void HandleMonsterDestroyed(AActor* DestroyedActor);

    // === 유틸 ===
    void BuildScreenPoints();      // 8개 스크린 포인트 재계산

protected:
    UPROPERTY(EditAnywhere, Category = "Spawn")
    TSubclassOf<AActor> ActorToSpawn;

    UPROPERTY(EditAnywhere, Category = "Spawn")
    float SpawnDistance = 500.f;

    // ----- 스폰 간격 튜닝 값 -----
    UPROPERTY(EditAnywhere, Category = "AdaptiveSpawn")
    float MinSpawnInterval = 0.3f;     // 너무 빠른 스폰의 하한

    UPROPERTY(EditAnywhere, Category = "AdaptiveSpawn")
    float MaxSpawnInterval = 5.0f;     // 느린 스폰의 상한

    // “빠른 죽음 / 느린 죽음”의 기준(초). EMA가 이보다 짧으면 더 자주 스폰.
    UPROPERTY(EditAnywhere, Category = "AdaptiveSpawn")
    float ShortLifeSeconds = 1.0f;     // 매우 빨리 죽는다 판단 기준
    UPROPERTY(EditAnywhere, Category = "AdaptiveSpawn")
    float LongLifeSeconds = 10.0f;    // 오래 산다 판단 기준

    // EMA 가중치 (0~1). 높을수록 최신값 반영 크게.
    UPROPERTY(EditAnywhere, Category = "AdaptiveSpawn")
    float EMAlpha = 0.35f;

private:
    // 현재 뷰포트 8지점 (좌우 3 + 상/하 중앙)
    TArray<FIntPoint> ScreenPoints;

    // 카메라 중앙 타깃 (뷰포트 중앙에서 앞 500)
    FVector CachedCenterTarget = FVector::ZeroVector;
    int32 CachedViewX = 0, CachedViewY = 0;

    // 타이머
    FTimerHandle SpawnTimer;

    // 몬스터 생존시간 통계
    double LifeEMA = 5.0; // 초기값(중간 정도)

    // 스폰 시간 기록
    TMap<TWeakObjectPtr<AActor>, double> SpawnTimeByActor;

    // 각 슬롯별 현재 몬스터 (없으면 nullptr)
    TArray<TWeakObjectPtr<AActor>> SlotOccupants;

    FTimerHandle InitRetryTimer;
    void TryInitFromViewport();
};
