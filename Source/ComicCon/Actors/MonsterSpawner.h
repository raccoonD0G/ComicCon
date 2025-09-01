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
    void SpawnAtViewportEdges(); // �ʱ� 8�� ���

    // === �ű�: �� ������ �����ϴ� �Լ�(Ÿ�̸ӿ��� ȣ��) ===
    void SpawnOne();
    void ScheduleNextSpawn();

    // === ���� �ı��� �ݹ� ===
	UFUNCTION()
    void HandleMonsterDestroyed(AActor* DestroyedActor);

    // === ��ƿ ===
    void BuildScreenPoints();      // 8�� ��ũ�� ����Ʈ ����

protected:
    UPROPERTY(EditAnywhere, Category = "Spawn")
    TSubclassOf<AActor> ActorToSpawn;

    UPROPERTY(EditAnywhere, Category = "Spawn")
    float SpawnDistance = 500.f;

    // ----- ���� ���� Ʃ�� �� -----
    UPROPERTY(EditAnywhere, Category = "AdaptiveSpawn")
    float MinSpawnInterval = 0.3f;     // �ʹ� ���� ������ ����

    UPROPERTY(EditAnywhere, Category = "AdaptiveSpawn")
    float MaxSpawnInterval = 5.0f;     // ���� ������ ����

    // ������ ���� / ���� �������� ����(��). EMA�� �̺��� ª���� �� ���� ����.
    UPROPERTY(EditAnywhere, Category = "AdaptiveSpawn")
    float ShortLifeSeconds = 1.0f;     // �ſ� ���� �״´� �Ǵ� ����
    UPROPERTY(EditAnywhere, Category = "AdaptiveSpawn")
    float LongLifeSeconds = 10.0f;    // ���� ��� �Ǵ� ����

    // EMA ����ġ (0~1). �������� �ֽŰ� �ݿ� ũ��.
    UPROPERTY(EditAnywhere, Category = "AdaptiveSpawn")
    float EMAlpha = 0.35f;

private:
    // ���� ����Ʈ 8���� (�¿� 3 + ��/�� �߾�)
    TArray<FIntPoint> ScreenPoints;

    // ī�޶� �߾� Ÿ�� (����Ʈ �߾ӿ��� �� 500)
    FVector CachedCenterTarget = FVector::ZeroVector;
    int32 CachedViewX = 0, CachedViewY = 0;

    // Ÿ�̸�
    FTimerHandle SpawnTimer;

    // ���� �����ð� ���
    double LifeEMA = 5.0; // �ʱⰪ(�߰� ����)

    // ���� �ð� ���
    TMap<TWeakObjectPtr<AActor>, double> SpawnTimeByActor;

    // �� ���Ժ� ���� ���� (������ nullptr)
    TArray<TWeakObjectPtr<AActor>> SlotOccupants;

    FTimerHandle InitRetryTimer;
    void TryInitFromViewport();
};
