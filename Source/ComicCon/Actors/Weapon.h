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
	void SetOwningPlayer(AMirroredPlayer* InMirroredPlayer);

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

// Attack Plane Section
protected:
	void OverlapPlaneOnce(const FVector& Center, const FQuat& Rot, TSet<AActor*>& UniqueActors, float DebugLifeTime);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    FVector2D PlaneHalfSize = FVector2D(5000.f, 1000.f); // ��� ���� ũ��(Y=����, Z=����)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    float PlaneHalfThickness = 20.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    uint8 bDrawDebugPlane : 1 = true;

    // ���� ��� �ּ�/�ִ� ����(��������Ʈ ����)
    UPROPERTY(EditAnywhere, Category = "Swing")
    int32 MinInterpPlanes = 2;

    UPROPERTY(EditAnywhere, Category = "Swing")
    int32 MaxInterpPlanes = 24;

    UPROPERTY()
    bool bSpawnAtEachPlane = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    TEnumAsByte<ECollisionChannel> PlaneChannel = ECC_Pawn; // ��ħ ���� ä��

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UDamageType> DamageTypeClass;

};
