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
    Niagara->bAutoActivate = false;           // 먼저 끄고 나중에 Activate
    Niagara->SetAutoDestroy(false);           // 액터가 수명 끝나면 함께 파괴
    // Subobject라 PoolingMethod = None (AutoDestroy 허용)
}

void AArcTrailActor::Init(const TArray<FVector>& Points, UNiagaraSystem* TrailNS,
    float MoveDurationSec, float TrailLifetimeSec)
{
    Duration = FMath::Max(0.01f, MoveDurationSec);
    TrailLife = FMath::Max(0.01f, TrailLifetimeSec);

    // 스플라인 세팅 (월드 포인트 그대로 추가)
    Spline->ClearSplinePoints(false);
    for (const FVector& P : Points)
        Spline->AddSplinePoint(P, ESplineCoordinateSpace::World, false);
    Spline->SetClosedLoop(false);
    Spline->UpdateSpline();
    Spline->SetUsingAbsoluteLocation(true);
    Spline->SetUsingAbsoluteRotation(true);
    Spline->SetUsingAbsoluteScale(true);

    SplineLen = Spline->GetSplineLength();     // 전체 길이. 이동은 거리 기반. :contentReference[oaicite:3]{index=3}

    // 나이아가라 에셋 지정 & 활성화
    Niagara->SetAsset(TrailNS);
    // NS 설정에서 반드시: CPU 시뮬, Local Space Off, Ribbon Renderer
    Niagara->Activate(true);

    // 액터 수명: 이동 + 리본 잔상 + 여유
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

    // 안정적 Up(월드 업). 평행이면 대체 Up 사용
    FVector Up = FVector::UpVector;
    if (FMath::Abs(FVector::DotProduct(Tangent, Up)) > 0.98f)
        Up = FVector::RightVector;

    const FRotator Rot = FRotationMatrix::MakeFromXZ(Tangent, Up).Rotator();

    // ★ 회전이 문제면, 우선 위치만 세팅해서 테스트해보세요.
    SetActorLocation(Loc);
    // 필요 시에만 회전 적용:
    // SetActorLocationAndRotation(Loc, Rot);
}

