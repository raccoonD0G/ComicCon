// Fill out your copyright notice in the Description page of Project Settings.


#include "Pawns/ViewPlayer.h"
#include "Actors/MirroredPlayer.h"
#include "Components/SingleSwingClassifierComponent.h"
#include "Actors/Sword.h"
#include "Actors/Amulet.h"
#include "MediaPlate.h"

AViewPlayer::AViewPlayer()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AViewPlayer::BeginPlay()
{
    Super::BeginPlay();
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    check(MirroredPlayerClass);
    check(SwordClass);
    check(AmuletClass);

    FTransform MirroredPlayerTF = FTransform(FRotator(), FVector(500, 0, -300), FVector(1, 1, 1));
    MirroredPlayer = GetWorld()->SpawnActor<AMirroredPlayer>(MirroredPlayerClass, MirroredPlayerTF, Params);

    FTransform SwordTF = FTransform();
    Sword = GetWorld()->SpawnActorDeferred<ASword>(SwordClass, MirroredPlayerTF);
    Sword->SetOwningPlayer(MirroredPlayer);
	Sword->SetOwingSwingClassifierComponent(MirroredPlayer->GetSwingClassifierComponent());
    Sword->FinishSpawning(SwordTF);

    FTransform AmuletTF = FTransform();
    AmuletLeft = GetWorld()->SpawnActorDeferred<AAmulet>(AmuletClass, MirroredPlayerTF);
    AmuletLeft->SetOwningPlayer(MirroredPlayer);
	AmuletLeft->SetOwingSingleSwingClassifierComponent(MirroredPlayer->GetLeftSingleSwingClassifierComponent());
    AmuletLeft->FinishSpawning(AmuletTF);

	AmuletRight = GetWorld()->SpawnActorDeferred<AAmulet>(AmuletClass, MirroredPlayerTF);
	AmuletRight->SetOwningPlayer(MirroredPlayer);
	AmuletRight->SetOwingSingleSwingClassifierComponent(MirroredPlayer->GetRightSingleSwingClassifierComponent());
	AmuletRight->FinishSpawning(AmuletTF);
}

void AViewPlayer::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!MirroredPlayer) return;

    UPoseClassifierComponent* Cls = MirroredPlayer->GetPoseClassifier();
    if (!Cls) return;

    FVector ShoulderMid;
    if (!Cls->GetShoulderMidWorld(ShoulderMid)) return;

    const FVector TargetLoc = ShoulderMid + FollowWorldOffset;

    if (FollowLagSpeed <= KINDA_SMALL_NUMBER)
    {
        SetActorLocation(TargetLoc);
    }
    else
    {
        SetActorLocation(FMath::VInterpTo(GetActorLocation(), TargetLoc, DeltaSeconds, FollowLagSpeed));
    }
}