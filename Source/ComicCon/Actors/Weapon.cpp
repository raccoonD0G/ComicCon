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
        // 이벤트 바인딩(중복 방지)
        OwningPlayer->OnCurrentWeaponChanged.RemoveDynamic(this, &AWeapon::OnOwnerWeaponChanged);
        OwningPlayer->OnCurrentWeaponChanged.AddDynamic(this, &AWeapon::OnOwnerWeaponChanged);

        // 시작 상태 즉시 반영
        OnOwnerWeaponChanged(OwningPlayer->GetCurrentWeapon());
    }
    else
    {
        // 소유 플레이어가 없으면 기본적으로 숨김
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
