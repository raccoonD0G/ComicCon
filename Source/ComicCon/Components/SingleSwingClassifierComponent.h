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

    // 속도 임계(어깨폭/초).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double MinPeakSpeedNorm = 1.50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double MinAvgSpeedNorm = 0.80;

    // 반전 허용 수/디바운스/데드밴드. ↑ 2 → 3 (조금 더 관대),
    // AxisHoldSeconds 0.06 → 0.05, SignDeadband 0.15 → 0.12
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    int32 MaxRadialReversals = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double AxisHoldSeconds = 0.05;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double SignDeadband = 0.12;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double MinDisplacementNorm = 0.6; // 어깨폭 대비 최소 이동 거리 (ex. 0.6~1.0 추천)

    // Swing Detection Thresholds
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double EnterSpeed = 7.0f;   // 스윙 시작 속도 임계 (정규화)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double ExitSpeed = 0.1;    // 스윙 종료 속도 임계 (정규화)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double HoldFast = 0.10;     // 시작 속도가 유지돼야 하는 최소 시간 (초)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    double HoldStill = 0.06;    // 종료 속도가 유지돼야 하는 최소 시간 (초)

    
};
