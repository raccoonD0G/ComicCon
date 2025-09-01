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

	// 1�� ���� �ݺ� Ÿ�̸� ����
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
		return; // ������ġ
	}

	// 1) ���� �ð� ����
	if (RemainingSeconds > 0)
	{
		--RemainingSeconds;
	}

	// 2) ���� ���� ���� �� ��ε�ĳ��Ʈ (0�� ���޵�)
	OnCountdownSec.Broadcast(RemainingSeconds);

	// 3) 0 ���ϰ� �Ǹ� �ѹ��� ���� ��ε�ĳ��Ʈ �� ���� ��ȯ
	if (RemainingSeconds <= 0)
	{
		bHasFinished = true;

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CountdownTimerHandle);

			// UI/���尡 ���� �̺�Ʈ�� ������ �� �ְ� ���� ��ε�ĳ��Ʈ
			OnCountdownFinished.Broadcast();

			if (NextLevelName != NAME_None)
			{
				// ��Ƽ�÷��� ������� ServerTravel ���
				UGameplayStatics::OpenLevel(this, NextLevelName);
			}
		}
	}
}