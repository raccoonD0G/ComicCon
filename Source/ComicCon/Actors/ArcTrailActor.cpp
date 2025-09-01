// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/ArcTrailActor.h"
#include "NiagaraComponent.h"
#include "Components/SplineComponent.h"

AArcTrailActor::AArcTrailActor()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
    Spline->SetupAttachment(RootComponent);

    Niagara = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara"));
    Niagara->SetupAttachment(RootComponent);
    Niagara->bAutoActivate = false;           // ���� ���� ���߿� Activate
    Niagara->SetAutoDestroy(false);           // ���Ͱ� ���� ������ �Բ� �ı�
    // Subobject�� PoolingMethod = None (AutoDestroy ���)
}

void AArcTrailActor::Init(const TArray<FVector>& Points, UNiagaraSystem* TrailNS,
    float MoveDurationSec, float TrailLifetimeSec)
{
    Duration = FMath::Max(0.01f, MoveDurationSec);
    TrailLife = FMath::Max(0.01f, TrailLifetimeSec);

    // ���ö��� ���� (���� ����Ʈ �״�� �߰�)
    Spline->ClearSplinePoints(false);
    for (const FVector& P : Points)
        Spline->AddSplinePoint(P, ESplineCoordinateSpace::World, false);
    Spline->SetClosedLoop(false);
    Spline->UpdateSpline();
    Spline->SetUsingAbsoluteLocation(true);
    Spline->SetUsingAbsoluteRotation(true);
    Spline->SetUsingAbsoluteScale(true);

    SplineLen = Spline->GetSplineLength();     // ��ü ����. �̵��� �Ÿ� ���. :contentReference[oaicite:3]{index=3}

    // ���̾ư��� ���� ���� & Ȱ��ȭ
    Niagara->SetAsset(TrailNS);
    // NS �������� �ݵ��: CPU �ù�, Local Space Off, Ribbon Renderer
    Niagara->Activate(true);

    // ���� ����: �̵� + ���� �ܻ� + ����
    SetLifeSpan(Duration + TrailLife + 0.2f);
}

// ArcTrailActor.cpp (Tick)
void AArcTrailActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Duration <= 0.f || SplineLen <= 0.f) return;

    Elapsed += DeltaSeconds;
    const float Alpha = FMath::Clamp(Elapsed / Duration, 0.f, 1.f);
    const float Distance = Alpha * SplineLen;

    const FVector  Loc = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
    const FVector  Tangent = Spline->GetDirectionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

    // ������ Up(���� ��). �����̸� ��ü Up ���
    FVector Up = FVector::UpVector;
    if (FMath::Abs(FVector::DotProduct(Tangent, Up)) > 0.98f)
        Up = FVector::RightVector;

    const FRotator Rot = FRotationMatrix::MakeFromXZ(Tangent, Up).Rotator();

    // �� ȸ���� ������, �켱 ��ġ�� �����ؼ� �׽�Ʈ�غ�����.
    SetActorLocation(Loc);
    // �ʿ� �ÿ��� ȸ�� ����:
    // SetActorLocationAndRotation(Loc, Rot);
}

