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
    /** ���� ���� �� ǥ�� */
    UPROPERTY(meta = (BindWidget))
    class UTextBlock* EnemyCountText;

    /** ü�� ǥ�� �� */
    UPROPERTY(meta = (BindWidget))
    class UProgressBar* HealthProgressBar;

    // �ʿ� �� �Լ� �߰� ����
    UFUNCTION(BlueprintCallable, Category = "BattleWidget")
    void SetEnemyCount(int32 Count);

    UFUNCTION(BlueprintCallable, Category = "BattleWidget")
    void SetHealthPercent(float Percent);

};
