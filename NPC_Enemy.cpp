// Fill out your copyright notice in the Description page of Project Settings.


#include "NPC/NPC_Enemy.h"
#include "Perception/PawnSensingComponent.h"
#include "Character/PC_Character.h"
#include "Components/SphereComponent.h"
#include "AIController.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Weapon/MelleWeapon.h"
#include "World/HON_GameInstance.h"
#include "World/ObjectLoader.h"
#include "Components/BoxComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "../HONGameMode.h"
#include "World/Globals.h"
#include "NPC/NPC_Companion.h"

ANPC_Enemy::ANPC_Enemy()
{
	m_MelleWeapon = CreateDefaultSubobject<AMelleWeapon>(TEXT("BaseMelleWeapon"));

	m_PawnSensor->SetComponentTickEnabled(true);

	m_PawnSensor->HearingThreshold = 1750;
	m_PawnSensor->LOSHearingThreshold = 3500;
	m_PawnSensor->SightRadius = 3360;
	m_PawnSensor->SensingInterval = 0.5;
	m_PawnSensor->HearingMaxSoundAge = 1;
	m_PawnSensor->SetPeripheralVisionAngle(25.0f);
	m_PawnSensor->bEnableSensingUpdates = true;
	m_PawnSensor->bTickInEditor = true;
	m_PawnSensor->SetComponentTickInterval(.0f);
	m_PawnSensor->bOnlySensePlayers = false;
	m_PawnSensor->bSeePawns = true;
	m_PawnSensor->bHearNoises = true;
	m_PawnSensor->bAutoActivate = true;
	m_PawnSensor->bAutoRegister = true;


	bIsAttacking = false;
	bIsDead = false;
	bWeaponIsEquipped = false;

	this->AutoPossessAI = EAutoPossessAI::Spawned;
	this->AutoPossessPlayer = EAutoReceiveInput::Disabled;

}

// Called when the game starts or when spawned
void ANPC_Enemy::BeginPlay()
{
	Super::BeginPlay();

	GM = Cast<AHONGameMode>(GetWorld()->GetAuthGameMode());
	Global = GM->Globals;

	m_PawnSensor->OnSeePawn.AddDynamic(this, &ANPC_Enemy::OnPawnSeen);
	m_PawnSensor->OnHearNoise.AddDynamic(this, &ANPC_Enemy::OnNoiseHeard);

	m_CombatSphere->OnComponentBeginOverlap.AddDynamic(this, &ANPC_Enemy::CombatSphereOnOverlapBegin);
	m_CombatSphere->OnComponentEndOverlap.AddDynamic(this, &ANPC_Enemy::CombatSphereOnOverlapEnd);

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	m_MelleWeapon = GetWorld()->SpawnActor<AMelleWeapon>(FVector(0, 0, 0), FRotator(0.0f, 0.0f, 0.0f), SpawnInfo);
	m_MelleWeapon->OwnerID = this->GetUniqueID();
	m_MelleWeapon->SetOwner(this);

}

// Called every frame
void ANPC_Enemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector Speed = this->GetVelocity();
	FVector LateralSpeed = FVector(Speed.X, Speed.Y, 0.f);
	MovementSpeed = LateralSpeed.Size();

	if (MovementSpeed > 150.f) {
		bIsAtRunSpeed = true;
	}
	else {
		bIsAtRunSpeed = false;
	}

	m_PawnSensor->ReceiveTick(DeltaTime);
}


// Called to bind functionality to input
void ANPC_Enemy::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void ANPC_Enemy::OnPawnSeen(APawn* SeenPawn)
{
	if (SeenPawn == nullptr) {
		return;
	}

	if (SeenPawn->IsA(APC_Character::StaticClass())) {
		APC_Character* Char = Cast<APC_Character>(SeenPawn);

		MoveToTarget(Char);

	}

	if (SeenPawn->IsA(ANPC_Companion::StaticClass())) {
		ANPC_Companion* Comp = Cast<ANPC_Companion>(SeenPawn);

		//Sees enemy
		if (Comp) {
			MoveToTarget(Comp);
		}
	}
}

void ANPC_Enemy::OnNoiseHeard(APawn* p_Instigator, const FVector& Location, float Volume)
{

	if (p_Instigator == nullptr) {
		return;
	}

	if (p_Instigator->IsA(APC_Character::StaticClass())) {
		APC_Character* Char = Cast<APC_Character>(p_Instigator);

		FVector Direction = Location - GetActorLocation();
		Direction.Normalize();

		FRotator NewLookAt = FRotationMatrix::MakeFromX(Direction).Rotator();
		NewLookAt.Pitch = 0.0f;
		NewLookAt.Roll = 0.0f;

		SetActorRotation(NewLookAt);

		if (Char) {
			MoveToTarget(Char);
		}
	}

	if (p_Instigator->IsA(ANPC_Companion::StaticClass())) {
		ANPC_Companion* Comp = Cast<ANPC_Companion>(p_Instigator);

		FVector Direction = Location - GetActorLocation();
		Direction.Normalize();

		FRotator NewLookAt = FRotationMatrix::MakeFromX(Direction).Rotator();
		NewLookAt.Pitch = 0.0f;
		NewLookAt.Roll = 0.0f;

		SetActorRotation(NewLookAt);

		if (Comp) {
			MoveToTarget(Comp);
		}
	}

}

void ANPC_Enemy::CombatSphereOnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {

	if (OtherActor->IsA(ANPC_Companion::StaticClass())) {
		ANPC_Companion* Comp = Cast<ANPC_Companion>(OtherActor);

		FVector Direction = OtherActor->GetActorLocation() - GetActorLocation();
		Direction.Normalize();

		FRotator NewLookAt = FRotationMatrix::MakeFromX(Direction).Rotator();
		NewLookAt.Pitch = 0.0f;
		NewLookAt.Roll = 0.0f;

		if (Comp) {
			SetActorRotation(NewLookAt);
			bIsAttacking = true;
			bOverlappingCombatSphere = true;
		}
	}
	if (OtherActor->IsA(APC_Character::StaticClass())) {
		APC_Character* Char = Cast<APC_Character>(OtherActor);

		FVector Direction = OtherActor->GetActorLocation() - GetActorLocation();
		Direction.Normalize();

		FRotator NewLookAt = FRotationMatrix::MakeFromX(Direction).Rotator();
		NewLookAt.Pitch = 0.0f;
		NewLookAt.Roll = 0.0f;

		if (Char) {
			SetActorRotation(NewLookAt);
			bIsAttacking = true;
			CombatTarget = Char;
			bOverlappingCombatSphere = true;
		}
	}

}

void ANPC_Enemy::CombatSphereOnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {
	
	if (OtherActor->IsA(ANPC_Companion::StaticClass())) {
		ANPC_Companion* Comp = Cast<ANPC_Companion>(OtherActor);

		if (Comp) {
			bIsAttacking = false;
			bOverlappingCombatSphere = false;
			MoveToTarget(Comp);
			CombatTarget = nullptr;
		}
	}
	if (OtherActor->IsA(APC_Character::StaticClass())) {
		APC_Character* Char = Cast<APC_Character>(OtherActor);

		if (Char) {
			bIsAttacking = false;
			bOverlappingCombatSphere = false;
			MoveToTarget(Char);
			CombatTarget = nullptr;
		}
	}

}

void ANPC_Enemy::HitByAttack(int dmg)
{
	m_Health -= dmg;

	if (m_Health <= 0) {
		//Dead
		Destroy();
	}
}

void ANPC_Enemy::EquipWeapon()
{

	AMelleWeapon* Ret = Global->GetObjectLoader()->GetWeapon("Unarmed");
	m_MelleWeapon->m_SkeletalMeshComp->SetSkeletalMesh(Ret->m_SkeletalMeshComp->SkeletalMesh, true);
	m_MelleWeapon->m_CombatCollision->SetRelativeLocation(Ret->m_CombatCollision->GetRelativeLocation());
	m_MelleWeapon->m_CombatCollision->SetRelativeScale3D(Ret->m_CombatCollision->GetRelativeScale3D());
	m_MelleWeapon->m_WeaponName = Ret->m_WeaponName;
	m_MelleWeapon->m_WeaponDamage = Ret->m_WeaponDamage;
	m_MelleWeapon->m_WeaponType = Ret->m_WeaponType;

	m_MelleWeapon->m_SkeletalMeshComp->SetHiddenInGame(false);
	RHSocket->AttachActor(m_MelleWeapon, this->GetMesh());

}

void ANPC_Enemy::MoveToTarget(APawn* Target) {


	if (AIController) {
		/*
		FAIMoveRequest MoveRequest;
		MoveRequest.SetGoalActor(Target);
		MoveRequest.SetAcceptanceRadius(5.0);

		FNavPathSharedPtr NavPath;

		AIController->MoveTo(MoveRequest, &NavPath);*/
		UAIBlueprintHelperLibrary::SimpleMoveToActor(AIController, Target);
	}
}

