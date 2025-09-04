// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/Weapon.h"
#include "Amulet.generated.h"

/**
 * 
 */
UCLASS()
class COMICCON_API AAmulet : public AWeapon
{
	GENERATED_BODY()

public:
	AAmulet();
    void SetOwingSingleSwingClassifierComponent(class USingleSwingClassifierComponent* InSingleSwingClassifierComponent);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class USingleSwingClassifierComponent> SingleSwingClassifierComponent;

// Follow Pose Section
private:
	// ---- 바인딩 ----
    UFUNCTION()
    void OnPoseInput(const FVector2f& Pelvis2D, const TArray<FPersonPose>& Poses, float PixelToUU, const FTransform& OwnerXform);

    // ---- 포즈 처리 ----
    void SetAmuletPose(const FVector& CenterWorld);

    // 한쪽 손의 연장 지점 계산: 성공하면 OutPoint 반환
    bool TryComputeExtendedPoint(const FVector2f& Pelvis2D, const FPersonPose& Pose, const FTransform& OwnerXform, bool bRightHand, FVector& OutPoint, float* OutScore = nullptr) const;

    // 튜닝: 전완 길이의 몇 %를 손목 바깥으로
    UPROPERTY(EditAnywhere, Category = "Amulet|Pose")
    float ExtendRatio = 0.25f; // 25%

    // (선택) 디버그
    UPROPERTY(EditAnywhere, Category = "Amulet|Debug")
    bool bDrawDebug = false;

// Attack Section
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<class AAmuletAttack> AmuletAttackClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|PlaneSweep", meta = (AllowPrivateAccess = "true"))
    float DebugSingleSwingDrawTime = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Damage", meta = (AllowPrivateAccess = "true"))
    float SingleSwingDamageAmount = 2.f;

    UPROPERTY()
    float AttackLifeSeconds = 1.f;

    UFUNCTION()
    void HandleSingleSwingDetected(TArray<FTimedPoseSnapshot> Snaps, TArray<int32> PersonIdxOf, bool bRightHand, int32 EnterIdx, int32 ExitIdx);

};
