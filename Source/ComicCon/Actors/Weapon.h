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
    // �÷��̾��� WeaponState�� ������ �ڵ����� ���̱�/����� ����
    UPROPERTY(EditAnywhere, Category = "Weapon|Visibility")
    bool bAutoVisibilityFromOwner = true;

    // �� ���Ⱑ "� ������ �� ������" (�Ļ� Ŭ�������� �ٲٱ�)
    UPROPERTY(EditAnywhere, Category = "Weapon|Visibility")
    EWeaponState VisibleWhenState = EWeaponState::Sword;  // �⺻�� Sword �����

    // �÷��̾� ���� ���� �̺�Ʈ ����
    UFUNCTION()
    void OnOwnerWeaponChanged(EWeaponState NewWeapon);

    // ���̱�/����� ����
    void ShowWeapon();
    void HideWeapon();
};
