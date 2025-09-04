// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/PoseClassifierComponent.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/SplineComponent.h"
#include "Utils.h"

UPoseClassifierComponent::UPoseClassifierComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UPoseClassifierComponent::BeginPlay()
{
    Super::BeginPlay();

    // Receiver 자동 탐색(같은 Actor에 있으면)
    if (!Receiver)
    {
        if (AActor* Owner = GetOwner())
        {
            Receiver = Owner->FindComponentByClass<UPoseUdpReceiverComponent>();
        }
    }
}

void UPoseClassifierComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    WindowBuffer.Reset();
    Super::EndPlay(EndPlayReason);
}

void UPoseClassifierComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    IngestLatestFromReceiver();

    // 리시버 타임스탬프 기준으로 오래된 스냅샷 제거
    PruneOld(NowMsFromReceiver());
    Detect(HandSource);
}

void UPoseClassifierComponent::IngestLatestFromReceiver()
{
    if (!Receiver) return;

    const uint64 Ts = Receiver->GetLatestTimestamp();
    if (Ts == 0 || Ts == LastIngestedTs) return; // 중복 회피

    const TArray<FPersonPose>& LatestPoses = Receiver->GetLatestPoses();

    FTimedPoseSnapshot Snap;
    Snap.TimestampMs = Ts;
    Snap.Poses = LatestPoses;             // 값 복사

    // v2: 손 데이터도 함께 스냅샷에 저장 (v1이면 빈 배열)
    if (Receiver->GetLatestHands().Num() > 0)
    {
        const TArray<FHandPose>& LatestHands = Receiver->GetLatestHands();
        Snap.Hands = LatestHands;              // 값 복사
    }

    WindowBuffer.Add(MoveTemp(Snap));
    LastIngestedTs = Ts;

    OnPoseWindowUpdated.Broadcast();
}

void UPoseClassifierComponent::PruneOld(uint64 NowMs)
{
    if (WindowSeconds <= 0.f) { WindowBuffer.Reset(); return; }

    const uint64 SpanMs = static_cast<uint64>(WindowSeconds * 1000.0f);
    const uint64 Cutoff = (NowMs > SpanMs) ? (NowMs - SpanMs) : 0;

    int32 FirstValidIdx = 0;
    while (FirstValidIdx < WindowBuffer.Num() && WindowBuffer[FirstValidIdx].TimestampMs < Cutoff)
    {
        ++FirstValidIdx;
    }
    if (FirstValidIdx > 0)
    {
        WindowBuffer.RemoveAt(0, FirstValidIdx, /*bAllowShrinking*/ false);
    }
}

uint64 UPoseClassifierComponent::NowMsFromReceiver() const
{
    // 수신 타임스탬프(송신 기준 ms)를 시간 기준으로 사용
    if (Receiver)
    {
        return Receiver->GetLatestTimestamp();
    }
    return 0;
}

int32 UPoseClassifierComponent::GetWindowTotalPersons() const
{
    int32 Sum = 0;
    for (const auto& Snap : WindowBuffer)
    {
        Sum += Snap.Poses.Num();
    }
    return Sum;
}

bool UPoseClassifierComponent::GetLatestJoint(int32 CocoIndex, FVector2D& OutXY) const
{
    if (WindowBuffer.Num() == 0) return false;

    const FTimedPoseSnapshot& Last = WindowBuffer.Last();
    if (Last.Poses.Num() == 0) return false;

    const FPersonPose& P = Last.Poses[0]; // 최신 스냅샷의 첫 사람
    if (!P.XY.IsValidIndex(CocoIndex)) return false;

    const FVector2f& V = P.XY[CocoIndex];
    if (!FMath::IsFinite(V.X) || !FMath::IsFinite(V.Y)) return false;

    OutXY = FVector2D(V.X, V.Y);
    return true;
}

int32 UPoseClassifierComponent::GetDominantPersonId() const
{
    TMap<int32, int32> Count;
    for (const auto& Snap : WindowBuffer)
    {
        for (const auto& PP : Snap.Poses)
        {
            Count.FindOrAdd((int32)PP.PersonId)++;
        }
    }
    int32 BestId = -1;
    int32 BestCnt = -1;
    for (const auto& It : Count)
    {
        if (It.Value > BestCnt)
        {
            BestCnt = It.Value;
            BestId = It.Key;
        }
    }
    return BestId;
}

bool UPoseClassifierComponent::PickBestPerson(const TArray<FPersonPose>& Poses, int32& OutIdx) const
{
    // "가장 손이 가까운 사람"을 선택 (프레임마다 주 대상 선택)
    float BestDist = TNumericLimits<float>::Max();
    int32 Best = INDEX_NONE;
    for (int32 i = 0; i < Poses.Num(); ++i)
    {
        const auto& P = Poses[i];
        if (P.XY.Num() < 17) continue;

        const FVector2f& LWR = P.XY[9];
        const FVector2f& RWR = P.XY[10];
        const FVector2f& LSH = P.XY[5];
        const FVector2f& RSH = P.XY[6];

        if (!IsFinite2D(LWR) || !IsFinite2D(RWR) || !IsFinite2D(LSH) || !IsFinite2D(RSH)) continue;

        const float ShoulderW = FMath::Abs(RSH.X - LSH.X);
        if (ShoulderW <= KINDA_SMALL_NUMBER) continue;

        const float Dist = FVector2f::Distance(LWR, RWR);
        if (Dist < BestDist)
        {
            BestDist = Dist;
            Best = i;
        }
    }
    OutIdx = Best;
    return Best != INDEX_NONE;
}

bool UPoseClassifierComponent::IsHandsClose(const FPersonPose& P, float& OutShoulderW, float& OutWristDist) const
{
    const FVector2f& LWR = P.XY[9];
    const FVector2f& RWR = P.XY[10];
    const FVector2f& LSH = P.XY[5];
    const FVector2f& RSH = P.XY[6];

    if (!IsFinite2D(LWR) || !IsFinite2D(RWR) || !IsFinite2D(LSH) || !IsFinite2D(RSH))
        return false;

    const float ShoulderW = FMath::Abs(RSH.X - LSH.X);
    if (ShoulderW <= KINDA_SMALL_NUMBER)
        return false;

    const float WristDist = FVector2f::Distance(LWR, RWR);

    OutShoulderW = ShoulderW;
    OutWristDist = WristDist;
    return (WristDist <= HandsCloseRatio * ShoulderW);
}

FVector UPoseClassifierComponent::Map2DToLocal(const FVector2f& P, const FVector2f& Pelvis2D) const
{
    const FVector2f Rel = P - Pelvis2D;          // 골반 기준 상대(픽셀)
    const float Y = Rel.X * Receiver->GetPixelToUU();          // 화면 X → 로컬 Y
    const float Z = -Rel.Y * Receiver->GetPixelToUU();          // 화면 Y(아래+) → 로컬 Z(위+) 부호 반전
    return FVector(Receiver->GetDepthOffsetX(), Y, Z);          // 로컬 X는 고정 오프셋(액터 앞)
}

bool UPoseClassifierComponent::GetExtendedArmWorld(const FPersonPose& Pose, FVector& OutStart, FVector& OutDir, FVector& OutUpperArmDir) const
{
    if (Pose.XY.Num() < 17 || !GetOwner()) return false;

    // 2D 포인트
    const FVector2f& LHip = Pose.XY[11];
    const FVector2f& RHip = Pose.XY[12];
    const FVector2f& LSh = Pose.XY[5];
    const FVector2f& RSh = Pose.XY[6];
    const FVector2f& LWr = Pose.XY[9];
    const FVector2f& RWr = Pose.XY[10];

    auto Finite2D = [](const FVector2f& V)
        {
            return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y);
        };
    if (!Finite2D(LHip) || !Finite2D(RHip) || !Finite2D(LSh) || !Finite2D(RSh) || !Finite2D(LWr) || !Finite2D(RWr))
        return false;

    const FVector2f Pelvis2D = (LHip + RHip) * 0.5f;

    // 2D → 로컬(Y=오른쪽, Z=위), X=깊이(고정 오프셋)
    auto Map2DToLocal = [&](const FVector2f& P)->FVector
        {
            const FVector2f Rel = P - Pelvis2D;
            const float Y = Rel.X * Receiver->GetPixelToUU();                 // 화면 X → 로컬 Y
            const float Z = (bInvertImageYToUp ? -Rel.Y : Rel.Y) * Receiver->GetPixelToUU(); // 화면 Y(아래+) → 로컬 Z(위+)
            return FVector(Receiver->GetDepthOffsetX(), Y, Z);
        };

    // 로컬 좌표
    const FVector LShL = Map2DToLocal(LSh);
    const FVector RShL = Map2DToLocal(RSh);
    const FVector LWrL = Map2DToLocal(LWr);
    const FVector RWrL = Map2DToLocal(RWr);

    // 손 선택: Auto(더 뻗은 팔) / Left / Right
    const float LenL = (LWrL - LShL).Size();
    const float LenR = (RWrL - RShL).Size();

    bool bUseLeft = (LenL >= LenR);
    switch (HandSource)
    {
    case EMainHand::Left:  bUseLeft = true;  break;
    case EMainHand::Right: bUseLeft = false; break;
    case EMainHand::Auto:  /* keep bUseLeft as decided by reach */ break;
    }

    const FVector ShL = bUseLeft ? LShL : RShL;
    const FVector WrL = bUseLeft ? LWrL : RWrL;

    const FTransform& Xf = GetOwner()->GetActorTransform();

    // 월드 변환
    const FVector LShW = Xf.TransformPosition(LShL);
    const FVector RShW = Xf.TransformPosition(RShL);
    const FVector ShW = Xf.TransformPosition(ShL);
    const FVector WrW = Xf.TransformPosition(WrL);

    // 어깨→손목 벡터 (월드)
    const FVector vSW = (WrW - ShW);
    const float   len = vSW.Size();
    if (len <= KINDA_SMALL_NUMBER) return false;

    const FVector Dir = vSW / len; // 진행 방향(어깨→손목)

    // 시작점 결정:
    //  - bStartAtShoulderMid == true  → 어깨 중점
    //  - else StartAlongArmRatio      → 어깨에서 손목 방향으로 비율만큼 이동 (1=손목, >1=손목 바깥)
    FVector StartW = ShW;
    if (bStartAtShoulderMid)
    {
        const FVector ShoulderMidW = 0.5f * (LShW + RShW);
        StartW = ShoulderMidW;
    }
    else
    {
        StartW = ShW + vSW * StartAlongArmRatio;
    }

    // UpperArmDir: 평면 구성용 축. 기본은 Dir 과 동일(안정적).
    FVector Upper = Dir;

    OutStart = StartW;
    OutDir = Dir;
    OutUpperArmDir = Upper;
    return true;
}

bool UPoseClassifierComponent::GetCameraView(FVector& OutCamLoc, FVector& OutCamForward) const
{
    OutCamLoc = FVector::ZeroVector;
    OutCamForward = FVector::ForwardVector;

    if (!GetWorld()) return false;

    // 우선: 오너가 플레이어가 조종 중이면 그 컨트롤러의 뷰포인트
    if (AActor* Owner = GetOwner())
    {
        if (APawn* Pawn = Cast<APawn>(Owner))
        {
            if (AController* Ctrl = Pawn->GetController())
            {
                FVector Loc; FRotator Rot;
                Ctrl->GetPlayerViewPoint(Loc, Rot);
                OutCamLoc = Loc;
                OutCamForward = Rot.Vector().GetSafeNormal();
                return true;
            }
        }
    }

    // 폴백: 첫 번째 플레이어 카메라
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        FVector Loc; FRotator Rot;
        PC->GetPlayerViewPoint(Loc, Rot);
        OutCamLoc = Loc;
        OutCamForward = Rot.Vector().GetSafeNormal();
        return true;
    }
    return false;
}

bool UPoseClassifierComponent::GetShoulderMidWorld(FVector& OutMidWorld) const
{
    OutMidWorld = FVector::ZeroVector;
    if (!Receiver || !GetOwner()) return false;

    const TArray<FPersonPose>& Poses = Receiver->GetLatestPoses();
    if (Poses.Num() == 0) return false;

    const int32 LSH = 5, RSH = 6, LHIP = 11, RHIP = 12;

    // 가장 신뢰도 높은 양어깨/양힙 유효 사람 선택
    int32 BestIdx = INDEX_NONE;
    float BestScore = -FLT_MAX;

    for (int32 i = 0; i < Poses.Num(); ++i)
    {
        const FPersonPose& P = Poses[i];
        if (!(P.XY.IsValidIndex(LSH) && P.XY.IsValidIndex(RSH) &&
            P.XY.IsValidIndex(LHIP) && P.XY.IsValidIndex(RHIP))) continue;

        const FVector2f& LSh = P.XY[LSH], & RSh = P.XY[RSH], & LHip = P.XY[LHIP], & RHip = P.XY[RHIP];
        if (!(IsFinite2D(LSh) && IsFinite2D(RSh) && IsFinite2D(LHip) && IsFinite2D(RHip))) continue;

        const float cL = (P.Conf.IsValidIndex(LSH) ? P.Conf[LSH] : 1.f);
        const float cR = (P.Conf.IsValidIndex(RSH) ? P.Conf[RSH] : 1.f);
        const float Score = cL + cR;

        if (Score > BestScore) { BestScore = Score; BestIdx = i; }
    }
    if (BestIdx == INDEX_NONE) return false;

    const FPersonPose& Best = Poses[BestIdx];
    const FVector2f Pelvis2D = (Best.XY[LHIP] + Best.XY[RHIP]) * 0.5f;

    // 이미지→로컬(Y=오른쪽, Z=위, X=깊이)
    auto Map2DToLocal = [&](const FVector2f& P)->FVector
        {
            const FVector2f Rel = P - Pelvis2D;
            const float Y = Rel.X * Receiver->GetPixelToUU();                                   // imgX -> localY
            const float Z = (bInvertImageYToUp ? -Rel.Y : Rel.Y) * Receiver->GetPixelToUU();    // imgY(↓) -> localZ(↑)
            return FVector(Receiver->GetDepthOffsetX(), Y, Z);
        };

    const FVector LShL = Map2DToLocal(Best.XY[LSH]);
    const FVector RShL = Map2DToLocal(Best.XY[RSH]);

    const FTransform& OwnerXf = GetOwner()->GetActorTransform();
    const FVector LShW = OwnerXf.TransformPosition(LShL);
    const FVector RShW = OwnerXf.TransformPosition(RShL);

    OutMidWorld = 0.5f * (LShW + RShW);
    return true;
}

void UPoseClassifierComponent::Detect(EMainHand WhichHand)
{
}



