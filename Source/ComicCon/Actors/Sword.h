// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/Weapon.h"
#include "Sword.generated.h"

/**
 * 
 */
UCLASS()
class COMICCON_API ASword : public AWeapon
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;

// Attack Section
public:
	FORCEINLINE USwingClassifierComponent* GetSwingClassifierComponent() const { return SwingClassifierComponent; }
    FORCEINLINE void SetOwingSwingClassifierComponent(class USwingClassifierComponent* InSwingClassifierComponent) { SwingClassifierComponent = InSwingClassifierComponent; }

private:
	TObjectPtr<class USwingClassifierComponent> SwingClassifierComponent;

    UFUNCTION()
    void HandleSwingDetected(TArray<FTimedPoseSnapshot> SnapsVal);

    // ===== Swing Plane Sweep 파라미터 =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (AllowPrivateAccess = "true"))
    float DebugSwingDrawTime = 2.0f;   // 디버그 박스 표시 시간(초)

    UPROPERTY(EditAnywhere, Category = "Attack")
    TSubclassOf<class ASwingProjectile> SwingProjectileClass;

	UPROPERTY(EditAnywhere, Category = "Attack")
    TSubclassOf<class ASwingAttack> SwingAttackClass;

    UPROPERTY(EditAnywhere, Category = "Attack")
    float TiltDegFromBlueTowardYellow = 25.f;

    UPROPERTY()
    float ProjectileSpeed = 3000.f;

    UPROPERTY()
    float ProjectileLifeSeconds = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (AllowPrivateAccess = "true"))
    float SwingDamageAmount = 2.f;

    float SpawnForwardOffset = 20.f; // 앞쪽으로 약간 띄워서 스폰

// Pos Section
private:
    UPROPERTY()
    TArray<FHandPose> CachedHands;

    UFUNCTION()
    void SetSwordPos(FVector DirWorld, FVector CenterWorld);

    UFUNCTION()
    void OnSwordPoseInput(const FVector2f& Pelvis2D, const TArray<FPersonPose>& Poses, float PixelToUU, const FTransform& OwnerXform);

    UFUNCTION()
    void OnSwordHandsInput(const FVector2f& Pelvis2D, const TArray<FHandPose>& Hands, float PixelToUU, const FTransform& OwnerXform);

    // 유틸: which=0(L) or 1(R) 손을 같은 PersonId에서 찾기
    bool FindHandCenter(uint16 PersonId, uint8 Which, const FVector2f& FallbackNearTo, FVector2f& OutCenter) const;

    // === Sword 내부에서 칼 포즈 계산 ===
    bool TryComputeSwordPoseFromPose(const FVector2f& Pelvis2D, const FPersonPose& Pose, float PixelToUU, const FTransform& OwnerXform, FVector& OutDirWorld, FVector& OutStartCenterWorld) const;

    // 픽셀→로컬 변환(프로젝트에 맞게 조정)
    FVector MakeLocal(const FVector2f& P, const FVector2f& Pelvis, float PixelToUU) const;

    // 손목에서 "앞"(팔꿈치→손목 방향)으로 내미는 비율
    UPROPERTY(EditAnywhere, Category = "Pose|Sword")
    float HandForwardRatio = 0.25f; // 전완 길이의 25%

    UPROPERTY(EditAnywhere, Category = "Pose|Units")
    float DepthOffsetX = 0.f;

    // 방향 반전(프로젝트 좌표 맞춤)
    UPROPERTY(EditAnywhere, Category = "Pose|Sword")
    bool bInvertDirectionForSword = true;

    // (참고용) 현재 계산된 결과
    UPROPERTY(BlueprintReadOnly, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    FVector SwordDirectionWorld;

    UPROPERTY(BlueprintReadOnly, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    FVector SwordStartCenterWorld;

    // 각도 간격(도). 이 값이 작을수록 더 촘촘히 샘플링
    UPROPERTY(EditAnywhere, Category = "Swing|Sweep")
    float DegreesPerInterp = 12.f;

    FVector SmoothedCenter = FVector::ZeroVector;
    FQuat   SmoothedRot = FQuat::Identity;

};
