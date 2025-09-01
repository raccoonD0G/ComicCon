// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/BattleWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

void UBattleWidget::SetEnemyCount(int32 Count)
{
    if (EnemyCountText)
    {
        EnemyCountText->SetText(FText::AsNumber(Count));
    }
}

void UBattleWidget::SetHealthPercent(float Percent)
{
    if (HealthProgressBar)
    {
        HealthProgressBar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
    }
}