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

    // === �⺻ �̵�/ȸ�� �Ķ���� ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing")
    float SpeedUUPerSec = 2500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing")
    bool bLoop = false;

    // �Ķ�(Z)���� ���(U) �������� ƿƮ ��(��)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing")
    float TiltDeg = 0.f;

    // ArcPoints�� ���޹޴� ���� (�ּ� �䱸)
    UFUNCTION(BlueprintCallable, Category = "Swing")
    void InitializeWithArc(const TArray<FVector>& InArcPoints);

    // ����: �� ��Ȯ�� ƿƮ�� ���ϸ� Rot0/Rot1�� �Բ�(���� ��� �� ���� �� ����)
    UFUNCTION(BlueprintCallable, Category = "Swing")
    void InitializeWithArcAndRot(const TArray<FVector>& InArcPoints, const FQuat& InRot0, const FQuat& InRot1);

protected:
    virtual void Tick(float DeltaSeconds) override;
    virtual void BeginPlay() override;

private:
    // ���
    TArray<FVector> ArcPoints;
    TArray<float>   CumLen;      // ���� �Ÿ� ���̺�
    float           TotalLen = 0.f;
    float           Progress = 0.f;

    // �� ����(�ɼ�)
    bool  bHasRotPair = false;
    FQuat Rot0, Rot1;

    // ��ƿ
    static FVector SafeNorm(const FVector& V, const FVector& Fallback);
    void BuildCumLen();
    bool FindSegAlpha(float Dist, int32& OutI, float& OutA) const;

    // ƿƮ ȸ�� �����(�� ���� ���)
    // (A) Rot0/Rot1�� ���� ��: N=AxisX, U=AxisY �� Z=N��U �� Z�� U������ Tilt
    FQuat MakeTiltedQuatFromRotPair(float T) const;

    // (B) Arc�� ���� ��: ���� ��� ������-������ �ٻ�. Z_base�� Binormal�� �ΰ� Tilt�� WorldUp ���� ���� ����
    FQuat MakeTiltedQuatFromTangent(int32 I, float A) const;
};