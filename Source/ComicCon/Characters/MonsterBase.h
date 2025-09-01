// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MonsterBase.generated.h"

UCLASS()
class COMICCON_API AMonsterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AMonsterBase();

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	// === 데미지 처리 ===
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	void Die();
private:
	UPROPERTY(EditAnywhere, Category = "Monster")
	int32 MaxHealth = 2;

	int32 CurrentHealth = 0;

};
