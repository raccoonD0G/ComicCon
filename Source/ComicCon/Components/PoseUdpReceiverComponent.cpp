// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/PoseUdpReceiverComponent.h"
#include "HAL/RunnableThread.h"
#include "Common/UdpSocketBuilder.h"
#include "DrawDebugHelpers.h"
#include "Utils.h"

// COCO skeleton edges
static const int32 EDGES[][2] = {
    {0,1},{0,2},{1,3},{2,4},
    {5,6},
    {5,7},{7,9},
    {6,8},{8,10},
    {11,12},
    {5,11},{6,12},
    {11,13},{13,15},
    {12,14},{14,16}
};
static const int32 NUM_EDGES = sizeof(EDGES) / sizeof(EDGES[0]);

// 시각화 파라미터(원하면 UPROPERTY로 빼서 에디터에서 조정)
const float PixelToUU = 1.0f; // 픽셀→언리얼 유닛 스케일
const float DepthOffsetX = 0.f; // 액터 로컬 X(전방)으로 살짝 띄우기
const float LineThickness = 2.f;
const float PointRadius = 1.5f;
const FColor LineColor = FColor::Yellow;
const FColor PointColor = FColor::White;

// 2D->로컬 변환: 엉덩이 중앙을 원점으로, +Y=우측, +Z=위쪽
auto MakeLocal = [&](const FVector2f& P, const FVector2f& Pelvis2D)->FVector
    {
        FVector2f Rel = P - Pelvis2D; // 골반 기준 상대좌표(픽셀)
        // 화면 y는 아래로 증가하므로 Z에서 부호 반전
        const float Y = Rel.X * PixelToUU;
        const float Z = -Rel.Y * PixelToUU;
        return FVector(DepthOffsetX, Y, Z); // X는 고정 오프셋으로 살짝 띄움
    };

static const int32 EXPECTED_PERSON_BLOCK = 2 + (34 * 4) + (17 * 4); // person_id + xy + conf = 2 + 136 + 68 = 206 bytes
static const int32 HEADER_SIZE = 16;

UPoseUdpReceiverComponent::UPoseUdpReceiverComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UPoseUdpReceiverComponent::BeginPlay()
{
    Super::BeginPlay();
    RecvBuffer.SetNumUninitialized(2048);
    InitSocket(7777);
}

void UPoseUdpReceiverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!GetWorld() || !GetOwner()) return;

    TArray<FPersonPose> Poses;
    uint64 Ts = 0;

    // 패킷이 한 틱에 여러 개 들어올 수 있어 while로 모두 처리
    while (ReceiveOnce(Poses, Ts))
    {
        const FTransform& OwnerXform = GetOwner()->GetActorTransform();

        for (const FPersonPose& Pose : Poses)
        {
            LatestPoses = Poses;
            LatestTimestamp = Ts;

            if (Pose.XY.Num() < 17) continue;

            const FVector2f& LHip = Pose.XY[11];
            const FVector2f& RHip = Pose.XY[12];
            if (!(IsFinite2D(LHip) && IsFinite2D(RHip))) continue;

            // 골반 중앙(로컬 원점)
            const FVector2f Pelvis2D = (LHip + RHip) * 0.5f;

            // === 손 박스 표시: 손 사이 거리 < 0.5 * 어깨 길이일 때만 ===
            OnPoseBufferChanged.Broadcast(Pelvis2D, Pose, OwnerXform);

            // ---- 라인(뼈대) 그리기 ----
            for (int32 e = 0; e < NUM_EDGES; ++e)
            {
                const int32 A = EDGES[e][0];
                const int32 B = EDGES[e][1];
                if (!Pose.XY.IsValidIndex(A) || !Pose.XY.IsValidIndex(B)) continue;

                const FVector2f& Pa2D = Pose.XY[A];
                const FVector2f& Pb2D = Pose.XY[B];
                if (!(IsFinite2D(Pa2D) && IsFinite2D(Pb2D))) continue;

                const FVector La = MakeLocal(Pa2D, Pelvis2D);
                const FVector Lb = MakeLocal(Pb2D, Pelvis2D);

                const FVector Wa = OwnerXform.TransformPosition(La);
                const FVector Wb = OwnerXform.TransformPosition(Lb);

                DrawDebugLine(GetWorld(), Wa, Wb, LineColor, /*bPersistentLines*/false,
                    /*LifeTime*/0.f, /*DepthPriority*/0, LineThickness);
            }

            // ---- 포인트(관절) 그리기 ----
            for (int32 k = 0; k < 17; ++k)
            {
                const FVector2f& P2D = Pose.XY[k];
                if (!IsFinite2D(P2D)) continue;

                const FVector Lp = MakeLocal(P2D, Pelvis2D);
                const FVector Wp = OwnerXform.TransformPosition(Lp);

                DrawDebugSphere(GetWorld(), Wp, PointRadius, /*Segments*/8, PointColor,
                    /*bPersistentLines*/false, /*LifeTime*/0.f, /*DepthPriority*/0, /*Thickness*/0.5f);
            }
        }
    }
}

void UPoseUdpReceiverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CloseSocket();
    Super::EndPlay(EndPlayReason);
}

bool UPoseUdpReceiverComponent::InitSocket(int32 Port)
{
    CloseSocket();

    Socket = FUdpSocketBuilder(TEXT("PoseRecv"))
        .AsNonBlocking()
        .AsReusable()
        .BoundToEndpoint(FIPv4Endpoint(FIPv4Address::Any, Port))
        .WithReceiveBufferSize(1 << 20);

    return Socket != nullptr;
}

void UPoseUdpReceiverComponent::CloseSocket()
{
    if (Socket)
    {
        Socket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
        Socket = nullptr;
    }
}

bool UPoseUdpReceiverComponent::ReceiveOnce(TArray<FPersonPose>& OutPoses, uint64& OutTsMs)
{
    if (!Socket) return false;

    uint32 BytesPending = 0;
    if (!Socket->HasPendingData(BytesPending) || BytesPending < HEADER_SIZE) return false;

    RecvBuffer.SetNumUninitialized(BytesPending);
    int32 BytesRead = 0;
    if (!Socket->Recv(RecvBuffer.GetData(), RecvBuffer.Num(), BytesRead, ESocketReceiveFlags::None)) return false;
    if (BytesRead < HEADER_SIZE) return false;

    const uint8* p = RecvBuffer.GetData();
    auto readU32 = [&p]() { uint32 v = 0; FMemory::Memcpy(&v, p, 4); p += 4; return v; };
    auto readU16 = [&p]() { uint16 v = 0; FMemory::Memcpy(&v, p, 2); p += 2; return v; };
    auto readU8 = [&p]() {  uint8 v = 0; v = *p; p += 1; return v; };
    auto readU64 = [&p]() { uint64 v = 0; FMemory::Memcpy(&v, p, 8); p += 8; return v; };
    auto readF32 = [&p]() { float v = 0; FMemory::Memcpy(&v, p, 4); p += 4; return v; };

    // 헤더: "POSE"(LE), ver, resv, persons, ts_ms
    uint32 magic = readU32();
    if (magic != *((uint32*)"POSE")) return false;
    uint8 ver = readU8();
    uint8 resv = readU8();
    uint16 persons = readU16();
    OutTsMs = readU64();

    OutPoses.Reset();
    OutPoses.Reserve(persons);

    const uint8* end = RecvBuffer.GetData() + BytesRead;
    for (uint16 i = 0; i < persons; ++i)
    {
        if (p + EXPECTED_PERSON_BLOCK > end) break;

        FPersonPose pose;
        pose.PersonId = readU16();

        pose.XY.SetNumUninitialized(17);
        for (int k = 0; k < 17; ++k) {
            float x = readF32();
            float y = readF32();
            pose.XY[k] = FVector2f(x, y);
        }
        pose.Conf.SetNumUninitialized(17);
        for (int k = 0; k < 17; ++k) {
            pose.Conf[k] = readF32();
        }
        OutPoses.Add(MoveTemp(pose));
    }
    return OutPoses.Num() > 0;
}
