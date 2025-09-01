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
    // === ���ù��� "���� ���� �Է�"�� �޴� �ݹ� ===
    UFUNCTION()
    void OnSwordPoseInput(const FVector2f& Pelvis2D, const FPersonPose& Pose, const FTransform& OwnerXform);

    // === Sword ���ο��� Į ���� ��� ===
    bool TryComputeSwordPoseFromPose(const FVector2f& Pelvis2D, const FPersonPose& Pose, const FTransform& OwnerXform, FVector& OutDirWorld, FVector& OutStartCenterWorld) const;

    // �ȼ������ ��ȯ(������Ʈ�� �°� ����)
    FVector MakeLocal(const FVector2f& P, const FVector2f& Pelvis) const;

    // === Ʃ��/����� �Ķ���� (�̰���) ===
    UPROPERTY(EditAnywhere, Category = "Pose|Sword")
    bool bDrawSwordDebug = true;

    // ���� ����� ����: HandDist < HandCloseRatio * ShoulderLen
    UPROPERTY(EditAnywhere, Category = "Pose|Sword")
    float HandCloseRatio = 1.0f;

    // �� �ڽ� ����/�ּ� ũ�� (UU)
    UPROPERTY(EditAnywhere, Category = "Pose|Sword")
    float BoxPadUU = 4.f;

    UPROPERTY(EditAnywhere, Category = "Pose|Sword")
    float BoxMinSizeUU = 8.f;

    // �ո񿡼� "��"(�Ȳ�ġ��ո� ����)���� ���̴� ����
    UPROPERTY(EditAnywhere, Category = "Pose|Sword")
    float HandForwardRatio = 0.25f; // ���� ������ 25%

    // �ȼ���UU ������ / ���� ������ / �̹��� Y ������(�ʿ��)
    UPROPERTY(EditAnywhere, Category = "Pose|Units")
    float PixelToUU = 1.f;

    UPROPERTY(EditAnywhere, Category = "Pose|Units")
    float DepthOffsetX = 0.f;

    UPROPERTY(EditAnywhere, Category = "Pose|Units")
    bool bInvertImageYToUp = true; // �̹��� Y�� �Ʒ��� �����ϸ� true

    // ���� ����(������Ʈ ��ǥ ����)
    UPROPERTY(EditAnywhere, Category = "Pose|Sword")
    bool bInvertDirectionForSword = true;

    // (�����) ���� ���� ���
    UPROPERTY(BlueprintReadOnly, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    FVector SwordDirectionWorld;

    UPROPERTY(BlueprintReadOnly, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    FVector SwordStartCenterWorld;


};
