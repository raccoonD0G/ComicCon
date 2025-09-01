// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleWidget.generated.h"

/**
 * 
 */
UCLASS()
class COMICCON_API UBattleWidget : public UUserWidget
{
	GENERATED_BODY()
	
private:
    /** 현재 적의 수 표시 */
    UPROPERTY(meta = (BindWidget))
    class UTextBlock* EnemyCountText;

    /** 체력 표시 바 */
    UPROPERTY(meta = (BindWidget))
    class UProgressBar* HealthProgressBar;

    // 필요 시 함수 추가 가능
    UFUNCTION(BlueprintCallable, Category = "BattleWidget")
    void SetEnemyCount(int32 Count);

    UFUNCTION(BlueprintCallable, Category = "BattleWidget")
    void SetHealthPercent(float Percent);

};
