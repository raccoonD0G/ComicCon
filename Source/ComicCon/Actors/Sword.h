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

protected:
    UFUNCTION()
    void SetSwordPos(FVector DirWorld, FVector CenterWorld);

    // Receiver Section
    // === 리시버의 "원시 포즈 입력"을 받는 콜백 ===
    UFUNCTION()
    void OnSwordPoseInput(const FVector2f& Pelvis2D, const FPersonPose& Pose, const FTransform& OwnerXform);

    // === Sword 내부에서 칼 포즈 계산 ===
    bool TryComputeSwordPoseFromPose(const FVector2f& Pelvis2D, const FPersonPose& Pose, const FTransform& OwnerXform, FVector& OutDirWorld, FVector& OutStartCenterWorld) const;

    // 픽셀→로컬 변환(프로젝트에 맞게 조정)
    FVector MakeLocal(const FVector2f& P, const FVector2f& Pelvis) const;

    // === 튜닝/디버그 파라미터 (이관됨) ===
    UPROPERTY(EditAnywhere, Category = "Pose|Sword")
    bool bDrawSwordDebug = true;

    // 손이 가까운 조건: HandDist < HandCloseRatio * ShoulderLen
    UPROPERTY(EditAnywhere, Category = "Pose|Sword")
    float HandCloseRatio = 1.0f;

    // 손 박스 여유/최소 크기 (UU)
    UPROPERTY(EditAnywhere, Category = "Pose|Sword")
    float BoxPadUU = 4.f;

    UPROPERTY(EditAnywhere, Category = "Pose|Sword")
    float BoxMinSizeUU = 8.f;

    // 손목에서 "앞"(팔꿈치→손목 방향)으로 내미는 비율
    UPROPERTY(EditAnywhere, Category = "Pose|Sword")
    float HandForwardRatio = 0.25f; // 전완 길이의 25%

    // 픽셀→UU 스케일 / 깊이 오프셋 / 이미지 Y 뒤집기(필요시)
    UPROPERTY(EditAnywhere, Category = "Pose|Units")
    float PixelToUU = 1.f;

    UPROPERTY(EditAnywhere, Category = "Pose|Units")
    float DepthOffsetX = 0.f;

    UPROPERTY(EditAnywhere, Category = "Pose|Units")
    bool bInvertImageYToUp = true; // 이미지 Y가 아래로 증가하면 true

    // 방향 반전(프로젝트 좌표 맞춤)
    UPROPERTY(EditAnywhere, Category = "Pose|Sword")
    bool bInvertDirectionForSword = true;

    // (참고용) 현재 계산된 결과
    UPROPERTY(BlueprintReadOnly, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    FVector SwordDirectionWorld;

    UPROPERTY(BlueprintReadOnly, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    FVector SwordStartCenterWorld;


};
