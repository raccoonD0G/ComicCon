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
    // �� �� ���ݹ��� ������ ü�� 1 ���̰�
    CurrentHealth = FMath::Max(0, CurrentHealth - 1);

    UE_LOG(LogTemp, Log, TEXT("%s took damage. HP: %d / %d"), *GetName(), CurrentHealth, MaxHealth);

    if (CurrentHealth <= 0)
    {
        Die();
    }

    // �θ� ȣ�� �� �ص� ������, �ٸ� �ý���(�ִϸ��̼� ��Ÿ�� ��) Ȱ���Ϸ��� Super ȣ�� ����
    return DamageAmount;
}

void AMonsterBase::Die()
{
    UE_LOG(LogTemp, Warning, TEXT("%s died!"), *GetName());

    // ������ ���� ����
    Destroy();

    // (����) ���⼭ ���� �ִϸ��̼�, ����Ʈ, ���� ó�� �� �߰� ����
}