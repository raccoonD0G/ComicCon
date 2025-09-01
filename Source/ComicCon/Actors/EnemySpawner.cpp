// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/EnemySpawner.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/Actor.h"
#include "Characters/EnemyBase.h"
#include "Utils.h"

AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	if (bUseDeterministicSeed)
	{
		Rng.Initialize(RandomSeed);
	}
	else
	{
		Rng.GenerateNewSeed();
	}

	if (bAutoStart)
	{
		Start();
	}
}

void AEnemySpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AEnemySpawner::Start()
{
	if (bRunning) return;
	bRunning = true;

	if (InitialDelay < KINDA_SMALL_NUMBER)
	{
		ScheduleNextSpawn();
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(SpawnLoopTimerHandle, this, &AEnemySpawner::ScheduleNextSpawn, InitialDelay, false);
	}
	
}

void AEnemySpawner::Stop()
{
	if (!bRunning) return;
	bRunning = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnLoopTimerHandle);
	}
}

void AEnemySpawner::ScheduleNextSpawn()
{
	if (!bRunning) return;

	if (UWorld* World = GetWorld())
	{
		float NextDelay = 0.0f;
		const float J = SpawnIntervalJitter;
		const float Noise = (J > 0.f) ? Rng.FRandRange(-J, +J) : 0.f;
		NextDelay = FMath::Max(0.05f, SpawnInterval + Noise);

		World->GetTimerManager().SetTimer(SpawnLoopTimerHandle, this, &AEnemySpawner::SpawnOnce, NextDelay, false);
	}
}

void AEnemySpawner::SpawnOnce()
{
	UE_LOG(LogTemp, Warning, TEXT("[Spawner] SpawnOnce ENTER (Running=%d, EnemyClass=%s)"),
		bRunning, *GetNameSafe(EnemyClass));

	if (!bRunning)
	{
		return;
	}

	if (!EnemyClass)
	{
		ScheduleNextSpawn();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		ScheduleNextSpawn();
		return;
	}

	const FTransform SpawnXform = GetSpawnTransform();

	// === 지연 스폰 ===
	AEnemyBase* NewEnemy = World->SpawnActorDeferred<AEnemyBase>(EnemyClass, SpawnXform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	if (NewEnemy)
	{
		ConfigureSpawnedEnemy(NewEnemy);
		NewEnemy->FinishSpawning(SpawnXform);
	}
	// 다음 스폰 예약
	ScheduleNextSpawn();
}

FTransform AEnemySpawner::GetSpawnTransform_Implementation()
{
	// 기본값: 자기 자신의 트랜스폼에서 스폰
	return GetActorTransform();
}

void AEnemySpawner::ConfigureSpawnedEnemy_Implementation(AEnemyBase* NewEnemy)
{
}
