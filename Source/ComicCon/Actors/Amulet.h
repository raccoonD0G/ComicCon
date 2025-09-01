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

protected:
	virtual void BeginPlay() override;

private:
	// ---- 바인딩 ----
    UFUNCTION()
    void OnPoseInput(const FVector2f& Pelvis2D, const FPersonPose& Pose, const FTransform& OwnerXform);

    // ---- 포즈 처리 ----
    void SetAmuletPose(const FVector& CenterWorld);
    FVector MakeLocal(const FVector2f& P, const FVector2f& Pelvis) const;

    // 한쪽 손의 연장 지점 계산: 성공하면 OutPoint 반환
    bool TryComputeExtendedPoint(const FVector2f& Pelvis2D, const FPersonPose& Pose, const FTransform& OwnerXform, bool bRightHand, FVector& OutPoint, float* OutScore = nullptr) const;

    // 튜닝: 전완 길이의 몇 %를 손목 바깥으로
    UPROPERTY(EditAnywhere, Category = "Amulet|Pose")
    float ExtendRatio = 0.25f; // 25%

    // 픽셀 -> UU 변환 및 이미지 좌표 기준
    UPROPERTY(EditAnywhere, Category = "Amulet|Pose")
    float PixelToUU = 1.f;

    UPROPERTY(EditAnywhere, Category = "Amulet|Pose")
    float DepthOffsetX = 0.f;

    UPROPERTY(EditAnywhere, Category = "Amulet|Pose")
    bool bInvertImageYToUp = true; // 이미지 Y가 아래로 증가하면 true

    // (선택) 디버그
    UPROPERTY(EditAnywhere, Category = "Amulet|Debug")
    bool bDrawDebug = false;
};
