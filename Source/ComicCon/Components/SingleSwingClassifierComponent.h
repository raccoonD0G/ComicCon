// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PoseClassifierComponent.h"
#include "SingleSwingClassifierComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnSingleSwingDetected, TArray<FTimedPoseSnapshot>, Snaps, TArray<int32>, PersonIdxOf, bool, bRightHand, int32, EnterIdx, int32, ExitIdx);

/**
 * 
 */
UCLASS()
class COMICCON_API USingleSwingClassifierComponent : public UPoseClassifierComponent
{
	GENERATED_BODY()

public:
	FOnSingleSwingDetected OnSingleSwingDetected;

protected:
    virtual void Detect(EMainHand WhichHand) override;

// Single Swing Section
private:
    uint64 LastSingleSwingLoggedMs = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    float SingleSwingCooldownSeconds = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Strict", meta = (AllowPrivateAccess = "true"))
    float SingleSwingRecentSeconds = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double MinOpenCoverage = 0.55;

    // �ӵ� �Ӱ�(�����/��).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double MinPeakSpeedNorm = 1.50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double MinAvgSpeedNorm = 0.80;

    // ���� ��� ��/��ٿ/������. �� 2 �� 3 (���� �� ����),
    // AxisHoldSeconds 0.06 �� 0.05, SignDeadband 0.15 �� 0.12
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    int32 MaxRadialReversals = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double AxisHoldSeconds = 0.05;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double SignDeadband = 0.12;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double MinDisplacementNorm = 0.6; // ����� ��� �ּ� �̵� �Ÿ� (ex. 0.6~1.0 ��õ)

    // Swing Detection Thresholds
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double EnterSpeed = 7.0f;   // ���� ���� �ӵ� �Ӱ� (����ȭ)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double ExitSpeed = 0.1;    // ���� ���� �ӵ� �Ӱ� (����ȭ)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double HoldFast = 0.10;     // ���� �ӵ��� �����ž� �ϴ� �ּ� �ð� (��)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double HoldStill = 0.06;    // ���� �ӵ��� �����ž� �ϴ� �ּ� �ð� (��)

    
};
