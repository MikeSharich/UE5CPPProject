// ©Michael Sharich©


#include "World/SQLiteDB.h"
#include "Character/PC_Character.h"
#include "Animation/AnimBlueprint.h"

SQLiteDB::SQLiteDB(FString Path, ESQLiteDatabaseOpenMode OpenMode)
{
	//ESQLite DatabaseOpenMode::ReadOnly
	//ESQLite DatabaseOpenMode::ReadWrite
	//ESQLite DatabaseOpenMode::ReadWriteCache (Only one connection can write and create a new tables to the database)

	Database = new FSQLiteDatabase();

	if (!Database->Open(*Path, OpenMode) || !Database->IsValid()) {
		UE_LOG(LogTemp, Warning, TEXT("Failed to open database: %s"), *Database->GetLastError());
	}

	//? (index) e.g. select * from people where name = '?'

	const TCHAR* SaveQuery = TEXT("replace into players (ID, x, y, z) values ($ID, $x, $y, $z)");
	SaveStatement.Create(*Database, SaveQuery, ESQLitePreparedStatementFlags::Persistent);

	const TCHAR* LoadQuery = TEXT("select * from players where ID = $ID limit 1");
	LoadStatement.Create(*Database, LoadQuery, ESQLitePreparedStatementFlags::Persistent);

	const TCHAR* LoadCharacter = TEXT("select * from activeplayers where PlayerID = $PlayerID limit 1");
	CharLS.Create(*Database, LoadCharacter, ESQLitePreparedStatementFlags::Persistent);

}


bool SQLiteDB::SavePlayerPosition(int32 PlayerID, FVector Position)
{
	if (Database->IsValid() && SaveStatement.IsValid()) {
		SaveStatement.Reset();

		bool bBindingSuccess = true;
		bBindingSuccess = bBindingSuccess && SaveStatement.SetBindingValueByName(TEXT("$ID"), PlayerID);
		bBindingSuccess = bBindingSuccess && SaveStatement.SetBindingValueByName(TEXT("$x"), Position.X);
		bBindingSuccess = bBindingSuccess && SaveStatement.SetBindingValueByName(TEXT("$y"), Position.Y);
		bBindingSuccess = bBindingSuccess && SaveStatement.SetBindingValueByName(TEXT("$z"), Position.Z);

		if (!bBindingSuccess || SaveStatement.Execute()) {
			return false;
		}
	}

	return false;
}

void SQLiteDB::LoadCharacter(int32 PlayerID, class APC_Character* InChar)
{
	if (Database->IsValid() && CharLS.IsValid()) {
		CharLS.Reset();

		if (CharLS.SetBindingValueByName(TEXT("$PlayerID"), PlayerID) && CharLS.Execute() && LoadStatement.Step() == ESQLitePreparedStatementStepResult::Row) {

			FString CharName;
			FString SkeletalMeshString;
			FSoftObjectPath SkeleMeshObjectPath;
			FString AnimBPString;
			FSoftObjectPath AnimBPObjectPath;

			CharLS.GetColumnValueByName(TEXT("PlayerName"), CharName);
			InChar->m_Player_Name = CharName;
			//Load Mesh
			CharLS.GetColumnValueByName(TEXT("SkeletalMesh"), SkeletalMeshString);
			SkeleMeshObjectPath = SkeletalMeshString;
			InChar->GetMesh()->SetSkeletalMesh(Cast<USkeletalMesh>(SkeleMeshObjectPath.ResolveObject()));
			if (InChar->GetMesh()->SkeletalMesh == nullptr) {
				InChar->GetMesh()->SetSkeletalMesh(Cast<USkeletalMesh>(SkeleMeshObjectPath.TryLoad()));
			}
			InChar->GetMesh()->SetRelativeLocationAndRotation(FVector(0, 0, -85.85), FRotator(0.0, -90.0, 0.0), false, nullptr, ETeleportType::None);
			
			//Load AnimBP
			CharLS.GetColumnValueByName(TEXT("AnimBP"), AnimBPString);
			AnimBPObjectPath = AnimBPString;
			InChar->m_AnimBP = Cast<UAnimBlueprint>(AnimBPObjectPath.ResolveObject());
			if (InChar->m_AnimBP == nullptr) {
				InChar->m_AnimBP = CastChecked<UAnimBlueprint>(AnimBPObjectPath.ResolveObject());
			}
			InChar->GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			InChar->GetMesh()->SetAnimClass(InChar->m_AnimBP->GeneratedClass);

			CharLS.GetColumnValueByName(TEXT("CurrentHP"), InChar->m_Health);
			CharLS.GetColumnValueByName(TEXT("MaxHP"), InChar->m_MaxHealth);
			CharLS.GetColumnValueByName(TEXT("TargetType"), InChar->m_TargetType);
		}
	}
}

FVector SQLiteDB::LoadPlayerPosition(int32 PlayerID)
{
	FVector Position = FVector(0.0f, 0.0f, 0.0f);

	if (Database->IsValid() && LoadStatement.IsValid()) {
		LoadStatement.Reset();

		if (LoadStatement.SetBindingValueByName(TEXT("$ID"), PlayerID) && LoadStatement.Execute() && LoadStatement.Step() == ESQLitePreparedStatementStepResult::Row) {

			LoadStatement.GetColumnValueByName(TEXT("x"), Position.X);
			LoadStatement.GetColumnValueByName(TEXT("y"), Position.Y);
			LoadStatement.GetColumnValueByName(TEXT("z"), Position.Z);
		}
	}
	return Position;
}


SQLiteDB::~SQLiteDB()
{
	SaveStatement.Destroy();
	LoadStatement.Destroy();
	CharLS.Destroy();

	if (!Database->Close()) {
		UE_LOG(LogTemp, Warning, TEXT("Failed to close database: %s"), *Database->GetLastError());
	}
	else {
		delete Database;
	}
}
