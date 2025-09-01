// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BattleGameState.generated.h"

// ī��Ʈ�ٿ� ���� �˸� (�� ��)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCountdownStarted);
// ���� �ð� �˷��ֱ� (�� ƽ)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCountdownSec, int32, RemainingSeconds);
// ī��Ʈ�ٿ� �� �˸� (�� ��)
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
	/** ī��Ʈ�ٿ� ���� �� 1ȸ ��ε�ĳ��Ʈ */
	UPROPERTY(BlueprintAssignable, Category = "Timer")
	FOnCountdownStarted OnCountdownStarted;

	/** 1�ʸ��� ���� �ð� ��ε�ĳ��Ʈ */
	UPROPERTY(BlueprintAssignable, Category = "Timer")
	FOnCountdownSec OnCountdownSec;

	/** ī��Ʈ�ٿ� ���� �� 1ȸ ��ε�ĳ��Ʈ */
	UPROPERTY(BlueprintAssignable, Category = "Timer")
	FOnCountdownFinished OnCountdownFinished;

protected:

	/** ���� �ð�(��). UI���� ���ε��ؼ� ���� ���� ���� */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Timer")
	int32 RemainingSeconds = 60;

	/** Ÿ�� ���� �̸� (������ �� ��ȯ) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Timer")
	FName NextLevelName = FName("ResultMap");

	/** 1�ʸ��� ȣ��Ǵ� �ݹ� */
	UFUNCTION()
	void HandleCountdownTick();

private:
	FTimerHandle CountdownTimerHandle;
	uint8 bHasFinished : 1 = false;
};