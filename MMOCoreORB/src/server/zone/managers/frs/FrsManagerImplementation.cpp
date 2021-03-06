#include "server/zone/managers/frs/FrsManager.h"
#include "server/zone/managers/skill/SkillManager.h"
#include "server/zone/ZoneServer.h"
#include "server/zone/managers/frs/RankMaintenanceTask.h"
#include "server/zone/managers/frs/VoteStatusTask.h"
#include "server/zone/managers/frs/FrsRankingData.h"
#include "server/zone/managers/frs/FrsManagerData.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/player/variables/FrsData.h"
#include "server/zone/objects/building/BuildingObject.h"
#include "server/chat/ChatManager.h"
#include "server/zone/managers/stringid/StringIdManager.h"
#include "server/zone/objects/player/sui/listbox/SuiListBox.h"
#include "server/zone/objects/player/sui/callbacks/EnclaveVotingTerminalSuiCallback.h"
#include "server/zone/managers/object/ObjectManager.h"
#include "server/zone/managers/player/PlayerManager.h"

void FrsManagerImplementation::initialize() {
	loadLuaConfig();

	if (!frsEnabled)
		return;

	setupEnclaves();
	loadFrsData();

	Time* lastTick = managerData->getLastMaintenanceTick();
	uint64 miliDiff = lastTick->miliDifference();

	if (rankMaintenanceTask == NULL) {
		rankMaintenanceTask = new RankMaintenanceTask(_this.getReferenceUnsafeStaticCast());

		if (miliDiff > maintenanceInterval)
			rankMaintenanceTask->execute();
		else
			rankMaintenanceTask->schedule(maintenanceInterval - miliDiff);
	} else {
		if (miliDiff > maintenanceInterval)
			rankMaintenanceTask->execute();
		else
			rankMaintenanceTask->reschedule(maintenanceInterval - miliDiff);
	}

	lastTick = managerData->getLastVoteStatusTick();
	miliDiff = lastTick->miliDifference();

	if (voteStatusTask == NULL) {
		voteStatusTask = new VoteStatusTask(_this.getReferenceUnsafeStaticCast());

		if (miliDiff > VOTE_STATUS_TICK)
			voteStatusTask->execute();
		else
			voteStatusTask->schedule(VOTE_STATUS_TICK - miliDiff);
	} else {
		if (miliDiff > VOTE_STATUS_TICK)
			voteStatusTask->execute();
		else
			voteStatusTask->reschedule(VOTE_STATUS_TICK - miliDiff);
	}
}

void FrsManagerImplementation::loadFrsData() {
	info("Loading frs manager data from frsmanager.db");

	ObjectDatabaseManager* dbManager = ObjectDatabaseManager::instance();
	ObjectDatabase* rankDatabase = ObjectDatabaseManager::instance()->loadObjectDatabase("frsmanager", true);

	if (rankDatabase == NULL) {
		error("Could not load the frsmanager database.");
		return;
	}

	try {
		ObjectDatabaseIterator iterator(rankDatabase);

		uint64 objectID;

		while (iterator.getNextKey(objectID)) {
			Reference<FrsManagerData*> manData = Core::getObjectBroker()->lookUp(objectID).castTo<FrsManagerData*>();

			if (manData != NULL) {
				managerData = manData;
			}
		}
	} catch (DatabaseException& e) {
		error("Database exception in FrsManagerImplementation::loadFrsData(): "	+ e.getMessage());
	}

	if (managerData == NULL) {
		FrsManagerData* newManagerData = new FrsManagerData();
		ObjectManager::instance()->persistObject(newManagerData, 1, "frsmanager");

		managerData = newManagerData;
	}

	Vector<ManagedReference<FrsRank*> >* rankData = managerData->getLightRanks();

	if (rankData->size() == 0) {
		short newStatus = VOTING_CLOSED;

		for (int i = 1; i <= 11; i++) {
			FrsRank* newRank = new FrsRank(COUNCIL_LIGHT, i, newStatus);
			ObjectManager::instance()->persistObject(newRank, 1, "frsranks");
			rankData->add(newRank);
		}
	}

	rankData = managerData->getDarkRanks();

	if (rankData->size() == 0) {
		Vector<ManagedReference<FrsRank*> >* newRankData = NULL;
		short newStatus = VOTING_CLOSED;

		for (int i = 1; i <= 11; i++) {
			FrsRank* newRank = new FrsRank(COUNCIL_DARK, i, newStatus);
			ObjectManager::instance()->persistObject(newRank, 1, "frsranks");
			rankData->add(newRank);
		}
	}
}

void FrsManagerImplementation::loadLuaConfig() {
	Lua* lua = new Lua();
	lua->init();

	bool res = lua->runFile("custom_scripts/managers/jedi/frs_manager.lua");

	if (!res)
		res = lua->runFile("scripts/managers/jedi/frs_manager.lua");

	if (!res) {
		error("Unable to load FrsManager data.");
		delete lua;
		return;
	}

	frsEnabled = lua->getGlobalInt("frsEnabled");
	petitionInterval = lua->getGlobalInt("petitionInterval");
	votingInterval = lua->getGlobalInt("votingInterval");
	acceptanceInterval = lua->getGlobalInt("acceptanceInterval");
	maintenanceInterval = lua->getGlobalInt("maintenanceInterval");
	requestDemotionDuration = lua->getGlobalInt("requestDemotionDuration");
	voteChallengeDuration = lua->getGlobalInt("voteChallengeDuration");
	baseMaintCost = lua->getGlobalInt("baseMaintCost");
	requestDemotionCost = lua->getGlobalInt("requestDemotionCost");
	voteChallengeCost = lua->getGlobalInt("voteChallengeCost");
	maxPetitioners = lua->getGlobalInt("maxPetitioners");
	missedVotePenalty = lua->getGlobalInt("missedVotePenalty");

	uint32 enclaveID = lua->getGlobalInt("lightEnclaveID");

	lightEnclave = zoneServer->getObject(enclaveID).castTo<BuildingObject*>();

	enclaveID = lua->getGlobalInt("darkEnclaveID");

	darkEnclave = zoneServer->getObject(enclaveID).castTo<BuildingObject*>();

	LuaObject luaObject = lua->getGlobalObject("enclaveRoomRequirements");

	if (luaObject.isValidTable()) {
		for(int i = 1; i <= luaObject.getTableSize(); ++i) {
			LuaObject entry = luaObject.getObjectAt(i);

			uint32 cellID = entry.getIntAt(1);
			int rank = entry.getIntAt(2);

			roomRequirements.put(cellID, rank);

			entry.pop();
		}
	}

	luaObject.pop();

	luaObject = lua->getGlobalObject("lightRankingData");

	if (luaObject.isValidTable()) {
		for(int i = 1; i <= luaObject.getTableSize(); ++i) {
			LuaObject entry = luaObject.getObjectAt(i);

			int rank = entry.getIntAt(1);
			String skillName = entry.getStringAt(2);
			String titleName = entry.getStringAt(3);
			int reqExp = entry.getIntAt(4);
			int playerCap = entry.getIntAt(5);

			Reference<FrsRankingData*> data = new FrsRankingData(rank, skillName, titleName, reqExp, playerCap);

			lightRankingData.put(rank, data);

			entry.pop();
		}
	}

	luaObject.pop();

	luaObject = lua->getGlobalObject("darkRankingData");

	if (luaObject.isValidTable()) {
		for(int i = 1; i <= luaObject.getTableSize(); ++i) {
			LuaObject entry = luaObject.getObjectAt(i);

			int rank = entry.getIntAt(1);
			String skillName = entry.getStringAt(2);
			String titleName = entry.getStringAt(3);
			int reqExp = entry.getIntAt(4);
			int playerCap = entry.getIntAt(5);

			Reference<FrsRankingData*> data = new FrsRankingData(rank, skillName, titleName, reqExp, playerCap);

			darkRankingData.put(rank, data);

			entry.pop();
		}
	}

	luaObject.pop();

	luaObject = lua->getGlobalObject("frsExperienceValues");

	if (luaObject.isValidTable()) {
		for(int i = 1; i <= luaObject.getTableSize(); ++i) {
			LuaObject entry = luaObject.getObjectAt(i);
			uint64 keyHash = entry.getStringAt(1).hashCode();

			Vector<int> expValues;

			for (int j = 0; j <= 11; j++) {
				int value = entry.getIntAt(j + 2);
				expValues.add(value);
			}

			experienceValues.put(keyHash, expValues);

			entry.pop();
		}
	}

	luaObject.pop();

	delete lua;
	lua = NULL;
}

void FrsManagerImplementation::setupEnclaves() {
	ManagedReference<BuildingObject*> enclaveBuilding = lightEnclave.get();

	if (enclaveBuilding != NULL)
		setupEnclaveRooms(enclaveBuilding, "LightEnclaveRank");

	enclaveBuilding = darkEnclave.get();

	if (enclaveBuilding != NULL)
		setupEnclaveRooms(enclaveBuilding, "DarkEnclaveRank");
}

FrsRank* FrsManagerImplementation::getFrsRank(short councilType, int rank) {
	if (rank < 0)
		return NULL;

	if (councilType == COUNCIL_LIGHT) {
		Vector<ManagedReference<FrsRank*> >* rankData = managerData->getLightRanks();

		for (int i = 0; i < rankData->size(); i++) {
			ManagedReference<FrsRank*> frsRank = rankData->get(i);

			if (frsRank->getRank() == rank)
				return frsRank;
		}
	} else if (councilType == COUNCIL_DARK) {
		Vector<ManagedReference<FrsRank*> >* rankData = managerData->getDarkRanks();

		for (int i = 0; i < rankData->size(); i++) {
			ManagedReference<FrsRank*> frsRank = rankData->get(i);

			if (frsRank->getRank() == rank)
				return frsRank;
		}
	}

	return NULL;
}


void FrsManagerImplementation::setupEnclaveRooms(BuildingObject* enclaveBuilding, const String& groupName) {
	for (int j = 1; j <= enclaveBuilding->getTotalCellNumber(); ++j) {
		ManagedReference<CellObject*> cell = enclaveBuilding->getCell(j);

		if (cell != NULL) {
			cell->setContainerComponent("EnclaveContainerComponent");

			int roomReq = getRoomRequirement(cell->getObjectID());

			if (roomReq == -1)
				continue;

			ContainerPermissions* permissions = cell->getContainerPermissions();

			permissions->setInheritPermissionsFromParent(false);
			permissions->clearDefaultAllowPermission(ContainerPermissions::WALKIN);
			permissions->setDefaultDenyPermission(ContainerPermissions::WALKIN);

			for (int i = 11; i >= roomReq; i--) {
				permissions->setAllowPermission(groupName + String::valueOf(i), ContainerPermissions::WALKIN);
			}
		}
	}
}

void FrsManagerImplementation::setPlayerRank(CreatureObject* player, int rank, bool giveRobe) {
	if (player == NULL)
		return;

	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == NULL)
		return;

	uint64 playerID = player->getObjectID();

	FrsData* frsData = ghost->getFrsData();

	int councilType = frsData->getCouncilType();
	String groupName = "";

	if (councilType == COUNCIL_LIGHT)
		groupName = "LightEnclaveRank";
	else if (councilType == COUNCIL_DARK)
		groupName = "DarkEnclaveRank";
	else
		return;

	int curRank = frsData->getRank();

	if (curRank > 0) {
		ghost->removePermissionGroup(groupName + String::valueOf(curRank), true);

		FrsRank* rankData = getFrsRank(councilType, curRank);

		if (rankData != NULL) {
			rankData->removeFromPlayerList(playerID);
		}
	}

	frsData->setRank(rank);

	ghost->addPermissionGroup(groupName + String::valueOf(rank), true);

	FrsRank* rankData = getFrsRank(councilType, rank);

	if (rankData != NULL) {
		rankData->addToPlayerList(playerID);
	}

	if (giveRobe) {
		if (rank == 0 || rank == 1 || rank == 5 || rank == 8 || rank == 10)
			recoverJediItems(player);
	}

	if (player->isOnline())
		updatePlayerSkills(player);
}

void FrsManagerImplementation::updatePlayerSkills(CreatureObject* player) {
	PlayerObject* ghost = player->getPlayerObject();
	SkillManager* skillManager = zoneServer->getSkillManager();

	if (ghost == NULL || skillManager == NULL)
		return;

	FrsData* frsData = ghost->getFrsData();
	int playerRank = frsData->getRank();
	int councilType = frsData->getCouncilType();
	VectorMap<uint, Reference<FrsRankingData*> > rankingData;

	if (councilType == COUNCIL_LIGHT)
		rankingData = lightRankingData;
	else if (councilType == COUNCIL_DARK)
		rankingData = darkRankingData;
	else
		return;

	for (int i = 0; i <= 11; i++) {
		Reference<FrsRankingData*> rankData = rankingData.get(i);
		String rankSkill = rankData->getSkillName();
		String rankTitle = rankData->getTitleName();
		int rank = rankData->getRank();

		if (playerRank >= rank) {
			if (!player->hasSkill(rankSkill))
				skillManager->awardSkill(rankSkill, player, true, false, true);

			if (rankTitle != "" && !player->hasSkill(rankTitle))
				player->addSkill(rankTitle, true);
		} else {
			if (player->hasSkill(rankSkill))
				skillManager->surrenderSkill(rankSkill, player, true, true);

			if (rankTitle != "" && player->hasSkill(rankTitle))
				player->removeSkill(rankTitle, true);
		}
	}
}

void FrsManagerImplementation::promotePlayer(CreatureObject* player) {
	if (player == NULL)
		return;

	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == NULL)
		return;

	FrsData* frsData = ghost->getFrsData();
	int rank = frsData->getRank();

	if (rank > 10)
		return;

	int newRank = rank + 1;
	setPlayerRank(player, newRank, true);

	String stfRank = "@force_rank:rank" + String::valueOf(newRank);
	String rankString = StringIdManager::instance()->getStringId(stfRank.hashCode()).toString();

	StringIdChatParameter param("@force_rank:rank_gained"); // You have achieved the Enclave rank of %TO.
	param.setTO(rankString);
	player->sendSystemMessage(param);
}

void FrsManagerImplementation::demotePlayer(CreatureObject* player) {
	if (player == NULL)
		return;

	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == NULL)
		return;

	FrsData* frsData = ghost->getFrsData();
	int rank = frsData->getRank();

	if (rank == 0)
		return;

	int newRank = rank - 1;
	setPlayerRank(player, newRank, false);

	String stfRank = "@force_rank:rank" + String::valueOf(newRank);
	String rankString = StringIdManager::instance()->getStringId(stfRank.hashCode()).toString();

	StringIdChatParameter param("@force_rank:rank_lost"); // You have been demoted to %TO.
	param.setTO(rankString);
	player->sendSystemMessage(param);
}

void FrsManagerImplementation::adjustFrsExperience(CreatureObject* player, int amount) {
	if (player == NULL || amount == 0)
		return;

	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == NULL)
		return;

	if (amount > 0) {
		ghost->addExperience("force_rank_xp", amount, true);

		StringIdChatParameter param("@force_rank:experience_granted"); // You have gained %DI Force Rank experience.
		param.setDI(amount);

		player->sendSystemMessage(param);
	} else {
		FrsData* frsData = ghost->getFrsData();
		int rank = frsData->getRank();
		int councilType = frsData->getCouncilType();

		int curExperience = ghost->getExperience("force_rank_xp");

		// Ensure we dont go into the negatives
		if ((amount * -1) > curExperience)
			amount = curExperience * -1;

		ghost->addExperience("force_rank_xp", amount, true);

		StringIdChatParameter param("@force_rank:experience_lost"); // You have lost %DI Force Rank experience.
		param.setDI(amount * -1);
		player->sendSystemMessage(param);

		curExperience += amount;

		Reference<FrsRankingData*> rankingData = NULL;

		if (councilType == COUNCIL_LIGHT)
			rankingData = lightRankingData.get(rank);
		else if (councilType == COUNCIL_DARK)
			rankingData = darkRankingData.get(rank);

		if (rankingData == NULL)
			return;

		int reqXp = rankingData->getRequiredExperience();

		if (reqXp > curExperience)
			demotePlayer(player);
	}
}

Vector<uint64> FrsManagerImplementation::getFullPlayerList() {
	Vector<uint64> playerList;
	Vector<uint64> memberList = getPlayerListByCouncil(COUNCIL_LIGHT);

	for (int i = 0; i < memberList.size(); ++i) {
		uint64 playerID = memberList.get(i);
		playerList.add(playerID);
	}

	memberList = getPlayerListByCouncil(COUNCIL_DARK);

	for (int i = 0; i < memberList.size(); ++i) {
		uint64 playerID = memberList.get(i);
		playerList.add(playerID);
	}

	return playerList;
}

Vector<uint64> FrsManagerImplementation::getPlayerListByCouncil(int councilType) {
	Vector<uint64> playerList;

	Vector<ManagedReference<FrsRank*> >* rankData;

	if (councilType == COUNCIL_LIGHT)
		rankData = managerData->getLightRanks();
	else if (councilType == COUNCIL_DARK)
		rankData = managerData->getDarkRanks();
	else
		return playerList;

	for (int i = 0; i < rankData->size(); i++) {
		ManagedReference<FrsRank*> frsRank = rankData->get(i);

		if (frsRank == NULL)
			continue;

		SortedVector<uint64>* rankList = frsRank->getPlayerList();

		for (int j = 0; j < rankList->size(); j++) {
			uint64 playerID = rankList->get(j);
			playerList.add(playerID);
		}
	}

	return playerList;
}


void FrsManagerImplementation::deductMaintenanceXp(CreatureObject* player) {
	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == NULL)
		return;

	FrsData* frsData = ghost->getFrsData();
	int rank = frsData->getRank();

	if (rank == 0)
		return;

	int maintXp = rank * 100;

	ChatManager* chatManager = zoneServer->getChatManager();

	StringIdChatParameter mailBody("@force_rank:xp_maintenance_body"); // You have lost %DI Force Rank experience. All members of Rank 1 or higher must pay experience each day to remain in their current positions. (Note: This loss may not take effect until your next login.)
	mailBody.setDI(maintXp);

	chatManager->sendMail("Enclave Records", "@force_rank:xp_maintenace_sub", mailBody, player->getFirstName(), NULL);

	addExperienceDebt(player, maintXp);
}

void FrsManagerImplementation::addExperienceDebt(CreatureObject* player, int amount) {
	uint64 playerID = player->getObjectID();

	Locker locker(_this.getReferenceUnsafeStaticCast());

	int curDebt = managerData->getExperienceDebt(playerID);

	managerData->setExperienceDebt(playerID, curDebt + amount);
}

void FrsManagerImplementation::deductDebtExperience(CreatureObject* player) {
	uint64 playerID = player->getObjectID();

	Locker clocker(_this.getReferenceUnsafeStaticCast(), player);

	int curDebt = managerData->getExperienceDebt(playerID);

	if (curDebt <= 0)
		return;

	adjustFrsExperience(player, curDebt * -1);

	managerData->removeExperienceDebt(playerID);
}

bool FrsManagerImplementation::isValidFrsBattle(CreatureObject* attacker, CreatureObject* victim) {
	PlayerObject* attackerGhost = attacker->getPlayerObject();
	PlayerObject* victimGhost = victim->getPlayerObject();

	// No credit if they were killed by the attacker recently
	if (attackerGhost == NULL || victimGhost == NULL)
		return false;

	FrsData* attackerData = attackerGhost->getFrsData();
	int attackerRank = attackerData->getRank();
	int attackerCouncil = attackerData->getCouncilType();

	FrsData* victimData = victimGhost->getFrsData();
	int victimRank = victimData->getRank();
	int victimCouncil = victimData->getCouncilType();

	// Neither player is in the FRS
	if (victimCouncil == 0 && attackerCouncil == 0)
		return false;

	// No credit if they are in the same council
	if ((attackerCouncil == COUNCIL_LIGHT && victimCouncil == COUNCIL_LIGHT) || (attackerCouncil == COUNCIL_DARK && victimCouncil == COUNCIL_DARK))
		return false;

	return true;
}

int FrsManagerImplementation::calculatePvpExperienceChange(CreatureObject* attacker, CreatureObject* victim, float contribution, bool isVictim) {
	PlayerObject* attackerGhost = attacker->getPlayerObject();
	PlayerObject* victimGhost = victim->getPlayerObject();

	if (attackerGhost == NULL || victimGhost == NULL)
		return 0;

	PlayerObject* playerGhost = NULL;
	PlayerObject* opponentGhost = NULL;

	if (isVictim) {
		playerGhost = victimGhost;
		opponentGhost = attackerGhost;
	} else {
		playerGhost = attackerGhost;
		opponentGhost = victimGhost;
	}

	int xpChange = getBaseExperienceGain(playerGhost, opponentGhost, !isVictim);

	if (xpChange != 0)
		xpChange = (int)((float)xpChange * contribution);

	return xpChange;
}

int FrsManagerImplementation::getBaseExperienceGain(PlayerObject* playerGhost, PlayerObject* opponentGhost, bool playerWon) {
	ManagedReference<CreatureObject*> opponent = opponentGhost->getParentRecursively(SceneObjectType::PLAYERCREATURE).castTo<CreatureObject*>();

	if (opponent == NULL)
		return 0;

	FrsData* playerData = playerGhost->getFrsData();
	int playerRank = playerData->getRank();
	int playerCouncil = playerData->getCouncilType();

	FrsData* opponentData = opponentGhost->getFrsData();
	int opponentRank = opponentData->getRank();

	// Make sure player is part of a council before we grab any value to award
	if (playerCouncil == 0)
		return 0;

	String key = "";

	if (opponent->hasSkill("combat_bountyhunter_master")) { // Opponent is MBH
		key = "bh_";
	} else if (opponentRank >= 0 && opponent->hasSkill("force_title_jedi_rank_03")) { // Opponent is at least a knight
		key = "rank" + String::valueOf(opponentRank) + "_";
	} else if (opponent->hasSkill("force_title_jedi_rank_02")) { // Opponent is padawan
		key = "padawan_";
	} else { // Opponent is non jedi
		key = "nonjedi_";
	}

	if (playerWon) {
		key = key + "win";
	} else {
		key = key + "lose";
	}

	uint64 keyHash = key.hashCode();

	if (!experienceValues.contains(keyHash))
		return 0;

	Vector<int> expValues = experienceValues.get(keyHash);

	int returnValue = expValues.get(playerRank);

	if (!playerWon)
		returnValue *= -1;

	return returnValue;
}

void FrsManagerImplementation::sendVoteSUI(CreatureObject* player, SceneObject* terminal, short suiType, short enclaveType) {
	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == NULL)
		return;

	FrsData* frsData = ghost->getFrsData();
	int council = frsData->getCouncilType();

	Vector<String> elementList;

	String councilString;
	String stfRank;

	if (council == 1)
		councilString = "light";
	else if (council == 2)
		councilString = "dark";

	for (int i = 1; i < 12; i++) {
		if (i >= 1 && i <= 9)
			stfRank = "@skl_n:force_rank_" + councilString + "_rank_0" + String::valueOf(i);
		else if (i == 10)
			stfRank = "@skl_n:force_rank_" + councilString + "_rank_10";
		else if (i == 11)
			stfRank = "@skl_n:force_rank_" + councilString + "_master";

		elementList.add(stfRank);
	}

	ManagedReference<SuiListBox*> box = new SuiListBox(player, SuiWindowType::ENCLAVE_VOTING, SuiListBox::HANDLETWOBUTTON);
	box->setCallback(new EnclaveVotingTerminalSuiCallback(player->getZoneServer(), suiType, enclaveType, -1, true));
	box->setUsingObject(terminal);
	box->setOkButton(true, "@ok");
	box->setCancelButton(true, "@cancel");

	if (suiType == SUI_VOTE_STATUS) {
		box->setPromptText("@force_rank:vote_status_select"); // Select the rank whose status you wish to view.
		box->setPromptTitle("@force_rank:rank_selection"); // Rank Selection
	} else if (suiType == SUI_VOTE_RECORD) {
		box->setPromptText("@force_rank:vote_record_select"); // Select the rank for which you wish to record your vote.
		box->setPromptTitle("@force_rank:rank_selection"); // Rank Selection
		int rank = frsData->getRank();

		for (int i = 0; i < elementList.size(); i++) {
			FrsRank* rankData = getFrsRank(enclaveType, i + 1);

			if (rankData == NULL || rankData->getVoteStatus() != VOTING_OPEN)
				continue;

			if (getVoteWeight(rank, i) > 0 && !hasPlayerVoted(player, rank)) {
				String voteString = elementList.get(i);
				voteString = voteString + " *";
				elementList.setElementAt(i, voteString);
			}
		}
	} else if (suiType == SUI_VOTE_ACCEPT_PROMOTE) {
		box->setPromptText("@force_rank:vote_promotion_select"); // Select the rank whose status you wish to view.
		box->setPromptTitle("@force_rank:rank_selection"); // Rank Selection
	}  else if (suiType == SUI_VOTE_PETITION) {
		box->setPromptText("@force_rank:vote_petition_select"); // Select the rank for which you wish to petition for membership.
		box->setPromptTitle("@force_rank:rank_selection"); // Rank Selection
	}  else if (suiType == SUI_VOTE_DEMOTE) {
		box->setPromptText("@force_rank:demote_select_rank"); //Select the rank whose member you wish to demote.
		box->setPromptTitle("@force_rank:rank_selection"); // Rank Selection
	} else if (suiType == SUI_FORCE_PHASE_CHANGE && ghost->isPrivileged()) {
		box->setPromptText("Select the rank you want to force a phase change on.");
		box->setPromptTitle("@force_rank:rank_selection"); // Rank Selection
	}

	for (int i = 0; i < elementList.size(); i++)
		box->addMenuItem(elementList.get(i));

	box->setUsingObject(terminal);
	player->getPlayerObject()->addSuiBox(box);
	player->sendMessage(box->generateMessage());
}

void FrsManagerImplementation::handleVoteStatusSui(CreatureObject* player, SceneObject* terminal, short enclaveType, int rank) {
	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == NULL)
		return;

	FrsRank* rankData = getFrsRank(enclaveType, rank);

	if (rankData == NULL) {
		player->sendSystemMessage("@force_rank:invalid_rank_selected"); // That is an invalid rank.
		return;
	}

	ManagedReference<SuiListBox*> box = new SuiListBox(player, SuiWindowType::ENCLAVE_VOTING, SuiListBox::HANDLESINGLEBUTTON);
	box->setOkButton(true, "@ok");
	box->setPromptText("Vote Status for Rank " + String::valueOf(rank));
	box->setPromptTitle("@force_rank:vote_status"); // Voting Status

	short voteStatus = rankData->getVoteStatus();

	if (voteStatus == 0)
		return;

	String curPhase = "";

	if (voteStatus == PETITIONING)
		curPhase = "Petitioning";
	else if (voteStatus == VOTING_OPEN)
		curPhase = "Voting";
	else if (voteStatus == WAITING)
		curPhase = "Acceptance";
	else
		curPhase = "Inactive";

	box->addMenuItem("Current Stage: " + curPhase);

	int slotsAvailable = getAvailableRankSlots(rankData);

	if (slotsAvailable < 0)
		slotsAvailable = 0;

	box->addMenuItem("Seats Available: " + String::valueOf(slotsAvailable));

	uint64 miliDiff = rankData->getLastUpdateTickDiff();
	uint64 interval = getVotingInterval(voteStatus);
	uint64 timeRemaining = interval - miliDiff;
	String timeLeft = "";

	if (timeRemaining > 0)
		timeLeft = getTimeString(timeRemaining / 1000);
	else
		timeLeft = "closed.";

	box->addMenuItem("Time Remaining: " + timeLeft);

	if (voteStatus != VOTING_CLOSED)
		box->addMenuItem("");

	if (voteStatus == PETITIONING || voteStatus == VOTING_OPEN) {

		VectorMap<uint64, int>* petitionerList = rankData->getPetitionerList();

		if (petitionerList->size() > 0) {
			box->addMenuItem("Petitioners:");

			for (int i = 0; i < petitionerList->size(); i++) {
				if (i > 20)
					break;

				VectorMapEntry<uint64, int> entry = petitionerList->elementAt(i);
				uint64 petitionerID = entry.getKey();
				int votes = entry.getValue();
				ManagedReference<CreatureObject*> player = zoneServer->getObject(petitionerID).castTo<CreatureObject*>();

				if (player == NULL) {
					rankData->removeFromPetitionerList(petitionerID);
					continue;
				}

				String playerName = player->getFirstName();

				if (voteStatus == PETITIONING)
					box->addMenuItem("   " + playerName);
				else
					box->addMenuItem("   " + playerName + "    " + String::valueOf(votes));
			}
		}
	} else if (voteStatus == WAITING) {
		SortedVector<uint64>* winnerList = rankData->getWinnerList();

		if (winnerList->size() > 0) {
			box->addMenuItem("Awaiting Acceptance:");

			for (int i = 0; i < winnerList->size(); i++) {
				uint64 winnerID = winnerList->get(i);
				ManagedReference<CreatureObject*> player = zoneServer->getObject(winnerID).castTo<CreatureObject*>();

				if (player == NULL) {
					rankData->removeFromPetitionerList(winnerID);
					continue;
				}

				String playerName = player->getFirstName();
				box->addMenuItem("   " + playerName);
			}
		}
	}

	box->setUsingObject(terminal);
	player->getPlayerObject()->addSuiBox(box);
	player->sendMessage(box->generateMessage());
}

void FrsManagerImplementation::sendVoteRecordSui(CreatureObject* player, SceneObject* terminal, short enclaveType, int rank) {
	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == NULL)
		return;

	FrsRank* rankData = getFrsRank(enclaveType, rank);

	if (rankData == NULL) {
		player->sendSystemMessage("@force_rank:invalid_rank_selected"); // That is an invalid rank.
		return;
	}

	short voteStatus = rankData->getVoteStatus();

	if (voteStatus != VOTING_OPEN) {
		player->sendSystemMessage("@force_rank:voting_not_open"); // Voting for that rank is not currently open.
		return;
	}

	FrsData* frsData = ghost->getFrsData();
	int playerRank = frsData->getRank();

	int voteWeight = getVoteWeight(playerRank, rank);

	if (voteWeight <= 0) {
		player->sendSystemMessage("@force_rank:cant_vote_for_rank"); // You are not allowed to cast votes for promotions to that rank.
		return;
	}

	if (rankData->isOnVotedList(player->getObjectID())) {
		player->sendSystemMessage("@force_rank:already_voted"); // You have already voted
		return;
	}

	if (rankData->getTotalPetitioners() <= 0) {
		player->sendSystemMessage("@force_rank:noone_to_vote_for"); // There is no one for which to vote.
		return;
	}

	VectorMap<uint64, int>* petitionerList = rankData->getPetitionerList();

	ManagedReference<SuiListBox*> box = new SuiListBox(player, SuiWindowType::ENCLAVE_VOTING, SuiListBox::HANDLETWOBUTTON);
	box->setCallback(new EnclaveVotingTerminalSuiCallback(player->getZoneServer(), SUI_VOTE_RECORD, enclaveType, rank, false));
	box->setUsingObject(terminal);
	box->setOkButton(true, "@ok");
	box->setCancelButton(true, "@cancel");

	box->setPromptText("@force_rank:vote_record_select_player"); // Select the petitioner to whom you wish to promote.
	box->setPromptTitle("@force_rank:vote_player_selection"); // Petitioner Selection

	for (int i = 0; i < petitionerList->size(); i++) {
		VectorMapEntry<uint64, int> entry = petitionerList->elementAt(i);
		uint64 petitionerID = entry.getKey();
		ManagedReference<CreatureObject*> player = zoneServer->getObject(petitionerID).castTo<CreatureObject*>();

		if (player == NULL) {
			rankData->removeFromPetitionerList(petitionerID);
			continue;
		}

		String playerName = player->getFirstName();
		box->addMenuItem(playerName);
	}

	box->setUsingObject(terminal);
	player->getPlayerObject()->addSuiBox(box);
	player->sendMessage(box->generateMessage());
}

void FrsManagerImplementation::forcePhaseChange(CreatureObject* player, short enclaveType, int rank) {
	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == NULL || !ghost->isPrivileged())
		return;

	FrsRank* rankData = getFrsRank(enclaveType, rank);

	if (rankData == NULL) {
		player->sendSystemMessage("@force_rank:invalid_rank_selected"); // That is an invalid rank.
		return;
	}

	runVotingUpdate(rankData);
	player->sendSystemMessage("Rank " + String::valueOf(rank) + "'s phase has been changed.");
}

void FrsManagerImplementation::handleVoteRecordSui(CreatureObject* player, SceneObject* terminal, short enclaveType, int rank, int index) {
	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == NULL)
		return;

	FrsRank* rankData = getFrsRank(enclaveType, rank);

	if (rankData == NULL) {
		player->sendSystemMessage("@force_rank:invalid_rank_selected"); // That is an invalid rank.
		return;
	}

	short voteStatus = rankData->getVoteStatus();

	if (voteStatus != VOTING_OPEN) {
		player->sendSystemMessage("@force_rank:vote_time_expired"); // Time has expired for the voting process.
		return;
	}

	FrsData* frsData = ghost->getFrsData();
	int playerRank = frsData->getRank();

	int voteWeight = getVoteWeight(playerRank, rank);

	if (voteWeight <= 0) {
		player->sendSystemMessage("@force_rank:cant_vote_for_rank"); // You are not allowed to cast votes for promotions to that rank.
		return;
	}

	uint64 playerID = player->getObjectID();

	if (rankData->isOnVotedList(playerID)) {
		player->sendSystemMessage("@force_rank:already_voted"); // You have already voted
		return;
	}

	VectorMap<uint64, int>* petitionerList = rankData->getPetitionerList();

	if (petitionerList->size() <= index)
		return;

	VectorMapEntry<uint64, int> petitionerData = petitionerList->elementAt(index);

	uint64 petitionerID = petitionerData.getKey();
	ManagedReference<CreatureObject*> petitioner = zoneServer->getObject(petitionerID).castTo<CreatureObject*>();

	if (petitioner == NULL) {
		rankData->removeFromPetitionerList(petitionerID);
		return;
	}

	int curVotes = petitionerData.getValue();

	String playerName = petitioner->getFirstName();
	rankData->addToPetitionerList(petitionerID, curVotes + 1);

	StringIdChatParameter voteCast("@force_rank:vote_cast"); // You cast your vote for %TO.
	voteCast.setTO(playerName);

	player->sendSystemMessage(voteCast);

	rankData->addToVotedList(playerID);
}

void FrsManagerImplementation::handleVotePetitionSui(CreatureObject* player, SceneObject* terminal, short enclaveType, int rank) {
	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == NULL)
		return;

	FrsRank* rankData = getFrsRank(enclaveType, rank);

	if (rankData == NULL) {
		player->sendSystemMessage("@force_rank:invalid_rank_selected"); // That is an invalid rank.
		return;
	}

	short voteStatus = rankData->getVoteStatus();

	if (voteStatus != PETITIONING) {
		player->sendSystemMessage("@force_rank:petition_time_expired"); // 	Time has expired for petitioning.
		return;
	}

	FrsData* frsData = ghost->getFrsData();
	int playerRank = frsData->getRank();
	uint64 playerID = player->getObjectID();

	if (playerRank >= rank) {
		player->sendSystemMessage("@force_rank:petition_already_have_rank"); // You have already attained this rank.
		return;
	}

	if (rankData->isOnPetitionerList(playerID)) {
		player->sendSystemMessage("@force_rank:already_petitioning"); // You are already petitioning for this rank.
		return;
	}

	if (rankData->getTotalPetitioners() >= maxPetitioners) {
		player->sendSystemMessage("@force_rank:petitioning_no_room"); // There is already the maximum number of petitioners.
		return;
	}

	if (!isEligibleForPromotion(player, rank)) {
		player->sendSystemMessage("@force_rank:petitioning_not_eligible"); // You are not eligible to petition for this rank.
		return;
	}

	rankData->addToPetitionerList(playerID, 0);
	player->sendSystemMessage("@force_rank:petitioning_complete"); // You add your name to the list of petitioners.
}

void FrsManagerImplementation::sendVoteDemoteSui(CreatureObject* player, SceneObject* terminal, short enclaveType, int rank) {
	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == NULL)
		return;

	FrsRank* rankData = getFrsRank(enclaveType, rank);

	if (rankData == NULL) {
		player->sendSystemMessage("@force_rank:invalid_rank_selected"); // That is an invalid rank.
		return;
	}

	FrsData* frsData = ghost->getFrsData();
	int playerRank = frsData->getRank();

	Time lastDemoteTime = frsData->getLastDemoteTimestamp();

	if (!lastDemoteTime.isPast()) {
		auto demoteTimeMs = lastDemoteTime.miliDifference();
		StringIdChatParameter param("@force_rank:demote_too_soon"); //You cannot demote a member for another %TO
		param.setTO(getTimeString(llabs(demoteTimeMs / 1000)));
		player->sendSystemMessage(param);
		return;
	}

	int requiredRank = rank + 2;

	if (playerRank < requiredRank && playerRank != 11) {
		player->sendSystemMessage("@force_rank:demote_too_low_rank"); // You must be at least two Tiers higher than that of the rank you wish to demote.  The Council Leader may demote anyone.
		return;
	}

	SortedVector<uint64>* playerList = rankData->getPlayerList();

	ManagedReference<SuiListBox*> box = new SuiListBox(player, SuiWindowType::ENCLAVE_VOTING, SuiListBox::HANDLETWOBUTTON);
	box->setCallback(new EnclaveVotingTerminalSuiCallback(player->getZoneServer(), SUI_VOTE_DEMOTE, enclaveType, rank, false));
	box->setUsingObject(terminal);
	box->setOkButton(true, "@ok");
	box->setCancelButton(true, "@cancel");

	box->setPromptText("@force_rank:demote_select_player"); // Select the member that you wish to demote.  Once you make this selection, it may not be undone.
	box->setPromptTitle("@force_rank:demote_member"); // Demote Lower Tier Member

	for (int i = 0; i < playerList->size(); i++) {
		uint64 playerID = playerList->elementAt(i);
		ManagedReference<CreatureObject*> player = zoneServer->getObject(playerID).castTo<CreatureObject*>();

		if (player == NULL)
			continue;

		String playerName = player->getFirstName();
		box->addMenuItem(playerName);
	}

	box->setUsingObject(terminal);
	player->getPlayerObject()->addSuiBox(box);
	player->sendMessage(box->generateMessage());
}

void FrsManagerImplementation::handleVoteDemoteSui(CreatureObject* player, SceneObject* terminal, short enclaveType, int rank, int index) {
	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == NULL)
		return;

	FrsRank* rankData = getFrsRank(enclaveType, rank);
	FrsData* frsData = ghost->getFrsData();

	if (rankData == NULL) {
		player->sendSystemMessage("@force_rank:invalid_rank_selected"); // That is an invalid rank.
		return;
	}

	SortedVector<uint64>* playerList = rankData->getPlayerList();

	if (playerList->size() <= index)
		return;

	if (ghost->getExperience("force_rank_xp") < 2500) {
		player->sendSystemMessage("You do not have the required experience to demote this player.");
		return;
	}

	uint64 playerID = playerList->elementAt(index);
	ManagedReference<CreatureObject*> demotedPlayer = zoneServer->getObject(playerID).castTo<CreatureObject*>();

	if (demotedPlayer == NULL) {
		player->sendSystemMessage("@force_rank:demote_player_changed_rank"); // That member's rank has changed since you have made your selection.
		return;
	}

	demotePlayer(demotedPlayer);
	frsData->updateLastDemoteTimestamp(requestDemotionDuration);
	ghost->addExperience("force_rank_xp", -2500, true);

	StringIdChatParameter demoteMsg("@force_rank:demote_player_complete"); // You demote %TO.
	demoteMsg.setTO(demotedPlayer->getFirstName());
	player->sendSystemMessage(demoteMsg);

	//Send Mail To Demoted Player
	ChatManager* chatManager = zoneServer->getChatManager();
	if (chatManager != NULL) {
		StringIdChatParameter mailBody("@force_rank:demote_request_body"); // Using Rank privilege, %TO has demoted you one rank.
		mailBody.setTO(player->getFirstName());
		chatManager->sendMail("Enclave Records", "@force_rank:demote_request_sub_rank", mailBody, demotedPlayer->getFirstName());
	}

	//Send Mail To All Members Of The Demoted Players Old Rank
	StringIdChatParameter mailBody("@force_rank:demote_request_body_rank"); // Using Rank privilege, %TU has demoted %TT by one rank.
	mailBody.setTU(player->getFirstName());
	mailBody.setTT(demotedPlayer->getFirstName());
	sendMailToList(playerList, "@force_rank:demote_request_sub_rank", mailBody);
}

void FrsManagerImplementation::handleAcceptPromotionSui(CreatureObject* player, SceneObject* terminal, short enclaveType, int rank) {
	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == NULL)
		return;

	FrsRank* rankData = getFrsRank(enclaveType, rank);

	if (rankData == NULL) {
		player->sendSystemMessage("@force_rank:invalid_rank_selected"); // That is an invalid rank.
		return;
	}

	short voteStatus = rankData->getVoteStatus();

	if (voteStatus != WAITING) {
		player->sendSystemMessage("@force_rank:acceptance_time_expired"); // Time has expired for accepting promotions.
		return;
	}

	uint64 playerID = player->getObjectID();

	if (!rankData->isOnWinnerList(playerID)) {
		player->sendSystemMessage("@force_rank:not_a_winner"); // You must win the vote before you can accept a promotion.
		return;
	}

	int availSlots = getAvailableRankSlots(rankData);

	if (availSlots <= 0) {
		SortedVector<uint64>* winnerList = rankData->getWinnerList();

		StringIdChatParameter mailBody("@force_rank:vote_last_seat_taken_body"); // The last available seat for %TO has been filled. When another becomes available, you will be notified. As long as you remain eligible for %TO, you will be able to accept the promotion without a further vote.
		mailBody.setTO("@force_rank:rank" + String::valueOf(rank));
		sendMailToList(winnerList, "@force_rank:vote_last_seat_taken_sub", mailBody);

		player->sendSystemMessage(mailBody);
		rankData->setVoteStatus(VOTING_CLOSED);
		return;
	}

	FrsData* frsData = ghost->getFrsData();
	int playerRank = frsData->getRank();

	if (playerRank >= rank) {
		player->sendSystemMessage("@force_rank:promotion_already_have_rank"); // You have already achieved this rank.
		return;
	}

	if (!isEligibleForPromotion(player, rank)) {
		player->sendSystemMessage("@force_rank:not_eligible_for_promotion"); // You are not eligible to accept the promotion. If you can meet the eligibility before the acceptance period expires, you can still receive the promotion.
		return;
	}

	player->sendSystemMessage("@force_rank:promotion_accepted"); // You accept the promotion.
	promotePlayer(player);
	rankData->removeFromWinnerList(playerID);

	SortedVector<uint64>* rankList = rankData->getPlayerList();

	StringIdChatParameter mailBody("@force_rank:promotion_accepted_body"); // %TT has accepted a promotion into %TO.
	mailBody.setTO("@force_rank:rank" + String::valueOf(rank));
	mailBody.setTT(player->getFirstName());
	sendMailToList(rankList, "@force_rank:promotion_accepted_sub", mailBody);

	if (rankData->getTotalWinners() <= 0) {
		rankData->resetVotingData();
		rankData->setVoteStatus(VOTING_CLOSED);
		return;
	}

	if ((availSlots - 1) <= 0) {
		SortedVector<uint64>* winnerList = rankData->getWinnerList();

		StringIdChatParameter mailBody("@force_rank:vote_last_seat_taken_body"); // The last available seat for %TO has been filled. When another becomes available, you will be notified. As long as you remain eligible for %TO, you will be able to accept the promotion without a further vote.
		mailBody.setTO("@force_rank:rank" + String::valueOf(rank));
		sendMailToList(winnerList, "@force_rank:vote_last_seat_taken_sub", mailBody);

		player->sendSystemMessage(mailBody);
		rankData->setVoteStatus(VOTING_CLOSED);
	}
}

bool FrsManagerImplementation::isEligibleForPromotion(CreatureObject* player, int rank) {
	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == NULL)
		return false;

	FrsData* frsData = ghost->getFrsData();
	int councilType = frsData->getCouncilType();
	VectorMap<uint, Reference<FrsRankingData*> > rankingData;

	if (councilType == COUNCIL_LIGHT)
		rankingData = lightRankingData;
	else if (councilType == COUNCIL_DARK)
		rankingData = darkRankingData;
	else
		return false;

	Reference<FrsRankingData*> rankData = rankingData.get(rank);
	String rankSkill = rankData->getSkillName();

	SkillManager* skillManager = zoneServer->getSkillManager();

	if (skillManager == NULL)
		return false;

	return skillManager->fulfillsSkillPrerequisitesAndXp(rankSkill, player);
}

int FrsManagerImplementation::getVoteWeight(int playerRank, int voteRank) {
	// Unranked players are unable to vote
	if (playerRank < 1)
		return -1;

	if (voteRank < 1 || voteRank > 11)
		return -1;
	else if (voteRank == playerRank) // Votes count for double if the voter is in the same rank they are voting for
		return 2;
	else if (voteRank > 0 && voteRank < 5 && playerRank > 0 && playerRank < 5) // Players of rank 1-4 can participate in votes of rank 1-4
		return 1;
	else if (voteRank > 4 && voteRank < 8 && playerRank > 4 && playerRank < 8) // Players of rank 5-7 can participate in votes of rank 5-7
		return 1;
	else if ((voteRank == 8 || voteRank == 9) && (playerRank == 8 || playerRank == 9)) // Players of rank 8-9 can particpate in votes of rank 8-9
		return 1;
	else if ((voteRank == 10 && playerRank == 11) || (voteRank == 10 && playerRank == 11)) // Rank 10 can vote for 11, 11 can vote for 10
		return 1;
	else
		return -1;
}

bool FrsManagerImplementation::hasPlayerVoted(CreatureObject* player, int rank) {
	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == NULL)
		return true;

	FrsData* frsData = ghost->getFrsData();
	int councilType = frsData->getCouncilType();

	FrsRank* rankData = getFrsRank(councilType, rank);

	if (rankData == NULL)
		return false;

	return rankData->isOnVotedList(player->getObjectID());
}

int FrsManagerImplementation::getAvailableRankSlots(FrsRank* rankInfo) {
	short councilType = rankInfo->getCouncilType();
	int rank = rankInfo->getRank();

	VectorMap<uint, Reference<FrsRankingData*> > rankingData;

	if (councilType == COUNCIL_LIGHT)
		rankingData = lightRankingData;
	else if (councilType == COUNCIL_DARK)
		rankingData = darkRankingData;

	Reference<FrsRankingData*> rankData = rankingData.get(rank);

	if (rankData == NULL)
		return 0;

	return rankData->getPlayerCap() - rankInfo->getTotalPlayersInRank();
}

void FrsManagerImplementation::runVotingUpdate(FrsRank* rankInfo) {
	short councilType = rankInfo->getCouncilType();;
	int rank = rankInfo->getRank();

	ChatManager* chatManager = zoneServer->getChatManager();

	short status = rankInfo->getVoteStatus();
	int availSlots = getAvailableRankSlots(rankInfo);

	if (status == VOTING_CLOSED) {
		if (availSlots > 0) {
			SortedVector<uint64>* winnerList = rankInfo->getWinnerList();

			if (winnerList->size() > 0) {
				StringIdChatParameter mailBody("@force_rank:vote_seat_available_body"); // A council seat has become available for %TO. Since you won the last voting session, you may now claim this seat by going to the voting pedestal and selecting "Accept Promotion".
				mailBody.setTO("@force_rank:rank" + String::valueOf(rank));
				sendMailToList(winnerList, "@force_rank:vote_seat_available_sub", mailBody);

				rankInfo->setVoteStatus(WAITING);
			} else {
				rankInfo->setVoteStatus(PETITIONING);
			}

		}
	} else if (status == PETITIONING) {
		int totalPetitioners = rankInfo->getTotalPetitioners();

		if (totalPetitioners == 0) { // No one petitioned
			if (availSlots == 0) { // No slots available, so clear vote data and set back to voting closed
				rankInfo->resetVotingData();
				rankInfo->setVoteStatus(VOTING_CLOSED);
			}
			// Slots available, leave the status the same to run the petitioning phase again
		} else {
			if (totalPetitioners <= availSlots || totalPetitioners == 1) { // More open slots than petitioners, or only one petitioner so everyone wins
				rankInfo->setVoteStatus(WAITING);

				VectorMap<uint64, int>* petitionerList = rankInfo->getPetitionerList();
				ManagedReference<PlayerManager*> playerManager = zoneServer->getPlayerManager();

				for (int i = 0; i < petitionerList->size(); i++) {
					VectorMapEntry<uint64, int> entry = petitionerList->elementAt(i);
					uint64 petitionerID = entry.getKey();

					String name = playerManager->getPlayerName(petitionerID);

					if (name.isEmpty())
						continue;

					rankInfo->addToWinnerList(petitionerID);
					chatManager->sendMail("Enclave Records", "@force_rank:vote_win_sub", "@force_rank:vote_win_body", name);
				}
			} else { // Move to voting phase to determine who should be promoted
				rankInfo->setVoteStatus(VOTING_OPEN);

				StringIdChatParameter mailBody("@force_rank:vote_cycle_begun_body"); // Voting has started for promotions into %TO. It is part of your Enclave duties to cast your vote for the petitioner who you deem most worthy. Voting time remaining: %TT
				mailBody.setTO("@force_rank:rank" + String::valueOf(rank));
				mailBody.setTT(getTimeString(votingInterval / 1000));

				sendMailToVoters(councilType, rank, "@force_rank:vote_cycle_begun_sub", mailBody);
			}
		}
	} else if (status == VOTING_OPEN) {
		VectorMap<uint64, int>* petitionerList = rankInfo->getPetitionerList();

		if (petitionerList->size() == 0) { // Empty petitioner list. This shouldn't happen, but just in case.
			rankInfo->resetVotingData();
			rankInfo->setVoteStatus(VOTING_CLOSED);
		} else {
			if (availSlots > 0) { // Add top X (where X = available slots) winners to winner list so they can accept next phase
				Vector<uint64>* winnerList = getTopVotes(rankInfo, availSlots);

				for (int i = 0; i < winnerList->size(); i++) {
					rankInfo->addToWinnerList(winnerList->get(i));
				}

				StringIdChatParameter mailBody("@force_rank:vote_win_body"); // Your Enclave peers have decided that you are worthy of a promotion within the hierarchy. You should return to your Enclave as soon as possible and select "Accept Promotion" at the voting terminal.
				sendMailToList(winnerList, "@force_rank:vote_win_sub", mailBody);

				rankInfo->setVoteStatus(WAITING);
			} else { // No available slot, top winner will be auto promoted next time a slot opens
				Vector<uint64>* winnerList = getTopVotes(rankInfo, 1);
				rankInfo->addToWinnerList(winnerList->get(0));

				StringIdChatParameter mailBody("@force_rank:vote_win_no_slot_body"); // You have won the vote by your Enclave peers in order to achieve a higher ranking. Unforuntately, there are no longer any open seats for you to fill. As a result, you will be offered a chance to accept an open seat the next time one becomes available.
				sendMailToList(winnerList, "@force_rank:vote_win_sub", mailBody);

				rankInfo->setVoteStatus(VOTING_CLOSED); // Set status to closed without resetting voting data so that the winner will auto take the next available slot
			}

			checkForMissedVotes(rankInfo);
		}
	} else if (status == WAITING) {
		SortedVector<uint64>* winnerList = rankInfo->getWinnerList();

		if (winnerList->size() > 0) {
			StringIdChatParameter mailBody("@force_rank:acceptance_expired_body"); // Your deadline for accepting a promotion to %TO has passed. You will have to petition and then win another vote in order to achieve this rank.
			mailBody.setTO("@force_rank:rank" + String::valueOf(rank));
			sendMailToList(winnerList, "@force_rank:acceptance_expired_sub", mailBody);
		}

		rankInfo->resetVotingData();
		rankInfo->setVoteStatus(VOTING_CLOSED);
	}

	rankInfo->updateLastTick();
}

void FrsManagerImplementation::checkForMissedVotes(FrsRank* rankInfo) {
	ChatManager* chatManager = zoneServer->getChatManager();
	short councilType = rankInfo->getCouncilType();
	int rank = rankInfo->getRank();

	Vector<uint64> missedVoteList;

	for (int i = 1; i <= 11; i++) {
		int voteWeight = getVoteWeight(i, rank);

		if (voteWeight <= 0)
			continue;

		FrsRank* rankData = getFrsRank(councilType, i);

		if (rankData == NULL)
			continue;

		int votePenalty = missedVotePenalty * pow(i, 2);

		SortedVector<uint64>* playerList = rankData->getPlayerList();

		for (int j = 0; j < playerList->size(); j++) {
			uint64 playerID = playerList->get(j);

			if (!rankInfo->isOnVotedList(playerID)) {
				ManagedReference<CreatureObject*> player = zoneServer->getObject(playerList->get(j)).castTo<CreatureObject*>();

				if (player != NULL) {
					missedVoteList.add(playerID);
					addExperienceDebt(player, votePenalty);

					StringIdChatParameter mailBody("@force_rank:vote_missed_body"); // You have missed a promotion vote for %TO. As a result, you have lost %DI Force Ranking experience. (Note: This loss may not take effect until your next login.)
					mailBody.setTO("@force_rank:rank" + String::valueOf(rank));
					mailBody.setDI(votePenalty);
					chatManager->sendMail("Enclave Records", "@force_rank:vote_missed_sub", mailBody, player->getFirstName());
				}
			}
		}
	}
}

void FrsManagerImplementation::sendMailToVoters(short councilType, int rank, const String& sub, StringIdChatParameter& body) {
	ChatManager* chatManager = zoneServer->getChatManager();

	for (int i = 1; i <= 11; i++) {
		int voteWeight = getVoteWeight(i, rank);

		if (voteWeight <= 0)
			continue;

		FrsRank* rankData = getFrsRank(councilType, i);

		if (rankData == NULL)
			continue;

		SortedVector<uint64>* voterList = rankData->getPlayerList();

		sendMailToList(voterList, sub, body);
	}
}

void FrsManagerImplementation::sendMailToList(Vector<uint64>* playerList, const String& sub, StringIdChatParameter& body) {
	ChatManager* chatManager = zoneServer->getChatManager();
	ManagedReference<PlayerManager*> playerManager = zoneServer->getPlayerManager();

	for (int j = 0; j < playerList->size(); j++) {
		uint64 playerID = playerList->get(j);
		String name = playerManager->getPlayerName(playerID);

		if (name.isEmpty())
			continue;

		chatManager->sendMail("Enclave Records", sub, body, name);
	}
}

Vector<uint64>* FrsManagerImplementation::getTopVotes(FrsRank* rankInfo, int numWinners) {
	Vector<uint64>* winnerList = new Vector<uint64>();
	VectorMap<uint32, uint64> reverseList;
	VectorMap<uint64, int>* petitionerList = rankInfo->getPetitionerList();

	for (int i = 0; i < petitionerList->size(); i++) {
		VectorMapEntry<uint64, int>& entry = petitionerList->elementAt(i);
		uint64 petitionerID = entry.getKey();
		uint32 petitionerVotes = entry.getValue();
		reverseList.put(petitionerVotes, petitionerID);
	}

	for (int i = 0; i < numWinners; i++) {
		VectorMapEntry<uint32, uint64>& entry = reverseList.elementAt(i);
		uint64 petitionerID = entry.getValue();
		winnerList->add(petitionerID);
	}

	return winnerList;
}

String FrsManagerImplementation::getTimeString(uint64 timestamp) {
	String abbrvs[4] = {"seconds", "minutes", "hours", "days"};

	int intervals[4] = {1, 60, 3600, 86400};
	int values[4] = {0, 0, 0, 0};

	StringBuffer str;

	for (int i = 3; i > -1; --i) {
		values[i] = floor((float)(timestamp / intervals[i]));
		timestamp -= values[i] * intervals[i];

		if (values[i] > 0) {
			if (str.length() > 0)
				str << ", ";

			str << values[i] << " " << abbrvs[i];
		}
	}

	return str.toString();
}

short FrsManagerImplementation::getEnclaveType(BuildingObject* enclave) {
	if (enclave == NULL)
		return 0;

	if (enclave->getServerObjectCRC() == STRING_HASHCODE("object/building/yavin/light_enclave.iff"))
		return FrsManager::COUNCIL_LIGHT;
	else if (enclave->getServerObjectCRC() == STRING_HASHCODE("object/building/yavin/dark_enclave.iff"))
		return FrsManager::COUNCIL_DARK;

	return 0;
}

void FrsManagerImplementation::surrenderRankSkill(CreatureObject* player, const String& skillName) {
	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == NULL)
		return;

	FrsData* frsData = ghost->getFrsData();
	int council = frsData->getCouncilType();
	int rank = frsData->getRank();
	int newRank = rank - 1;

	Reference<FrsRankingData*> rankingData = NULL;

	if (council == COUNCIL_LIGHT)
		rankingData = lightRankingData.get(rank);
	else if (council == COUNCIL_DARK)
		rankingData = darkRankingData.get(rank);

	if (rankingData == NULL)
		return;

	if (skillName == rankingData->getSkillName()) {
		setPlayerRank(player, newRank, false);

		if (newRank < 0) {
			frsData->setCouncilType(0);
			ghost->setJediState(2);
		}
	}
}

void FrsManagerImplementation::recoverJediItems(CreatureObject* player) {
	PlayerObject* ghost = player->getPlayerObject();

	if (ghost == NULL)
		return;

	FrsData* frsData = ghost->getFrsData();
	int councilType = frsData->getCouncilType();
	int playerRank = frsData->getRank();
	String RankRobe = "";

	if (councilType == COUNCIL_LIGHT) {
		if (playerRank >= 10)
			RankRobe = "object/tangible/wearables/robe/robe_jedi_light_s05.iff";
		else if (playerRank >= 8)
			RankRobe = "object/tangible/wearables/robe/robe_jedi_light_s04.iff";
		else if (playerRank >= 5)
			RankRobe = "object/tangible/wearables/robe/robe_jedi_light_s03.iff";
		else if (playerRank >= 1)
			RankRobe = "object/tangible/wearables/robe/robe_jedi_light_s02.iff";
		else if (playerRank == 0)
			RankRobe = "object/tangible/wearables/robe/robe_jedi_light_s01.iff";
		else
			return;

	} else if (councilType == COUNCIL_DARK) {
		if (playerRank >= 10)
			RankRobe = "object/tangible/wearables/robe/robe_jedi_dark_s05.iff";
		else if (playerRank >= 8)
			RankRobe = "object/tangible/wearables/robe/robe_jedi_dark_s04.iff";
		else if (playerRank >= 5)
			RankRobe = "object/tangible/wearables/robe/robe_jedi_dark_s03.iff";
		else if (playerRank >= 1)
			RankRobe = "object/tangible/wearables/robe/robe_jedi_dark_s02.iff";
		else if (playerRank == 0)
			RankRobe = "object/tangible/wearables/robe/robe_jedi_dark_s01.iff";
		else
			return;
	}

	ManagedReference<SceneObject*> inventory = player->getSlottedObject("inventory");
	ZoneServer* zserv = player->getZoneServer();

	if (inventory->isContainerFullRecursive()) {
		player->sendSystemMessage("@jedi_spam:inventory_full_jedi_robe"); //	You have too many items in your inventory. In order to get your Padawan Robe you must clear out at least one free slot.
		return;
	}

	ManagedReference<SceneObject*> rankRobe = zserv->createObject(RankRobe.hashCode(), 1);
	rankRobe->sendTo(player, true);
	inventory->transferObject(rankRobe, -1, true);
}
