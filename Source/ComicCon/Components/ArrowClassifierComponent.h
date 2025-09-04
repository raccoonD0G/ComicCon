// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PoseClassifierComponent.h"
#include "ArrowClassifierComponent.generated.h"

/**
 * 
 */
UCLASS()
class COMICCON_API UArrowClassifierComponent : public UPoseClassifierComponent
{
	GENERATED_BODY()
	
    // Arrow Section
private:
    // === Awrrow �Ǻ� �Ķ���� ===
    // "����� �־���" ���� ����: �ո� �� �Ÿ� �� ArrowFarRatio * �����
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow", meta = (AllowPrivateAccess = "true"))
    float ArrowFarRatio = 1.8f;            // ������� 1.2�� �̻� �������� ȭ�� �߻�� ����

    // ������ ������ ������־��� ������(����ȭ) �ּ�ġ (�����׸��ý�)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow", meta = (AllowPrivateAccess = "true"))
    float ArrowMinDeltaNorm = 0.5f;        // (�־��� - �����) / �����

    // �־����� �ӵ�(����ȭ: �����/��) �ּ�ġ (�ʹ� ���� ��ȭ ����)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow", meta = (AllowPrivateAccess = "true"))
    float ArrowMinSpeedNorm = 1.0f;

    // �α� �� ��ٿ�(��)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow", meta = (AllowPrivateAccess = "true"))
    float ArrowCooldownSeconds = 1.0f;

    // ===== Arrow ���(Plane) ������ �Ķ���� =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Plane", meta = (AllowPrivateAccess = "true"))
    float PlaneDistance = 150.f;        // ������(Start)���� ����(Dir)�� �󸶳� ������ ���� ����� ����

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Plane", meta = (AllowPrivateAccess = "true"))
    float ArrowPlaneHalfThickness = 20.f;     // �ü� ����(X) �β� (�������� '��'�� �����)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Damage", meta = (AllowPrivateAccess = "true"))
    float ArrowDamageAmount = 2.f;        // ������ ��

private:
    uint64 LastArrowLoggedMs = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    FVector2D PlaneHalfSize = FVector2D(5000.f, 1000.f); // ��� ���� ũ��(Y=����, Z=����)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    float PlaneHalfThickness = 20.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    uint8 bDrawDebugPlane : 1 = false;

    UPROPERTY()
    float ProjectileSpeed = 3000.f;

    UPROPERTY()
    float ProjectileLifeSeconds = 2.f;

    UPROPERTY()
    bool bSpawnAtEachPlane = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|PlaneSweep", meta = (AllowPrivateAccess = "true"))
    float DebugSingleSwingDrawTime = 2.0f;

    // ���� ��� �ּ�/�ִ� ����(��������Ʈ ����)
    UPROPERTY(EditAnywhere, Category = "Swing")
    int32 MinInterpPlanes = 2;

    UPROPERTY(EditAnywhere, Category = "Swing")
    int32 MaxInterpPlanes = 24;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Damage", meta = (AllowPrivateAccess = "true"))
    float SingleSwingDamageAmount = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UDamageType> DamageTypeClass;

    virtual void Detect(EMainHand WhichHand) override; // �� Tick���� ȣ��

    void ApplyArrowDamage(const FVector& ShoulderW, const FVector& WristW);
};
