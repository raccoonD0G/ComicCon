// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Actors/MirroredPlayer.h"
#include "Weapon.generated.h"

UCLASS()
class COMICCON_API AWeapon : public AActor
{
	GENERATED_BODY()
	
public:
	AWeapon();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class AMirroredPlayer> OwningPlayer;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneComponent;

// Mesh Section
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> WeaponMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMesh> WeaponMesh;

// Visibility Section
protected:
    // 플레이어의 WeaponState와 연동해 자동으로 보이기/숨기기 할지
    UPROPERTY(EditAnywhere, Category = "Weapon|Visibility")
    bool bAutoVisibilityFromOwner = true;

    // 이 무기가 "어떤 상태일 때 보일지" (파생 클래스에서 바꾸기)
    UPROPERTY(EditAnywhere, Category = "Weapon|Visibility")
    EWeaponState VisibleWhenState = EWeaponState::Sword;  // 기본은 Sword 무기용

    // 플레이어 상태 변경 이벤트 수신
    UFUNCTION()
    void OnOwnerWeaponChanged(EWeaponState NewWeapon);

    // 보이기/숨기기 헬퍼
    void ShowWeapon();
    void HideWeapon();
};
