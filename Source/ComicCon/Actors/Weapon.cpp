// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/Weapon.h"
#include "Actors/MirroredPlayer.h"

AWeapon::AWeapon()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"));
	RootComponent = SceneComponent;

	WeaponMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	WeaponMeshComponent->SetupAttachment(RootComponent);
}

void AWeapon::SetOwningPlayer(AMirroredPlayer* InMirroredPlayer)
{
    OwningPlayer = InMirroredPlayer;
}

void AWeapon::BeginPlay()
{
	Super::BeginPlay();

	check(OwningPlayer);

    if (bAutoVisibilityFromOwner)
    {
        // �̺�Ʈ ���ε�
        OwningPlayer->OnCurrentWeaponChanged.AddDynamic(this, &AWeapon::OnOwnerWeaponChanged);

        // ���� ���� ��� �ݿ�
        OnOwnerWeaponChanged(OwningPlayer->GetCurrentWeapon());

    }
    else
    {
        // ���� �÷��̾ ������ �⺻������ ����
        HideWeapon();
    }
}

void AWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (OwningPlayer)
    {
        OwningPlayer->OnCurrentWeaponChanged.RemoveDynamic(this, &AWeapon::OnOwnerWeaponChanged);
    }
    Super::EndPlay(EndPlayReason);
}

void AWeapon::OnOwnerWeaponChanged(EWeaponState NewWeapon)
{
    if (!bAutoVisibilityFromOwner) return;

    const bool bShouldShow = (NewWeapon == VisibleWhenState);
    if (bShouldShow)
    {
        ShowWeapon();
    }
    else
    {
        HideWeapon();
    }
}

void AWeapon::ShowWeapon()
{
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
}

void AWeapon::HideWeapon()
{
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
}

void AWeapon::OverlapPlaneOnce(const FVector& Center, const FQuat& Rot, TSet<AActor*>& UniqueActors, float DebugLifeTime)
{
    const FVector HalfExtent(PlaneHalfThickness, PlaneHalfSize.X, PlaneHalfSize.Y);
    const FCollisionShape Shape = FCollisionShape::MakeBox(HalfExtent);

    const FVector X = Rot.RotateVector(FVector::XAxisVector);
    const FVector Y = Rot.RotateVector(FVector::YAxisVector);
    // Center�� ���� ��(-X face)�� �ǵ���, ���͸� +X * �ݵβ� ��ŭ �̵�
    const FVector CenterFromStartFace = Center; // + (X * HalfExtent.X); + (Y * HalfExtent.Y);

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SwingPlaneSweep), /*bTraceComplex*/ false, GetOwner());
    const bool bAny = GetWorld()->OverlapMultiByChannel(Overlaps, CenterFromStartFace, Rot, PlaneChannel, Shape, Params);

    if (bDrawDebugPlane)
    {
        DrawDebugBox(GetWorld(), CenterFromStartFace, HalfExtent, Rot, FColor::Cyan, false, DebugLifeTime, 0, 2.0f);

        // (����) ���� �� �ð�ȭ: Center ��ġ�� -X���� �߽��� ǥ��
        const FVector StartFaceCenter = Center; // ��û�� '���� ��' ��ġ
        DrawDebugPoint(GetWorld(), StartFaceCenter, 10.f, FColor::Yellow, false, DebugLifeTime);
        DrawDebugLine(GetWorld(), StartFaceCenter, StartFaceCenter + X * 50.f, FColor::Magenta, false, DebugLifeTime, 0, 2.f);
    }

    if (!bAny) return;

    for (const FOverlapResult& R : Overlaps)
    {
        if (AActor* A = R.GetActor())
        {
            if (A != GetOwner())
            {
                UniqueActors.Add(A);
            }
        }
    }
}
