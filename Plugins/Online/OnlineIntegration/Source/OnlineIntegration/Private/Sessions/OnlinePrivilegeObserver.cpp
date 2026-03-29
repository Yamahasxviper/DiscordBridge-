#include "Sessions/OnlinePrivilegeObserver.h"

void UFailedPrivilegeCheckResult::Reset(UOnlineIntegrationBackend* InBackend, ECommonUserPrivilege InPrivilege, ECommonUserPrivilegeResult InResult, const FText& InFailureDescription){ }
void UOnlinePrivilegeObserver::Initialize(ULocalUserInfo* InUserInfo, const USessionDefinition* SessionDefinition){ }
void UOnlinePrivilegeObserver::OnBackendLoginStateChanged(ULocalUserInfo* InUserInfo, TSharedRef<UE::Online::FAccountInfo> AccountInfo, UOnlineIntegrationBackend* Backend){ }
void UOnlinePrivilegeObserver::OnBackendConnectionStateChanged(UObject*, UE::FieldNotification::FFieldId){ }
void UOnlinePrivilegeObserver::OnApplicationReactivated(){ }
void UOnlinePrivilegeObserver::OnPrivilegeQueryDone(UOnlineIntegrationBackend* Backend, ECommonUserPrivilege Privilege, ECommonUserPrivilegeResult PrivilegeResult){ }
void UOnlinePrivilegeObserver::QueryAllPrivilegesForBackend(UOnlineIntegrationBackend* Backend){ }
