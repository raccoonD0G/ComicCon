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
	// ---- ���ε� ----
    UFUNCTION()
    void OnPoseInput(const FVector2f& Pelvis2D, const FPersonPose& Pose, const FTransform& OwnerXform);

    // ---- ���� ó�� ----
    void SetAmuletPose(const FVector& CenterWorld);
    FVector MakeLocal(const FVector2f& P, const FVector2f& Pelvis) const;

    // ���� ���� ���� ���� ���: �����ϸ� OutPoint ��ȯ
    bool TryComputeExtendedPoint(const FVector2f& Pelvis2D, const FPersonPose& Pose, const FTransform& OwnerXform, bool bRightHand, FVector& OutPoint, float* OutScore = nullptr) const;

    // Ʃ��: ���� ������ �� %�� �ո� �ٱ�����
    UPROPERTY(EditAnywhere, Category = "Amulet|Pose")
    float ExtendRatio = 0.25f; // 25%

    // �ȼ� -> UU ��ȯ �� �̹��� ��ǥ ����
    UPROPERTY(EditAnywhere, Category = "Amulet|Pose")
    float PixelToUU = 1.f;

    UPROPERTY(EditAnywhere, Category = "Amulet|Pose")
    float DepthOffsetX = 0.f;

    UPROPERTY(EditAnywhere, Category = "Amulet|Pose")
    bool bInvertImageYToUp = true; // �̹��� Y�� �Ʒ��� �����ϸ� true

    // (����) �����
    UPROPERTY(EditAnywhere, Category = "Amulet|Debug")
    bool bDrawDebug = false;
};
