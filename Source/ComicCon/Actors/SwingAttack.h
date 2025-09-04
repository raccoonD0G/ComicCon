// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SwingAttack.generated.h"

UCLASS()
class COMICCON_API ASwingAttack : public AActor
{
    GENERATED_BODY()

public:
    ASwingAttack();

    // === 기본 이동/회전 파라미터 ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing")
    float SpeedUUPerSec = 2500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing")
    bool bLoop = false;

    // 파란(Z)에서 노란(U) 쪽으로의 틸트 각(도)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing")
    float TiltDeg = 0.f;

    // ArcPoints만 전달받는 버전 (최소 요구)
    UFUNCTION(BlueprintCallable, Category = "Swing")
    void InitializeWithArc(const TArray<FVector>& InArcPoints);

    // 선택: 더 정확한 틸트를 원하면 Rot0/Rot1도 함께(스윙 계산 때 쓰던 축 복원)
    UFUNCTION(BlueprintCallable, Category = "Swing")
    void InitializeWithArcAndRot(const TArray<FVector>& InArcPoints, const FQuat& InRot0, const FQuat& InRot1);

protected:
    virtual void Tick(float DeltaSeconds) override;
    virtual void BeginPlay() override;

private:
    // 경로
    TArray<FVector> ArcPoints;
    TArray<float>   CumLen;      // 누적 거리 테이블
    float           TotalLen = 0.f;
    float           Progress = 0.f;

    // 축 보간(옵션)
    bool  bHasRotPair = false;
    FQuat Rot0, Rot1;

    // 유틸
    static FVector SafeNorm(const FVector& V, const FVector& Fallback);
    void BuildCumLen();
    bool FindSegAlpha(float Dist, int32& OutI, float& OutA) const;

    // 틸트 회전 만들기(두 가지 경로)
    // (A) Rot0/Rot1이 있을 때: N=AxisX, U=AxisY → Z=N×U → Z를 U쪽으로 Tilt
    FQuat MakeTiltedQuatFromRotPair(float T) const;

    // (B) Arc만 있을 때: 접선 기반 프레네-프레임 근사. Z_base를 Binormal로 두고 Tilt는 WorldUp 방향 기준 보정
    FQuat MakeTiltedQuatFromTangent(int32 I, float A) const;
};