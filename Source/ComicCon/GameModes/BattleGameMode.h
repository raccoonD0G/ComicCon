// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BattleGameMode.generated.h"

/**
 * 
 */
UCLASS()
class COMICCON_API ABattleGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;

private:
	FTimerHandle InitRetryTimer;

	UFUNCTION()
	void Init();

	UFUNCTION()
	void StartMatch();

	UFUNCTION()
	void EndMatch();
};
