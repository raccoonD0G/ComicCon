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
    // === Swing 판별 파라미터 ===
    // 속도 계산 시 포함할 프레임의 "손 가까움" 비율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    float MinCloseCoverage = 0.2f; // 80% 이상 가까워야

    // 로그 중복 방지 쿨다운(초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing", meta = (AllowPrivateAccess = "true"))
    float SwingCooldownSeconds = 0.2f;

    // 스윙 속도 강화용 파라미터
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Strict", meta = (AllowPrivateAccess = "true"))
    float SwingRecentSeconds = 0.3f;          // 최근 이 시간 내 구간만 사용(예: 0.6~1.0s)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Strict", meta = (AllowPrivateAccess = "true"))
    float MinPathLenNorm = 1.5f;              // 이동 누적거리(어깨폭 단위) 최저치

    

    // === Down-swing 전용 판정 파라미터 ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float DownVyThresholdNorm = 0.40f;      // vy_norm > 이 값일 때 '충분히 아래로 빠름'으로 집계(어깨폭/초)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
    float MinDownCoverage = 0.45f;          // vy_norm > DownVyThresholdNorm 인 샘플 비율의 하한(0~1)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float MinDownAvgSpeedNorm = 0.55f;      // 평균 ↓속도(어깨폭/초)의 하한

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float MinDownPeakSpeedNorm = 1.00f;     // 최대 ↓속도(어깨폭/초)의 하한

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float MinDownDeltaNorm = 0.70f;         // 시작→끝 순수 ↓변위(어깨폭 단위) 최소치

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
    int32 MaxVertReversals = 1;             // 수직 방향(위/아래) 반전 허용 횟수

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Swing|Down", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float VertHoldSeconds = 0.06f;          // 반전 유효 디바운스 시간(초). 이 시간 이상 지속 시 '진짜 반전'으로 카운트

           // 데미지 양

    // --- VFX: 스윙 아크 트레일 ---
    UPROPERTY(EditAnywhere, Category = "VFX|SwingArc")
    bool bSpawnArcTrail = true;

    UPROPERTY(VisibleAnywhere, Category = "VFX|SwingArc")
    TObjectPtr<class USplineComponent> SplineComponent;

    UPROPERTY(VisibleAnywhere, Category = "VFX|SwingArc")
    TObjectPtr<class UNiagaraComponent> NiagaraComponent;

    UPROPERTY(EditAnywhere, Category = "VFX|SwingArc", meta = (ClampMin = "0.05", ClampMax = "5.0"))
    float ArcTrailLifetime = 1.0f;

    uint64 LastSwingLoggedMs = 0;

    // 각도 간격(도). 이 값이 작을수록 더 촘촘히 샘플링
    UPROPERTY(EditAnywhere, Category = "Swing|Sweep")
    float DegreesPerInterp = 12.f;

    virtual void Detect(EMainHand WhichHand) override; // <- Tick에서 호출
};
