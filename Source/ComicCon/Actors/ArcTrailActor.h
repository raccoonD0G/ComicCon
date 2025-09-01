// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcTrailActor.generated.h"

UCLASS()
class COMICCON_API AArcTrailActor : public AActor
{
    GENERATED_BODY()

public:
    AArcTrailActor();

    // Points: ���� ��ǥ ���ö��� ����Ʈ
    void Init(const TArray<FVector>& Points, class UNiagaraSystem* TrailNS, float MoveDurationSec, float TrailLifetimeSec);

protected:
    virtual void Tick(float DeltaSeconds) override;

private:
    UPROPERTY()
    TObjectPtr<USceneComponent> Root;
    UPROPERTY()
    TObjectPtr<class USplineComponent> Spline;
    UPROPERTY()
    TObjectPtr<class UNiagaraComponent> Niagara;   // Subobject�� Ǯ�� �̽� ����

    float SplineLen = 0.f;
    float Duration = 0.3f;   // �̵� �ð�
    float TrailLife = 0.3f;   // ��ƼŬ ����
    float Elapsed = 0.f;

};
