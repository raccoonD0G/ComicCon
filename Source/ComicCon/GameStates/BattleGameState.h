// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BattleGameState.generated.h"

// 카운트다운 시작 알림 (한 번)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCountdownStarted);
// 남은 시간 알려주기 (매 틱)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCountdownSec, int32, RemainingSeconds);
// 카운트다운 끝 알림 (한 번)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCountdownFinished);

/**
 * 
 */
UCLASS()
class COMICCON_API ABattleGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ABattleGameState();

protected:
	virtual void BeginPlay() override;

public:
	/** 카운트다운 시작 시 1회 브로드캐스트 */
	UPROPERTY(BlueprintAssignable, Category = "Timer")
	FOnCountdownStarted OnCountdownStarted;

	/** 1초마다 남은 시간 브로드캐스트 */
	UPROPERTY(BlueprintAssignable, Category = "Timer")
	FOnCountdownSec OnCountdownSec;

	/** 카운트다운 종료 시 1회 브로드캐스트 */
	UPROPERTY(BlueprintAssignable, Category = "Timer")
	FOnCountdownFinished OnCountdownFinished;

protected:

	/** 남은 시간(초). UI에서 바인딩해서 쓰기 좋게 공개 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Timer")
	int32 RemainingSeconds = 60;

	/** 타깃 레벨 이름 (끝났을 때 전환) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Timer")
	FName NextLevelName = FName("ResultMap");

	/** 1초마다 호출되는 콜백 */
	UFUNCTION()
	void HandleCountdownTick();

private:
	FTimerHandle CountdownTimerHandle;
	uint8 bHasFinished : 1 = false;
};