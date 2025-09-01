// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"


UCLASS()
class COMICCON_API AEnemySpawner : public AActor
{
	GENERATED_BODY()
	
public:
	AEnemySpawner();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	/** 스폰 대상 Enemy 클래스(자식에서 AFlyingEnemy 등으로 교체) */
	UPROPERTY(EditAnywhere, Category = "Spawner")
	TSubclassOf<class AEnemyBase> EnemyClass;

	/** 기본 스폰 주기(초) */
	UPROPERTY(EditAnywhere, Category = "Spawner", meta = (ClampMin = "0.05"))
	float SpawnInterval = 3.0f;

	/** 스폰 주기 노이즈(초). 실제 주기는 [SpawnInterval - Jitter, SpawnInterval + Jitter] 랜덤 */
	UPROPERTY(EditAnywhere, Category = "Spawner", meta = (ClampMin = "0.0"))
	float SpawnIntervalJitter = 0.5f;

	/** 시작 지연(초) */
	UPROPERTY(EditAnywhere, Category = "Spawner", meta = (ClampMin = "0.0"))
	float InitialDelay = 0.1f;

	/** 시작 시 자동 실행 */
	UPROPERTY(EditAnywhere, Category = "Spawner")
	uint8 bAutoStart :1 = true;

	/** (옵션) 고정 시드 사용 시 재현 가능한 랜덤 */
	UPROPERTY(EditAnywhere, Category = "Spawner")
	uint8 bUseDeterministicSeed : 1 = false;

	UPROPERTY(EditAnywhere, Category = "Spawner", meta = (EditCondition = "bUseDeterministicSeed"))
	int32 RandomSeed = 12345;

	/** 스폰 시작/중지 */
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void Start();

	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void Stop();

	UFUNCTION(BlueprintPure, Category = "Spawner")
	bool IsRunning() const { return bRunning; }

protected:
	/** 한 번 스폰(Deferred) */
	UFUNCTION()
	virtual void SpawnOnce();

	/** 다음 스폰 예약 */
	virtual void ScheduleNextSpawn();

	/** 자식에서 스폰 위치/회전 정의 (기본: 자신의 Transform) */
	UFUNCTION(BlueprintNativeEvent, Category = "Spawner")
	FTransform GetSpawnTransform();
	virtual FTransform GetSpawnTransform_Implementation();

	/** 자식에서 스폰된 적 초기 설정(예: Init 호출 등) */
	UFUNCTION(BlueprintNativeEvent, Category = "Spawner")
	void ConfigureSpawnedEnemy(class AEnemyBase* NewEnemy);
	virtual void ConfigureSpawnedEnemy_Implementation(class AEnemyBase* NewEnemy);

protected:
	UPROPERTY()
	uint8 bRunning : 1 = false;

	FTimerHandle SpawnLoopTimerHandle;

	/** 랜덤 스트림(노이즈 계산에 사용) */
	FRandomStream Rng;

};
