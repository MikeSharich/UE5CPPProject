// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/PC_Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Character/MainPlayerController.h"
#include "Weapon/Weapon.h"
#include "Weapon/MelleWeapon.h"
#include "Weapon/RangedWeapon.h"
#include "World/Globals.h"
#include "../HONGameMode.h"
#include "NPC/NPC_Companion.h"
#include "World/ObjectLoader.h"
#include "Character/Abilities.h"
#include "Components/BoxComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimBlueprint.h"
#include "Weapon/Projectile.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include "HUD/MainHUD.h"
#include "World/SQLiteDB.h"

// Sets default values
APC_Character::APC_Character()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	GM = CreateDefaultSubobject<AHONGameMode>(TEXT("HONGameMode"));
	PlayerID = 1;
	
	if(!GM->bUseSQL) {
		static ConstructorHelpers::FObjectFinder<UDataTable> CharacterTableObject(TEXT("DataTable'/Game/Characters/CharacterDataCsv.CharacterDataCsv'"));
		if (CharacterTableObject.Succeeded())
		{
			DataTable = CharacterTableObject.Object;

			FName RowName = FName("Paladin");
			FString ContextString = TEXT("Debug: ");

			FChar_DT* CharData = DataTable->FindRow<FChar_DT>(RowName, ContextString, true);

			if (CharData) {
				this->m_Player_Name = CharData->data_Player_Name;
				this->GetMesh()->SetSkeletalMesh(CharData->data_SkeletalMesh);
				this->GetMesh()->SetRelativeLocationAndRotation(FVector(0, 0, -85.85), FRotator(0.0, -90.0, 0.0), false, nullptr, ETeleportType::None);
				this->m_AnimBP = CharData->data_AnimBP;
				this->GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
				this->GetMesh()->SetAnimClass(this->m_AnimBP->GeneratedClass);
				this->m_Health = CharData->data_Health;
				this->m_MaxHealth = CharData->data_MaxHealth;
				this->m_TargetType = CharData->data_TargetType;
			}
		}
	}
	
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 450.f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->SetRelativeLocation(FVector(0, 0, -50), false, nullptr, ETeleportType::None);
	
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->SetRelativeLocation(FVector(-550.f, 0.f, 180.f));
	FollowCamera->SetWorldRotation(FRotator(0.f, 0.f, 0.f), false, nullptr, ETeleportType::None);
	FollowCamera->bUsePawnControlRotation = false;
		

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 650.f;
	GetCharacterMovement()->AirControl = 0.8f;

	//Init Weapon
	m_Weapon = CreateDefaultSubobject<AWeapon>(TEXT("BaseWeapon"));
	m_RangedWeapon = CreateDefaultSubobject<ARangedWeapon>(TEXT("BaseRangedWeapon"));
	m_MelleWeapon = CreateDefaultSubobject<AMelleWeapon>(TEXT("BaseMelleWeapon"));
	Abilities = CreateDefaultSubobject<UAbilities>(TEXT("Abilities"));

	// Set our turn rates for input
	BaseTurnRate = 65.f;
	BaseLookUpAtRate = 65.f;

	MovementSpeed = 0.f;
	bIsAtRunSpeed = false;
	bLMBDownSelect = false;
	bSelectCombatMode = false;

	//Interface
	bShowInventory = false;
	bShowEquipmentView = false;

	//Combat
	bIsAttacking = false;
	bWeaponEquipped = false;
	LMBTimerSet = false;
	m_ProjFired = false;
	m_RangedWepDrawn = false;

	CompanionAmt = 0;

	m_EquippedWeaponType = "None";
	m_LMBReleased = false;

	CurCameraStatus = ECameraStatus::ECS_Select;


}

void APC_Character::HitByAttack(int dmg)
{

}

// Called when the game starts or when spawned
void APC_Character::BeginPlay()
{
	Super::BeginPlay();

	Global = GM->Globals;

	Database = new SQLiteDB(TEXT("C:\\Unreal Projects\\HON\\Database\\HON_DB.sqlite"), ESQLiteDatabaseOpenMode::ReadWrite);

	Database->LoadCharacter(PlayerID, this);

	FVector Position = Database->LoadPlayerPosition(PlayerID);
	LastSave = 0;

	if (Position != FVector(0.0f, 0.0f, 0.0f)) {
		GetController()->GetPawn()->TeleportTo(Position, GetController()->GetPawn()->GetActorRotation(), false, true);
	}


	Abilities->SetUsingChar(this);
	Abilities->BindAbility(1, "SummonWolf");
	Abilities->BindAbility(2, "Teleport");
	Abilities->BindAbility(3, "Charge");
	
	//CameraBoom->bUsePawnControlRotation = true;
	FollowCamera->bUsePawnControlRotation = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	MPC = Cast<AMainPlayerController>(GetController());
	MPC->bShowMouseCursor = true;

	MainHUD = Cast<AMainHUD>(MPC->GetHUD());

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	m_MelleWeapon = GetWorld()->SpawnActor<AMelleWeapon>(FVector(0, 0, 0), FRotator(0.0f, 0.0f, 0.0f), SpawnInfo);
	m_MelleWeapon->OwnerID = this->GetUniqueID();
	m_MelleWeapon->SetOwner(this);

	m_RangedWeapon = GetWorld()->SpawnActor<ARangedWeapon>(this->GetActorLocation(), this->GetActorRotation(), SpawnInfo);
	m_RangedWeapon->OwnerID = this->GetUniqueID();
	m_RangedWeapon->SetOwner(this);

	RHSocket = this->GetMesh()->GetSocketByName("RightHandSocket");
	LHSocket = this->GetMesh()->GetSocketByName("LeftHandSocket");
	
}

void APC_Character::EndPlay(const EEndPlayReason::Type type)
{
	delete Database;
}


// Called every frame
void APC_Character::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector Speed = this->GetVelocity();
	FVector LateralSpeed = FVector(Speed.X, Speed.Y, 0.f);
	MovementSpeed = LateralSpeed.Size();

	FRotator ActorRotation = GetActorRotation();
	ActorRotation.Yaw += CameraInput.X;
	SetActorRotation(ActorRotation);

	FRotator NewRotation = CameraBoom->GetComponentRotation();
	NewRotation.Pitch = FMath::Clamp(NewRotation.Pitch + CameraInput.Y, -80.0f, -15.0f);
	CameraBoom->SetWorldRotation(NewRotation);

	m_CamLoc = FollowCamera->GetComponentLocation();
	m_CamRot = FollowCamera->GetRelativeRotation();

	LastSave += DeltaTime;

	if (LastSave > 4.0f) {
		LastSave = 0;
		Database->SavePlayerPosition(PlayerID, GetController()->GetPawn()->GetActorLocation());
	}

	if (MovementSpeed > 150.f) {
		bIsAtRunSpeed = true;
	}
	else {
		bIsAtRunSpeed = false;
	}

	if (CurCameraStatus == ECameraStatus::ECS_Select) {
		FVector CurPos = this->GetRootComponent()->GetRelativeLocation();

		FRotator CurRot = this->GetRootComponent()->GetRelativeRotation();
		FQuat QCurRot = UKismetMathLibrary::Conv_RotatorToQuaternion(CurRot);

		FVector FrontOfActor = FVector(140, 0, 50);

		FVector NewPos = CurPos + UKismetMathLibrary::Quat_RotateVector(QCurRot, FrontOfActor);

		m_LookAtLocation = NewPos;

		//DrawDebugSphere(this->GetWorld(), NewPos, 32, 8, FColor::Black, false, .9, 0U, 5);

	}

	if (CurCameraStatus == ECameraStatus::ECS_Combat) {
		MPC->SetMouseLocation(550, 500);
	}

	if (bLMBDownSelect) {
		MPC->SetMouseLocation(LMBMouseX, LMBMouseY);
	}

	if (CurCameraStatus == ECameraStatus::ECS_Combat) {

		FVector CurPos = this->GetRootComponent()->GetRelativeLocation();

		FRotator CurRot = this->GetRootComponent()->GetRelativeRotation();
		FQuat QCurRot = UKismetMathLibrary::Conv_RotatorToQuaternion(CurRot);

		FVector FrontOfActor = FVector(140, 90, 50);

		FVector NewPos = CurPos + UKismetMathLibrary::Quat_RotateVector(QCurRot, FrontOfActor);

		m_CamLoc = FollowCamera->GetComponentLocation();
		//UE_LOG(LogTemp, Warning, TEXT("X : %f  - Y : %f  - Z : %f"), m_CamLoc.X, m_CamLoc.Y, m_CamLoc.Z);


		m_CamRot = FollowCamera->GetComponentRotation();
		//UE_LOG(LogTemp, Warning, TEXT("P : %f  - Y : %f  - R : %f"), m_CamRot.Pitch, m_CamRot.Yaw, m_CamRot.Roll);

		//Looking down
		if (m_CamRot.Pitch <= 0) {

			//Looked Down more - -40 fully looking down
			float ZChange = m_CamRot.Pitch * 2;

			NewPos.Z += ZChange;
			
		}

		//Looking Up
		else if (m_CamRot.Pitch >= 0) {

			//Looked Down more - 15 fully looking down
			float ZChange = m_CamRot.Pitch * 2;

			NewPos.Z += ZChange;

		}

		m_LookAtLocation = NewPos;

		//DrawDebugSphere(this->GetWorld(), m_LookAtLocation, 32, 8, FColor::Black, false, .9, 0U, 5);
		
		//UE_LOG(LogTemp, Warning, TEXT("Pitch : %f  - LookAtZ : %f  - OldPitch: %f"), m_CamRot.Pitch, m_LookAtLocation.Z, m_OldPitch);


	}

	if (bIsCharging) {
		bIsCharging = false;
	}

	m_OldPitch = m_CamRot.Pitch;

	m_MelleWeapon->UpdateComponentTransforms();
}

// Called to bind functionality to input
void APC_Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	//Player Actions
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAction("LMB", IE_Pressed, this, &APC_Character::LMB);
	PlayerInputComponent->BindAction("LMB", IE_Released, this, &APC_Character::LMBRelease);

	//Sword
	PlayerInputComponent->BindAction("Equip", IE_Pressed, this, &APC_Character::EquipWeapon);

	PlayerInputComponent->BindAction("NumOne", IE_Pressed, this, &APC_Character::NumOnePressed);
	PlayerInputComponent->BindAction("NumTwo", IE_Pressed, this, &APC_Character::NumTwoPressed);
	PlayerInputComponent->BindAction("NumThree", IE_Pressed, this, &APC_Character::NumThreePressed);

	//Interface
	PlayerInputComponent->BindAction("Inventory", IE_Pressed, this, &APC_Character::ToggleShowInventory);
	PlayerInputComponent->BindAction("EquipmentView", IE_Pressed, this, &APC_Character::ToggleShowEquipmentView);

	PlayerInputComponent->BindAction("ChangeView", IE_Pressed, this, &APC_Character::ChangeCameraView);

	//Player Movement
	PlayerInputComponent->BindAxis("MoveForward", this, &APC_Character::MoveForward);
	PlayerInputComponent->BindAxis("MoveBackward", this, &APC_Character::MoveBackward);

	PlayerInputComponent->BindAxis("MoveRight", this, &APC_Character::MoveRight);
	PlayerInputComponent->BindAxis("MoveLeft", this, &APC_Character::MoveLeft);

	PlayerInputComponent->BindAxis("Turn", this, &APC_Character::PlayerAddControllerYawInput);
	PlayerInputComponent->BindAxis("LookUp", this, &APC_Character::PlayerAddControllerPitchInput);
}

void APC_Character::LMB() {
	if (CurCameraStatus == ECameraStatus::ECS_Combat) {
		//Attack
		if (!LMBTimerSet) {
			Attack();
		}
		m_LMBReleased = false;
		m_RangedWepDrawn = false;
		m_ProjFired = false;

		if (m_EquippedWeaponType.Equals("Ranged") && !LMBTimerSet) {
			GetWorldTimerManager().SetTimer(handle, this, &APC_Character::LMBTimer, 0.6f, true, 0.5f);
			LMBTimerSet = true;
		}
	}
	if (CurCameraStatus == ECameraStatus::ECS_Select) {
		bLMBDownSelect = true;
		MPC->GetMousePosition(LMBMouseX, LMBMouseY);
	}

}

void APC_Character::LMBRelease() {
	if (CurCameraStatus == ECameraStatus::ECS_Combat) {
		
		if (m_EquippedWeaponType.Equals("Ranged")) {
			m_LMBReleased = true;
		}

	}

	if (CurCameraStatus == ECameraStatus::ECS_Select) {
		MPC->bShowMouseCursor = true;
		bLMBDownSelect = false;
		MPC->SetMouseLocation(LMBMouseX, LMBMouseY);
	}

}

void APC_Character::LMBTimer()
{
	m_RangedWepDrawn = true;

	if (m_LMBReleased) {
		
		FVector CameraLocation;
		FRotator CameraRotation;
		GetActorEyesViewPoint(CameraLocation, CameraRotation);

		MuzzleOffset.Set(70.0f, 0.0f, 0.0f);

		// Transform MuzzleOffset from camera space to world space.
		FVector MuzzleLocation = CameraLocation + FTransform(CameraRotation).TransformVector(MuzzleOffset);

		FRotator MuzzleRotation = CameraRotation;
		m_Projectile->DetachRootComponentFromParent(true);

		m_Projectile->LaunchDirection = MuzzleRotation.Vector();

		DrawDebugLine(GetWorld(), CameraLocation, MuzzleLocation, FColor::White, false, 1.0f, 0, 1.0f);

		m_Projectile->FireInDirection();
		FRotator ArrowRot = CameraRotation;
		ArrowRot.Yaw -= 90;
		m_Projectile->m_SkeletalMeshComp->SetRelativeRotation(ArrowRot, false, nullptr, ETeleportType::None);

		m_ProjFired = true;
		
		GetWorldTimerManager().ClearTimer(handle);
		LMBTimerSet = false;

	}
}

void APC_Character::PlayerAddControllerPitchInput(float Val)
{

	CameraInput.Y = Val;

	if (CurCameraStatus == ECameraStatus::ECS_Combat || bLMBDownSelect == true) {
		if (Val != 0.f && Controller && Controller->IsLocalPlayerController())
		{
			APlayerController* const PC = CastChecked<APlayerController>(Controller);
			PC->AddPitchInput(Val);
		}
	}

}

void APC_Character::PlayerAddControllerYawInput(float Val) {

	CameraInput.X = Val;

	if (CurCameraStatus == ECameraStatus::ECS_Combat || bLMBDownSelect == true) {
		if (Val != 0.f && Controller && Controller->IsLocalPlayerController())
		{
			APlayerController* const PC = CastChecked<APlayerController>(Controller);
			PC->AddYawInput(Val);
		}
	}
}

void APC_Character::MoveForward(float Value) {
	if ((Controller != nullptr) && (Value != 0.0f)) {

		// Find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		AddMovementInput(Direction, Value);

	}
}

void APC_Character::MoveBackward(float Value) {
	if ((Controller != nullptr) && (Value != 0.0f)) {
		Value = -Value;
		// Find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		AddMovementInput(Direction, Value);
	}
}

void APC_Character::MoveRight(float Value) {
	if ((Controller != nullptr) && (Value != 0.0f)) {

		// Find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(Direction, Value);
	}
}

void APC_Character::MoveLeft(float Value) {
	if ((Controller != nullptr) && (Value != 0.0f)) {
		Value = -Value;
		// Find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(Direction, Value);
	}
}

void APC_Character::EquipWeapon() {

	if (bWeaponEquipped == false) {
		bWeaponEquipped = true;
		bSelectCombatMode = true;

		ARangedWeapon* Ret = Global->GetObjectLoader()->GetRangedWeapon("ShenMin_Bow");
		m_RangedWeapon->m_SkeletalMeshComp->SetSkeletalMesh(Ret->m_SkeletalMeshComp->SkeletalMesh, true);

		m_RangedWeapon->m_WeaponName = Ret->m_WeaponName;
		m_RangedWeapon->m_WeaponDamage = Ret->m_WeaponDamage;
		m_RangedWeapon->m_WeaponType = Ret->m_WeaponType;

		m_RangedWeapon->m_SkeletalMeshComp->SetHiddenInGame(false);
		RHSocket->AttachActor(m_RangedWeapon, this->GetMesh());
		m_RangedWeapon->m_SkeletalMeshComp->SetRelativeScale3D(FVector(1.2, 1.2, 1.2));
		m_RangedWeapon->m_SkeletalMeshComp->SetRelativeLocationAndRotation(FVector(14, 7.1, -58.6), FRotator(0, 210, 0), false, nullptr, ETeleportType::None);

		m_EquippedWeaponType = "Ranged";

		/*
		//m_MelleWeapon->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform, NAME_None);
		AMelleWeapon* Ret = Global->GetObjectLoader()->GetWeapon("DragonSword");
		m_MelleWeapon->m_SkeletalMeshComp->SetSkeletalMesh(Ret->m_SkeletalMeshComp->SkeletalMesh, true);
		m_MelleWeapon->m_CombatCollision->SetRelativeLocation(Ret->m_CombatCollision->GetRelativeLocation());
		m_MelleWeapon->m_CombatCollision->SetRelativeScale3D(Ret->m_CombatCollision->GetRelativeScale3D());
		m_MelleWeapon->m_WeaponName = Ret->m_WeaponName;
		m_MelleWeapon->m_WeaponDamage = Ret->m_WeaponDamage;
		m_MelleWeapon->m_WeaponType = Ret->m_WeaponType;

		m_MelleWeapon->m_SkeletalMeshComp->SetHiddenInGame(false);
		RHSocket->AttachActor(m_MelleWeapon, this->GetMesh());

		m_EquippedWeaponType = "Melle";
		*/
	}
	else if (bWeaponEquipped == true) {
		bWeaponEquipped = false;
		bSelectCombatMode = false;
		
		m_RangedWeapon->m_SkeletalMeshComp->SetHiddenInGame(true);
		//m_MelleWeapon->m_SkeletalMeshComp->SetHiddenInGame(true);
	}
}

void APC_Character::Attack() {

	if (m_EquippedWeaponType.Equals("Ranged")) {
		/*
		FVector CameraLocation;
		FRotator CameraRotation;
		GetActorEyesViewPoint(CameraLocation, CameraRotation);

		// Set MuzzleOffset to spawn projectiles slightly in front of the camera.
		MuzzleOffset.Set(70.0f, 0.0f, 0.0f);

		// Transform MuzzleOffset from camera space to world space.
		FVector BowHolster = CameraLocation + FTransform(CameraRotation).TransformVector(MuzzleOffset);

		// Skew the aim to be slightly upwards.
		FRotator BowRotation = CameraRotation;
		*/


		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		m_Projectile = GetWorld()->SpawnActor<AProjectile>(LHSocket->GetSocketLocation(this->GetMesh()), this->GetActorRotation(), SpawnParams);
		m_Projectile->OwnerID = this->GetUniqueID();
		m_Projectile->SetOwner(this);

		AProjectile* Ret = Global->GetObjectLoader()->GetProjectile("ShenMin_Arrow");
		m_Projectile->m_SkeletalMeshComp->SetSkeletalMesh(Ret->m_SkeletalMeshComp->SkeletalMesh, true);

		m_Projectile->m_WeaponName = Ret->m_WeaponName;
		m_Projectile->m_WeaponDamage = Ret->m_WeaponDamage;
		m_Projectile->m_WeaponType = Ret->m_WeaponType;

		m_Projectile->m_SkeletalMeshComp->SetHiddenInGame(false);
		LHSocket->AttachActor(m_Projectile, this->GetMesh());

		m_Projectile->m_SkeletalMeshComp->SetRelativeScale3D(FVector(1.6, 1.6, 1.6));
	
	}

	bIsAttacking = true;
}

void APC_Character::ChangeCameraView() {
	FTransform PlayerLoc = this->GetActorTransform();
	FVector NewCamLoc = FVector(0.f, 0.f, 0.f);
	if (CurCameraStatus == ECameraStatus::ECS_Select) {
		CurCameraStatus = ECameraStatus::ECS_Combat;

		MPC->bShowMouseCursor = false;
		MPC->SetIgnoreLookInput(false);

		//Set Camera and player to Combat mode
		CameraBoom->TargetArmLength = 150.f;
		//CameraBoom->SetRelativeLocation(FVector(CameraBoom->GetRelativeLocation().X, 0, -70.0f), false, nullptr, ETeleportType::None);

		//CameraBoom->bUsePawnControlRotation = true;
		FollowCamera->bUsePawnControlRotation = false;
		GetCharacterMovement()->bOrientRotationToMovement = true;
		this->GetMesh()->SetScalarParameterValueOnMaterials("Opacity", 0.2);

		FollowCamera->SetRelativeRotation(FRotator(0, FollowCamera->GetRelativeRotation().Yaw, FollowCamera->GetRelativeRotation().Roll));
	}
	else {
		CurCameraStatus = ECameraStatus::ECS_Select;

		//Set Camera and player to Combat mode
		CameraBoom->TargetArmLength = 450.f;
		//CameraBoom->SetRelativeLocation(FVector(CameraBoom->GetRelativeLocation().X, 0, 0), false, nullptr, ETeleportType::None);

		//CameraBoom->bUsePawnControlRotation = true;
		FollowCamera->bUsePawnControlRotation = false;
		GetCharacterMovement()->bOrientRotationToMovement = true;

		this->GetMesh()->SetScalarParameterValueOnMaterials("Opacity", 1.0);
	}
	MPC->Update();
}

void APC_Character::NumOnePressed() {
	Abilities->UseAbility(1);
	
}

void APC_Character::NumTwoPressed()
{
	Abilities->UseAbility(2);
}

void APC_Character::NumThreePressed()
{
	Abilities->UseAbility(3);
}

void APC_Character::Charge() {

	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

	const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

	AddMovementInput(Direction, 3);
}

void APC_Character::ToggleShowInventory() {
	if (bShowInventory == false) {
		bShowInventory = true;
	}
	else {
		bShowInventory = false;
	}
	MainHUD->Update();
}

void APC_Character::ToggleShowEquipmentView() {
	if (bShowEquipmentView == false) {
		bShowEquipmentView = true;
	}
	else {
		bShowEquipmentView = false;
	}
	MainHUD->Update();
}

APC_Character* APC_Character::GetCharacter()
{
	return this;
}