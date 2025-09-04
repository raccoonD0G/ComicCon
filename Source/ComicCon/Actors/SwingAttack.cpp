// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/SwingAttack.h"
#include "Kismet/KismetMathLibrary.h"

ASwingAttack::ASwingAttack()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ASwingAttack::BeginPlay()
{
    Super::BeginPlay();
}

static FORCEINLINE float ClampSmall(float v) { return FMath::Max(v, 1e-6f); }

FVector ASwingAttack::SafeNorm(const FVector& V, const FVector& Fallback)
{
    FVector R = V;
    if (!R.Normalize()) R = Fallback;
    return R;
}

void ASwingAttack::InitializeWithArc(const TArray<FVector>& InArcPoints)
{
    ArcPoints = InArcPoints;
    bHasRotPair = false;
    BuildCumLen();
    Progress = 0.f;
}

void ASwingAttack::InitializeWithArcAndRot(const TArray<FVector>& InArcPoints, const FQuat& InRot0, const FQuat& InRot1)
{
    ArcPoints = InArcPoints;
    Rot0 = InRot0;
    Rot1 = InRot1;
    bHasRotPair = true;
    BuildCumLen();
    Progress = 0.f;
}

void ASwingAttack::BuildCumLen()
{
    CumLen.Reset();
    TotalLen = 0.f;

    if (ArcPoints.Num() < 2) return;

    CumLen.Reserve(ArcPoints.Num());
    CumLen.Add(0.f);
    for (int32 i = 1; i < ArcPoints.Num(); ++i)
    {
        TotalLen += FVector::Distance(ArcPoints[i - 1], ArcPoints[i]);
        CumLen.Add(TotalLen);
    }
}

bool ASwingAttack::FindSegAlpha(float Dist, int32& OutI, float& OutA) const
{
    if (ArcPoints.Num() < 2 || CumLen.Num() != ArcPoints.Num()) return false;

    const float D = FMath::Clamp(Dist, 0.f, TotalLen);

    int32 lo = 0, hi = CumLen.Num() - 1, mid = 0;
    while (lo + 1 < hi)
    {
        mid = (lo + hi) / 2;
        (CumLen[mid] <= D) ? lo = mid : hi = mid;
    }

    OutI = lo;
    const float segLen = ClampSmall(CumLen[lo + 1] - CumLen[lo]);
    OutA = (D - CumLen[lo]) / segLen;
    return true;
}

FQuat ASwingAttack::MakeTiltedQuatFromRotPair(float T) const
{
    // Rot0→Rot1 Slerp에서 축 복원
    const FQuat RotK = FQuat::Slerp(Rot0, Rot1, T).GetNormalized();
    const FVector N = SafeNorm(RotK.GetAxisX(), FVector(1, 0, 0));   // X
    const FVector U = SafeNorm(RotK.GetAxisY(), FVector(0, 1, 0));   // Y
    const FVector Z_base = SafeNorm(FVector::CrossProduct(N, U), FVector::UpVector);

    // 회전축: Z × U
    FVector Axis = FVector::CrossProduct(Z_base, U);
    if (!Axis.Normalize())
    {
        Axis = FVector::CrossProduct(Z_base, FVector::UpVector);
        if (!Axis.Normalize()) Axis = FVector::RightVector;
    }

    const FQuat Q(Axis, FMath::DegreesToRadians(TiltDeg));
    const FVector Z_tilt = SafeNorm(Q.RotateVector(Z_base), FVector::UpVector);

    // X=N, Z=Z_tilt를 동시에 만족하는 회전(직교화 포함)
    const FRotator R = UKismetMathLibrary::MakeRotFromXZ(N, Z_tilt);
    return R.Quaternion();
}

FQuat ASwingAttack::MakeTiltedQuatFromTangent(int32 I, float A) const
{
    // 접선 기반 프레임: T = (Pi+1 - Pi-1). Normalize.
    const int32 last = ArcPoints.Num() - 1;
    const int32 ip = (I > 0) ? I - 1 : 0;
    const int32 in = (I < last - 1) ? I + 1 : last;

    const FVector Pp = ArcPoints[ip];
    const FVector Pn = ArcPoints[in];
    const FVector Tvec = SafeNorm(Pn - Pp, FVector(1, 0, 0));    // tangent

    // 하나의 기준 Up (WorldUp)에서 수직 성분 추출 → N_tangent = (Up ⟂ T)
    FVector Up = FVector::UpVector;
    FVector Nvec = Up - FVector::DotProduct(Up, Tvec) * Tvec;
    Nvec = SafeNorm(Nvec, FVector::RightVector);               // normal

    // Binormal = T × N  (이게 파란 Z_base 유사)
    const FVector Z_base = SafeNorm(FVector::CrossProduct(Tvec, Nvec), FVector::UpVector);

    // U역할을 Nvec으로 보고 Z를 그쪽으로 Tilt (Arc만 있을 때의 근사)
    FVector Axis = FVector::CrossProduct(Z_base, Nvec);
    if (!Axis.Normalize())
    {
        Axis = FVector::CrossProduct(Z_base, FVector::UpVector);
        if (!Axis.Normalize()) Axis = FVector::RightVector;
    }

    const FQuat Q(Axis, FMath::DegreesToRadians(TiltDeg));
    const FVector Z_tilt = SafeNorm(Q.RotateVector(Z_base), FVector::UpVector);

    // X는 Tvec(진행 방향), Z=Z_tilt를 만족
    const FRotator R = UKismetMathLibrary::MakeRotFromXZ(Tvec, Z_tilt);
    return R.Quaternion();
}

void ASwingAttack::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (ArcPoints.Num() < 2 || TotalLen <= KINDA_SMALL_NUMBER)
        return;

    // 이동 거리 누적
    Progress += SpeedUUPerSec * DeltaSeconds;

    float Dist = Progress;
    if (bLoop)
    {
        Dist = FMath::Fmod(Progress, TotalLen);
        if (Dist < 0.f) Dist += TotalLen;
    }
    else
    {
        Dist = FMath::Clamp(Progress, 0.f, TotalLen);
    }

    int32 I = 0; float A = 0.f;
    if (!FindSegAlpha(Dist, I, A))
        return;

    const FVector Pos = FMath::Lerp(ArcPoints[I], ArcPoints[I + 1], A);

    // 회전 계산 (전처럼 Rot0/Rot1 있으면 보간, 아니면 탄젠트 기반)
    FQuat Rot;
    if (bHasRotPair)
    {
        const float T = (TotalLen > 0.f) ? (Dist / TotalLen) : 0.f;
        Rot = MakeTiltedQuatFromRotPair(T);
    }
    else
    {
        Rot = MakeTiltedQuatFromTangent(I, A);
    }

    SetActorLocationAndRotation(Pos, Rot, false, nullptr, ETeleportType::TeleportPhysics);

    // === 도착 체크 ===
    if (!bLoop && Progress >= TotalLen)
    {
        Destroy();
    }
}
