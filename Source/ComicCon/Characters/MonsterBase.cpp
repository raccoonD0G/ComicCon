// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/MonsterBase.h"

AMonsterBase::AMonsterBase()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AMonsterBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	CurrentHealth = MaxHealth;
}

void AMonsterBase::BeginPlay()
{
	Super::BeginPlay();
}

float AMonsterBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    // 한 번 공격받을 때마다 체력 1 깎이게
    CurrentHealth = FMath::Max(0, CurrentHealth - 1);

    UE_LOG(LogTemp, Log, TEXT("%s took damage. HP: %d / %d"), *GetName(), CurrentHealth, MaxHealth);

    if (CurrentHealth <= 0)
    {
        Die();
    }

    // 부모 호출 안 해도 되지만, 다른 시스템(애니메이션 몽타주 등) 활용하려면 Super 호출 가능
    return DamageAmount;
}

void AMonsterBase::Die()
{
    UE_LOG(LogTemp, Warning, TEXT("%s died!"), *GetName());

    // 간단히 액터 제거
    Destroy();

    // (선택) 여기서 죽음 애니메이션, 이펙트, 점수 처리 등 추가 가능
}