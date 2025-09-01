// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ViewPlayer.generated.h"

UCLASS()
class COMICCON_API AViewPlayer : public APawn
{
	GENERATED_BODY()

public:
	AViewPlayer();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Follow", meta = (AllowPrivateAccess = "ture"))
    TObjectPtr<class AMirroredPlayer> MirroredPlayer;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Follow", meta = (AllowPrivateAccess = "ture"))
    TObjectPtr<class AMediaPlate> MediaPlate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Follow", meta = (AllowPrivateAccess = "ture"))
    TSubclassOf<class AMirroredPlayer> MirroredPlayerClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Follow", meta = (AllowPrivateAccess = "ture"))
    TSubclassOf<class AMediaPlate> MediaPlateClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Follow", meta = (AllowPrivateAccess = "ture"))
    TObjectPtr<class ASword> Sword;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Follow", meta = (AllowPrivateAccess = "ture"))
    TObjectPtr<class AAmulet> Amulet;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Follow", meta = (AllowPrivateAccess = "ture"))
    TSubclassOf<class ASword> SwordClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Follow", meta = (AllowPrivateAccess = "ture"))
    TSubclassOf<class AAmulet> AmuletClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Follow", meta = (AllowPrivateAccess = "ture"))
    float FollowLagSpeed = 12.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Follow", meta = (AllowPrivateAccess = "ture"))
    FVector FollowWorldOffset = FVector::ZeroVector;
};
