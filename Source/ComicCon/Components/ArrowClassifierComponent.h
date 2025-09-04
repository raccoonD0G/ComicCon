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
    // === Awrrow 판별 파라미터 ===
    // "충분히 멀어짐" 종료 조건: 손목 간 거리 ≥ ArrowFarRatio * 어깨폭
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow", meta = (AllowPrivateAccess = "true"))
    float ArrowFarRatio = 1.8f;            // 어깨폭의 1.2배 이상 벌어지면 화살 발사로 간주

    // 윈도우 내에서 가까움→멀어짐 증가량(정규화) 최소치 (히스테리시스)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow", meta = (AllowPrivateAccess = "true"))
    float ArrowMinDeltaNorm = 0.5f;        // (멀어짐 - 가까움) / 어깨폭

    // 멀어지는 속도(정규화: 어깨폭/초) 최소치 (너무 느린 변화 방지)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow", meta = (AllowPrivateAccess = "true"))
    float ArrowMinSpeedNorm = 1.0f;

    // 로그 후 쿨다운(초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow", meta = (AllowPrivateAccess = "true"))
    float ArrowCooldownSeconds = 1.0f;

    // ===== Arrow 평면(Plane) 데미지 파라미터 =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Plane", meta = (AllowPrivateAccess = "true"))
    float PlaneDistance = 150.f;        // 시작점(Start)에서 방향(Dir)로 얼마나 떨어진 곳에 평면을 둘지

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Plane", meta = (AllowPrivateAccess = "true"))
    float ArrowPlaneHalfThickness = 20.f;     // 시선 방향(X) 두께 (얇을수록 '면'에 가까움)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Damage", meta = (AllowPrivateAccess = "true"))
    float ArrowDamageAmount = 2.f;        // 데미지 양

private:
    uint64 LastArrowLoggedMs = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    FVector2D PlaneHalfSize = FVector2D(5000.f, 1000.f); // 평면 절반 크기(Y=가로, Z=세로)

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

    // 보간 평면 최소/최대 개수(엔드포인트 포함)
    UPROPERTY(EditAnywhere, Category = "Swing")
    int32 MinInterpPlanes = 2;

    UPROPERTY(EditAnywhere, Category = "Swing")
    int32 MaxInterpPlanes = 24;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose|Arrow|Damage", meta = (AllowPrivateAccess = "true"))
    float SingleSwingDamageAmount = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UDamageType> DamageTypeClass;

    virtual void Detect(EMainHand WhichHand) override; // ← Tick에서 호출

    void ApplyArrowDamage(const FVector& ShoulderW, const FVector& WristW);
};
