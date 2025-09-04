// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/ArrowClassifierComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Utils.h"

void UArrowClassifierComponent::Detect(EMainHand WhichHand)
{
    Super::Detect(WhichHand);

    if (WindowBuffer.Num() == 0 || !Receiver) return;

    const uint64 Now = NowMsFromReceiver();
    if (Now == 0) return;

    // 쿨다운
    if (LastArrowLoggedMs != 0)
    {
        const uint64 CoolMs = static_cast<uint64>(ArrowCooldownSeconds * 1000.0);
        if (Now - LastArrowLoggedMs < CoolMs) return;
    }

    // 윈도우에서 유효 스냅샷 모으기
    const uint64 SpanMs = static_cast<uint64>(WindowSeconds * 1000.0);
    const uint64 Oldest = (Now > SpanMs) ? (Now - SpanMs) : 0;

    struct FArrowSample { double T; float ShoulderW; float WristDist; bool Close; };
    TArray<FArrowSample> Smp; Smp.Reserve(WindowBuffer.Num());

    for (const auto& Snap : WindowBuffer)
    {
        if (Snap.TimestampMs < Oldest || Snap.Poses.Num() == 0) continue;

        int32 Idx = INDEX_NONE;
        if (!PickBestPerson(Snap.Poses, Idx)) continue;
        const auto& P = Snap.Poses[Idx];
        if (P.XY.Num() < 17) continue;

        float SW = 0.f, WD = 0.f;
        const bool bClose = IsHandsClose(P, SW, WD);
        if (SW <= KINDA_SMALL_NUMBER) continue;

        FArrowSample a;
        a.T = (double)Snap.TimestampMs * 0.001;
        a.ShoulderW = SW;
        a.WristDist = WD;
        a.Close = bClose;
        Smp.Add(MoveTemp(a));
    }
    if (Smp.Num() < 3) return;

    // 1) 시작: "가까운 상태"가 윈도우에 반드시 존재
    float MinWD = FLT_MAX;
    float MaxWD = 0.f;
    double TMin = 0.0, TMax = 0.0;
    bool bSawClose = false;

    for (const auto& a : Smp)
    {
        if (a.Close) bSawClose = true;
        if (a.WristDist < MinWD) { MinWD = a.WristDist; TMin = a.T; }
        if (a.WristDist > MaxWD) { MaxWD = a.WristDist; TMax = a.T; }
    }
    if (!bSawClose) return;

    // 2) 어깨폭 정규화
    double MeanSW = 0.0;
    for (const auto& a : Smp) MeanSW += (double)a.ShoulderW;
    MeanSW /= (double)Smp.Num();
    if (MeanSW <= KINDA_SMALL_NUMBER) return;

    const double MinNorm = (double)MinWD / MeanSW;
    const double MaxNorm = (double)MaxWD / MeanSW;
    const double DeltaNorm = MaxNorm - MinNorm;

    // 3) 멀어짐 크기/증가량/속도
    const bool bFarEnough = (MaxNorm >= (double)ArrowFarRatio);
    const bool bDeltaEnough = (DeltaNorm >= (double)ArrowMinDeltaNorm);

    double SpeedNorm = 0.0;
    if (TMax > TMin)
    {
        const double dt = TMax - TMin;
        SpeedNorm = DeltaNorm / dt; // (어깨폭/초)
    }
    const bool bFastEnough = (SpeedNorm >= (double)ArrowMinSpeedNorm);

    if (!(bFarEnough && bDeltaEnough && bFastEnough)) return;

    // -------- 여기서부터 "어깨/손목 월드 포지션" 산출 --------
    const FTimedPoseSnapshot& LastSnap = WindowBuffer.Last();
    int32 BestIdx = INDEX_NONE;
    if (!PickBestPerson(LastSnap.Poses, BestIdx)) return;
    const FPersonPose& P = LastSnap.Poses[BestIdx];
    if (P.XY.Num() < 17) return;

    // 양쪽 어깨/손목 2D
    if (!(P.XY.IsValidIndex(COCO_LSH) && P.XY.IsValidIndex(COCO_RSH) &&
        P.XY.IsValidIndex(COCO_LWR) && P.XY.IsValidIndex(COCO_RWR))) return;

    const FVector2f LSH = P.XY[COCO_LSH];
    const FVector2f RSH = P.XY[COCO_RSH];
    const FVector2f LWR = P.XY[COCO_LWR];
    const FVector2f RWR = P.XY[COCO_RWR];
    if (!IsFinite2D(LSH) || !IsFinite2D(RSH) || !IsFinite2D(LWR) || !IsFinite2D(RWR)) return;

    // 어느 팔이 더 “쭉 뻗었는지” (픽셀 기준: 어깨-손목 거리)
    const double LArmLenPx = FVector2D::Distance(FVector2D(LSH.X, LSH.Y), FVector2D(LWR.X, LWR.Y));
    const double RArmLenPx = FVector2D::Distance(FVector2D(RSH.X, RSH.Y), FVector2D(RWR.X, RWR.Y));
    const bool bUseRight = (RArmLenPx >= LArmLenPx);

    // 월드 변환
    FVector2f Pelvis2D;
    if (!GetPelvis2D(P, Pelvis2D)) return;

    const FVector ShoulderW = ToWorldFrom2D(Pelvis2D, bUseRight ? RSH : LSH, GetOwner()->GetActorTransform(), GetPixelToUU(), bInvertImageYToUp, GetDepthOffsetX());
    const FVector WristW = ToWorldFrom2D(Pelvis2D, bUseRight ? RWR : LWR, GetOwner()->GetActorTransform(), GetPixelToUU(), bInvertImageYToUp, GetDepthOffsetX());

    // 평면 데미지: 어깨가 중심, Y축은 어깨→손목 (ApplyArrowPlaneDamage의 어깨/손목 버전 사용)
    ApplyArrowDamage(ShoulderW, WristW);

    LastArrowLoggedMs = Now;
    WindowBuffer.Reset();
}

void UArrowClassifierComponent::ApplyArrowDamage(const FVector& ShoulderW, const FVector& WristW)
{
    if (!GetWorld()) return;

    // 카메라 시선
    FVector CamLoc, CamFwd;
    if (!GetCameraView(CamLoc, CamFwd)) return;

    // 어깨→손목 방향 (쭉 뻗은 팔의 실제 방향)
    FVector ArmDir = (WristW - ShoulderW).GetSafeNormal();
    if (ArmDir.IsNearlyZero())
    {
        // 폴백: 카메라 전방
        ArmDir = CamFwd.GetSafeNormal();
        if (ArmDir.IsNearlyZero()) ArmDir = FVector::ForwardVector;
    }

    // 법선 = CamFwd × ArmDir  (둘 다에 수직)
    FVector N = FVector::CrossProduct(CamFwd, ArmDir).GetSafeNormal();
    if (N.IsNearlyZero())
    {
        // 폴백: 카메라 전방과 월드업
        N = FVector::CrossProduct(CamFwd, FVector::UpVector).GetSafeNormal();
        if (N.IsNearlyZero()) N = FVector::RightVector;
    }

    // 평면 내 Z축 = N × ArmDir
    FVector Z = FVector::CrossProduct(N, ArmDir).GetSafeNormal();
    if (Z.IsNearlyZero()) Z = FVector::UpVector;

    // 회전행렬 → 쿼터니언 (X=N, Y=ArmDir, Z=N×Y)
    const FQuat Rot(FMatrix(
        FPlane(N.X, N.Y, N.Z, 0.f),
        FPlane(ArmDir.X, ArmDir.Y, ArmDir.Z, 0.f),
        FPlane(Z.X, Z.Y, Z.Z, 0.f),
        FPlane(0, 0, 0, 1)
    ));

    // 박스 형상
    const FVector HalfExtent(PlaneHalfThickness, PlaneHalfSize.X, PlaneHalfSize.Y);

    // 어깨가 중심
    const FVector Center = ShoulderW;

    // 오버랩 1회 (공유 헬퍼)
    TSet<AActor*> UniqueActors;
    // OverlapPlaneOnce(Center, Rot, UniqueActors, /*DebugLifeTime=*/1.0f);

    // 디버그(선택)
    if (bDrawDebugPlane)
    {
        //DrawDebugDirectionalArrow(GetWorld(), Center, Center + CamFwd * 120.f, 20.f, FColor::Blue, false, 1.0f, 0, 5.0f); // 카메라
        //DrawDebugDirectionalArrow(GetWorld(), Center, Center + ArmDir * 120.f, 20.f, FColor::Yellow, false, 1.0f, 0, 5.0f); // 어깨→손목(Y)
        //DrawDebugDirectionalArrow(GetWorld(), Center, Center + N * 120.f, 20.f, FColor::Magenta, false, 1.0f, 0, 5.0f); // 법선(X)

        //DrawDebugSphere(GetWorld(), ShoulderW, 6.f, 12, FColor::Green, false, 1.0f); // 실제 어깨
        //DrawDebugSphere(GetWorld(), Center, 6.f, 12, FColor::Cyan, false, 1.0f); // 보정된 센터
    }

    if (UniqueActors.Num() == 0) return;

    AController* Inst = GetOwner() ? GetOwner()->GetInstigatorController() : nullptr;
    for (AActor* A : UniqueActors)
    {
        if (A && A != GetOwner())
        {
            UGameplayStatics::ApplyDamage(A, ArrowDamageAmount, Inst, GetOwner(), DamageTypeClass);
        }
    }
}