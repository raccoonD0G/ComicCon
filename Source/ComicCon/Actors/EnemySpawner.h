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

	/** ���� ��� Enemy Ŭ����(�ڽĿ��� AFlyingEnemy ������ ��ü) */
	UPROPERTY(EditAnywhere, Category = "Spawner")
	TSubclassOf<class AEnemyBase> EnemyClass;

	/** �⺻ ���� �ֱ�(��) */
	UPROPERTY(EditAnywhere, Category = "Spawner", meta = (ClampMin = "0.05"))
	float SpawnInterval = 3.0f;

	/** ���� �ֱ� ������(��). ���� �ֱ�� [SpawnInterval - Jitter, SpawnInterval + Jitter] ���� */
	UPROPERTY(EditAnywhere, Category = "Spawner", meta = (ClampMin = "0.0"))
	float SpawnIntervalJitter = 0.5f;

	/** ���� ����(��) */
	UPROPERTY(EditAnywhere, Category = "Spawner", meta = (ClampMin = "0.0"))
	float InitialDelay = 0.1f;

	/** ���� �� �ڵ� ���� */
	UPROPERTY(EditAnywhere, Category = "Spawner")
	uint8 bAutoStart :1 = true;

	/** (�ɼ�) ���� �õ� ��� �� ���� ������ ���� */
	UPROPERTY(EditAnywhere, Category = "Spawner")
	uint8 bUseDeterministicSeed : 1 = false;

	UPROPERTY(EditAnywhere, Category = "Spawner", meta = (EditCondition = "bUseDeterministicSeed"))
	int32 RandomSeed = 12345;

	/** ���� ����/���� */
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void Start();

	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void Stop();

	UFUNCTION(BlueprintPure, Category = "Spawner")
	bool IsRunning() const { return bRunning; }

protected:
	/** �� �� ����(Deferred) */
	UFUNCTION()
	virtual void SpawnOnce();

	/** ���� ���� ���� */
	virtual void ScheduleNextSpawn();

	/** �ڽĿ��� ���� ��ġ/ȸ�� ���� (�⺻: �ڽ��� Transform) */
	UFUNCTION(BlueprintNativeEvent, Category = "Spawner")
	FTransform GetSpawnTransform();
	virtual FTransform GetSpawnTransform_Implementation();

	/** �ڽĿ��� ������ �� �ʱ� ����(��: Init ȣ�� ��) */
	UFUNCTION(BlueprintNativeEvent, Category = "Spawner")
	void ConfigureSpawnedEnemy(class AEnemyBase* NewEnemy);
	virtual void ConfigureSpawnedEnemy_Implementation(class AEnemyBase* NewEnemy);

protected:
	UPROPERTY()
	uint8 bRunning : 1 = false;

	FTimerHandle SpawnLoopTimerHandle;

	/** ���� ��Ʈ��(������ ��꿡 ���) */
	FRandomStream Rng;

};
