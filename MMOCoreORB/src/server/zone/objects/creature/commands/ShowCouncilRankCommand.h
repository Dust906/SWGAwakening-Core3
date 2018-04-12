#ifndef SHOWCOUNCILRANKCOMMAND_H_
#define SHOWCOUNCILRANKCOMMAND_H_

#include "server/zone/managers/frs/FrsManager.h"

class ShowCouncilRankCommand : public QueueCommand {
public:

	ShowCouncilRankCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {

		if (!checkStateMask(creature))
			return INVALIDSTATE;

		if (!checkInvalidLocomotions(creature))
			return INVALIDLOCOMOTION;

		PlayerObject* ghost = creature->getPlayerObject();

		if (ghost == NULL) {
			return GENERALERROR;
		}

		if (!creature->hasSkill("force_title_jedi_rank_03")) {
			creature->sendSystemMessage("You must be atleast a Jedi Knight to use this command.");
			return GENERALERROR;
		}

		ManagedReference<CreatureObject*> targetObj;
		ZoneServer* zoneServer = server->getZoneServer();

		targetObj = zoneServer->getObject(creature->getTargetID()).castTo<CreatureObject*>();

		if (targetObj == NULL || !targetObj->isPlayerCreature()) {
			creature->sendSystemMessage("Invalid target. This command only works on players");
			return INVALIDTARGET;
		}

		PlayerObject* targetGhost = targetObj->getPlayerObject();

		if (targetGhost == NULL) {
			return GENERALERROR;
		}

		FrsManager* frsManager = zoneServer->getFrsManager();
		FrsData* frsData = targetGhost->getFrsData();

		int council = frsData->getCouncilType();
		int rank = frsData->getRank();

		Locker locker(targetObj, creature);

		String rankString;
		StringBuffer msg;

		if (council == 0) {
			creature->sendSystemMessage(targetObj->getDisplayedName() + " is not in the Force Ranking System.");
			return INVALIDTARGET;
		} else if (council == 1) {
			if (rank == 1) {
				rankString = "Sentinel I";
			} else if (rank == 2) {
				rankString = "Sentinel II";
			} else if (rank == 3) {
				rankString = "Sentinel III";
			} else if (rank == 4) {
				rankString = "Sentinel IV";
			} else if (rank == 5) {
				rankString = "Consular I";
			} else if (rank == 6) {
				rankString = "Consular II";
			} else if (rank == 7) {
				rankString = "Consular III";
			} else if (rank == 8) {
				rankString = "Arbiter I";
			} else if (rank == 9) {
				rankString = "Arbiter II";
			} else if (rank == 10) {
				rankString = "Council Member";
			} else if (rank == 11) {
				rankString = "Council Leader";
			} else {
				rankString = "Rank Member";
			}
		} else if (council == 2) {
			if (rank == 1) {
				rankString = "Enforcer I";
			} else if (rank == 2) {
				rankString = "Enforcer II";
			} else if (rank == 3) {
				rankString = "Enforcer III";
			} else if (rank == 4) {
				rankString = "Enforcer IV";;
			} else if (rank == 5) {
				rankString = "Templar I";
			} else if (rank == 6) {
				rankString = "Templar II";
			} else if (rank == 7) {
				rankString = "Templar III";
			} else if (rank == 8) {
				rankString = "Oppressor I";
			} else if (rank == 9) {
				rankString = "Oppressor II";
			} else if (rank == 10) {
				rankString = "Council Member";
			} else if (rank == 11) {
				rankString= "Council Leader";
			} else {
				rankString = "Rank Member";
			}
		}
		ManagedReference<SuiMessageBox*> box = new SuiMessageBox(creature, SuiWindowType::NONE);
		box->setPromptTitle(rankString);
		msg << targetObj->getDisplayedName() << " has the rank of " << rankString << "." << endl;
		msg << endl;
		if (rank >= 1) {
			FrsRank* rankData = frsManager->getFrsRank(council, rank);
			int availSeats = frsManager->getAvailableRankSlots(rankData);

			if (availSeats < 0)
				availSeats = 0;

			msg << "Seats Available for " << rankString << ": " << availSeats << endl;
			msg << endl;
		}

		box->setPromptText(msg.toString());
		ghost->addSuiBox(box);
		creature->sendMessage(box->generateMessage());

	return SUCCESS;
	}
};

#endif //SHOWCOUNCILRANKCOMMAND_H_
