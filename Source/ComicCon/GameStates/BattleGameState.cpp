// Fill out your copyright notice in the Description page of Project Settings.


#include "GameStates/BattleGameState.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

ABattleGameState::ABattleGameState()
{
}

void ABattleGameState::BeginPlay()
{
	Super::BeginPlay();

	// 1초 간격 반복 타이머 시작
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(CountdownTimerHandle, this, &ABattleGameState::HandleCountdownTick, 1.0f, true);
	}

	OnCountdownStarted.Broadcast();
	OnCountdownSec.Broadcast(RemainingSeconds);
}

void ABattleGameState::HandleCountdownTick()
{
	if (bHasFinished)
	{
		return; // 안전장치
	}

	// 1) 남은 시간 감소
	if (RemainingSeconds > 0)
	{
		--RemainingSeconds;
	}

	// 2) 감소 직후 현재 값 브로드캐스트 (0도 전달됨)
	OnCountdownSec.Broadcast(RemainingSeconds);

	// 3) 0 이하가 되면 한번만 종료 브로드캐스트 후 레벨 전환
	if (RemainingSeconds <= 0)
	{
		bHasFinished = true;

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CountdownTimerHandle);

			// UI/사운드가 종료 이벤트에 반응할 수 있게 먼저 브로드캐스트
			OnCountdownFinished.Broadcast();

			if (NextLevelName != NAME_None)
			{
				// 멀티플레이 서버라면 ServerTravel 고려
				UGameplayStatics::OpenLevel(this, NextLevelName);
			}
		}
	}
}