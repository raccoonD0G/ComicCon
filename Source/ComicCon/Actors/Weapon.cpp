// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/Weapon.h"
#include "Actors/MirroredPlayer.h"

AWeapon::AWeapon()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"));
	RootComponent = SceneComponent;

	WeaponMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	WeaponMeshComponent->SetupAttachment(RootComponent);
}

void AWeapon::BeginPlay()
{
	Super::BeginPlay();

	check(OwningPlayer);

    if (bAutoVisibilityFromOwner)
    {
        // �̺�Ʈ ���ε�(�ߺ� ����)
        OwningPlayer->OnCurrentWeaponChanged.RemoveDynamic(this, &AWeapon::OnOwnerWeaponChanged);
        OwningPlayer->OnCurrentWeaponChanged.AddDynamic(this, &AWeapon::OnOwnerWeaponChanged);

        // ���� ���� ��� �ݿ�
        OnOwnerWeaponChanged(OwningPlayer->GetCurrentWeapon());
    }
    else
    {
        // ���� �÷��̾ ������ �⺻������ ����
        HideWeapon();
    }
}

void AWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (OwningPlayer)
    {
        OwningPlayer->OnCurrentWeaponChanged.RemoveDynamic(this, &AWeapon::OnOwnerWeaponChanged);
    }
    Super::EndPlay(EndPlayReason);
}

void AWeapon::OnOwnerWeaponChanged(EWeaponState NewWeapon)
{
    if (!bAutoVisibilityFromOwner) return;

    const bool bShouldShow = (NewWeapon == VisibleWhenState);
    if (bShouldShow)
    {
        ShowWeapon();
    }
    else
    {
        HideWeapon();
    }
}

void AWeapon::ShowWeapon()
{
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
}

void AWeapon::HideWeapon()
{
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
}
