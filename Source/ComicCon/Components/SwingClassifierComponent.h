// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PoseClassifierComponent.h"
#include "SwingClassifierComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSwingDetected, TArray<FTimedPoseSnapshot>, Snaps);

/**
 * 
 */
UCLASS()
class COMICCON_API USwingClassifierComponent : public UPoseClassifierComponent
{
	GENERATED_BODY()

public:
    FOnSwingDetected OnSwingDetected;

private:
    // === Swing �Ǻ� �Ķ���� ===
    // �ӵ� ��� �� ������ �������� "�� �����" ����
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    float MinCloseCoverage = 0.2f; // 80% �̻� �������

    // �α� �ߺ� ���� ��ٿ�(��)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    float SwingCooldownSeconds = 0.2f;

    // ���� �ӵ� ��ȭ�� �Ķ����
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Strict", meta = (AllowPrivateAccess = "true"))
    float SwingRecentSeconds = 0.3f;          // �ֱ� �� �ð� �� ������ ���(��: 0.6~1.0s)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Strict", meta = (AllowPrivateAccess = "true"))
    float MinPathLenNorm = 1.5f;              // �̵� �����Ÿ�(����� ����) ����ġ

    

    // === Down-swing ���� ���� �Ķ���� ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float DownVyThresholdNorm = 0.40f;      // vy_norm > �� ���� �� '����� �Ʒ��� ����'���� ����(�����/��)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
    float MinDownCoverage = 0.45f;          // vy_norm > DownVyThresholdNorm �� ���� ������ ����(0~1)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float MinDownAvgSpeedNorm = 0.55f;      // ��� ��ӵ�(�����/��)�� ����

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float MinDownPeakSpeedNorm = 1.00f;     // �ִ� ��ӵ�(�����/��)�� ����

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float MinDownDeltaNorm = 0.70f;         // ���ۡ泡 ���� �麯��(����� ����) �ּ�ġ

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
    int32 MaxVertReversals = 1;             // ���� ����(��/�Ʒ�) ���� ��� Ƚ��

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float VertHoldSeconds = 0.06f;          // ���� ��ȿ ��ٿ �ð�(��). �� �ð� �̻� ���� �� '��¥ ����'���� ī��Ʈ

           // ������ ��

    // --- VFX: ���� ��ũ Ʈ���� ---
    UPROPERTY(EditAnywhere, Category = "VFX|SwingArc")
    bool bSpawnArcTrail = true;

    UPROPERTY(VisibleAnywhere, Category = "VFX|SwingArc")
    TObjectPtr<class USplineComponent> SplineComponent;

    UPROPERTY(VisibleAnywhere, Category = "VFX|SwingArc")
    TObjectPtr<class UNiagaraComponent> NiagaraComponent;

    UPROPERTY(EditAnywhere, Category = "VFX|SwingArc", meta = (ClampMin = "0.05", ClampMax = "5.0"))
    float ArcTrailLifetime = 1.0f;

    uint64 LastSwingLoggedMs = 0;

    // ���� ����(��). �� ���� �������� �� ������ ���ø�
    UPROPERTY(EditAnywhere, Category = "Swing|Sweep")
    float DegreesPerInterp = 12.f;

    virtual void Detect(EMainHand WhichHand) override; // <- Tick���� ȣ��
};
