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

// Attack Plane Section
protected:
	void OverlapPlaneOnce(const FVector& Center, const FQuat& Rot, TSet<AActor*>& UniqueActors, float DebugLifeTime);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    FVector2D PlaneHalfSize = FVector2D(5000.f, 1000.f); // 평면 절반 크기(Y=가로, Z=세로)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    float PlaneHalfThickness = 20.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    uint8 bDrawDebugPlane : 1 = true;

    // 보간 평면 최소/최대 개수(엔드포인트 포함)
    UPROPERTY(EditAnywhere, Category = "Swing")
    int32 MinInterpPlanes = 2;

    UPROPERTY(EditAnywhere, Category = "Swing")
    int32 MaxInterpPlanes = 24;

    UPROPERTY()
    bool bSpawnAtEachPlane = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    TEnumAsByte<ECollisionChannel> PlaneChannel = ECC_Pawn; // 겹침 판정 채널

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UDamageType> DamageTypeClass;

};
