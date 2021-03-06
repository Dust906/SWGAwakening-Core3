/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions. */

#ifndef SNOOPCOMMAND_H_
#define SNOOPCOMMAND_H_

#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/objects/player/sui/messagebox/SuiMessageBox.h"
#include "server/zone/objects/tangible/components/vendor/VendorDataComponent.h"
#include "server/zone/managers/mission/MissionManager.h"

#include "server/zone/managers/auction/AuctionManager.h"
#include "server/zone/managers/auction/AuctionsMap.h"
#include "server/zone/managers/director/ScreenPlayTask.h"

class SnoopCommand : public QueueCommand {
public:

	SnoopCommand(const String& name, ZoneProcessServer* server)
	: QueueCommand(name, server) {

	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {

		if (!checkStateMask(creature))
			return INVALIDSTATE;

		if (!checkInvalidLocomotions(creature))
			return INVALIDLOCOMOTION;

		PlayerObject* ghost = creature->getPlayerObject();

		if (ghost == NULL)
			return GENERALERROR;

		StringTokenizer args(arguments.toString());
		String targetName = "";
		String container = "";

		PlayerManager* playerManager = server->getZoneServer()->getPlayerManager();
		ManagedReference<CreatureObject*> targetObj = NULL;

		if (creature->getTargetID() != 0) {
			targetObj = server->getZoneServer()->getObject(creature->getTargetID()).castTo<CreatureObject*>();
		} else {
			if(!args.hasMoreTokens())
				return GENERALERROR;

			args.getStringToken(targetName);

			targetObj = playerManager->getPlayer(targetName);
		}

		if (args.hasMoreTokens())
			args.getStringToken(container);

		if (targetObj == NULL)
			return INVALIDTARGET;

		if (!targetObj->isCreatureObject())
			return INVALIDTARGET;

		if (container == "equipment") {
			targetObj->sendWithoutParentTo(creature);
			targetObj->openContainerTo(creature);
		} else if (container == "datapad") {
			SceneObject* creatureDatapad = targetObj->getSlottedObject("datapad");

			if (creatureDatapad == NULL)
				return GENERALERROR;

			creatureDatapad->sendWithoutParentTo(creature);
			creatureDatapad->openContainerTo(creature);
		}  else if (container == "bank") {
			SceneObject* creatureBank = targetObj->getSlottedObject("bank");

			if (creatureBank == NULL)
				return GENERALERROR;

			creatureBank->sendWithoutParentTo(creature);
			creatureBank->openContainerTo(creature);
		} else if (container == "skills") {
			ManagedReference<PlayerObject*> targetGhost = targetObj->getPlayerObject();
			Locker locker(targetObj);

			SkillManager* skillManager = SkillManager::instance();
			SkillList* skillList = targetObj->getSkillList();
                        StringBuffer body;

			if (skillList == NULL){
				return GENERALERROR;
			}

			body << "Player Name:\t" << targetObj->getFirstName();
			body << "\nSkills on Character:\n";
			String skillName = "";
			Vector<String> listOfNames;
			skillList->getStringList(listOfNames);
			SkillList copyOfList;
			copyOfList.loadFromNames(listOfNames);

					for (int i = 0; i < copyOfList.size(); i++) {
						Skill* skill = copyOfList.get(i);
						String skillName = skill->getSkillName();
						body <<  skillName << "\n";
					}
					ManagedReference<SuiMessageBox*> box = new SuiMessageBox(creature, 0);
					box->setPromptTitle("Skill Info");
					box->setPromptText(body.toString());
					box->setUsingObject(targetObj);
					box->setForceCloseDisabled();

					ghost->addSuiBox(box);
					creature->sendMessage(box->generateMessage());
		} else if (container == "credits") {
			ManagedReference<PlayerObject*> targetGhost = targetObj->getPlayerObject();
			int cash = targetObj->getCashCredits();
			int bank = targetObj->getBankCredits();
			StringBuffer body;

			body << "Player Name:\t" << targetObj->getFirstName();
			body << "\nCash Credits:\t" << String::valueOf(cash);
			body << "\nBank Credits:\t" << String::valueOf(bank);

			if (targetGhost != NULL)
				body << "\nBank Location:\t" << targetGhost->getBankLocation();

			ManagedReference<SuiMessageBox*> box = new SuiMessageBox(creature, SuiWindowType::ADMIN_PLAYER_CREDITS);
			box->setPromptTitle("Player Credits");
			box->setPromptText(body.toString());
			box->setUsingObject(targetObj);
			box->setForceCloseDisabled();

			ghost->addSuiBox(box);
			creature->sendMessage(box->generateMessage());
		} else if (container == "jeditrainer") {
			ManagedReference<PlayerObject*> targetGhost = targetObj->getPlayerObject();

			if (targetGhost->getJediState() < 2 || !targetObj->hasSkill("force_title_jedi_rank_02")) {
				creature->sendSystemMessage(targetObj->getFirstName() + " does not have a jedi state of 2+ or does not have the padawan skill box.");
				return GENERALERROR;
			}

			String planet = ghost->getTrainerZoneName();
			Vector3 coords = ghost->getTrainerCoordinates();

			creature->sendSystemMessage(targetObj->getFirstName() + "'s jedi trainer is located at " + coords.toString() + " on " + planet);
		} else if (container == "ham") {
			return sendHam(creature, targetObj);
		} else if (container == "lots") {
			return sendLots(creature, targetObj);
		} else if (container == "vendors") {
			return sendVendorInfo(creature, targetObj);
		} else if (container == "veteranrewards") {
			return sendVeteranRewardInfo(creature, targetObj);
		} else if(container == "faction") {
			return sendFactionInfo(creature, targetObj);
		} else if (container == "screenplaydata") {
			if (!args.hasMoreTokens()) {
				creature->sendSystemMessage("SYNTAX: /snoop [player] screenplaydata <screenplay> [variable]");
				return INVALIDPARAMETERS;
			}

			String playName, varName;
			args.getStringToken(playName);

			if (!args.hasMoreTokens()) {
				creature->sendSystemMessage("SYNTAX: /snoop [player] screenplaydata <screenplay> [variable]");
				return INVALIDPARAMETERS;
			}

			args.getStringToken(varName);

			PlayerObject* targetGhost = targetObj->getPlayerObject();

			if (targetGhost == NULL)
				return GENERALERROR;

			String result = targetGhost->getScreenPlayData(playName, varName);

			creature->sendSystemMessage(targetObj->getFirstName() + "'s screenplay data value for screenplay " + playName + " and variable " + varName + " is: " + result);
		} else if (container == "screenplaystate") {
			if (!args.hasMoreTokens()) {
				creature->sendSystemMessage("SYNTAX: /snoop [player] screenplaystate <stateName> [state]");
				return INVALIDPARAMETERS;
			}
			String stateName;
			args.getStringToken(stateName);

			uint64 state = targetObj->getScreenPlayState(stateName);
			if (args.hasMoreTokens()) {
				uint64 stateToCheck = args.getIntToken();
				if (state & stateToCheck)
					creature->sendSystemMessage(targetObj->getFirstName() + " state check of '" + String::valueOf(stateToCheck) + "' for screenplayState '" + stateName + "': TRUE.");
				else
					creature->sendSystemMessage(targetObj->getFirstName() + " state check of '" + String::valueOf(stateToCheck) + "' for screenplayState '" + stateName + "': FALSE.");
			} else {
				creature->sendSystemMessage(targetObj->getFirstName() + " state check for screenplayState '" + stateName + "': " + String::valueOf(state) + ".");
			}
		} else if (container == "activescreenplay") {
			String key = String::valueOf(targetObj->getObjectID()) + ":activeScreenPlay";
			String data = DirectorManager::instance()->getStringSharedMemory(key);
			creature->sendSystemMessage(targetObj->getFirstName() + " active screenplay: " + data);
		} else if (container == "luaevents") {
			return sendLuaEvents(creature, targetObj);
		} else if (container == "buffs") {
			return sendBuffs(creature, targetObj);
		} else if (container == "visibility") {
			MissionManager* missionManager = creature->getZoneServer()->getMissionManager();

			if (missionManager->sendPlayerBountyDebug(creature, targetObj))
				return SUCCESS;
			else
				return GENERALERROR;
		} else {
			SceneObject* creatureInventory = targetObj->getSlottedObject("inventory");

			if (creatureInventory == NULL)
				return GENERALERROR;

			creatureInventory->sendWithoutParentTo(creature);
			creatureInventory->openContainerTo(creature);
		}



		return SUCCESS;
	}

	int sendLuaEvents(CreatureObject* creature, CreatureObject* target) const {
		ManagedReference<PlayerObject*> ghost = creature->getPlayerObject();

		if (ghost == NULL)
			return GENERALERROR;

		Vector<Reference<ScreenPlayTask*> > eventList = DirectorManager::instance()->getObjectEvents(target);

		ManagedReference<SuiListBox*> box = new SuiListBox(creature, 0);
		box->setPromptTitle("LUA Events");
		Time currentTime;
		box->setPromptText("Below are the LUA Events currently scheduled for the player.\n\nCurrent server time: " + currentTime.getFormattedTime());
		box->setUsingObject(target);
		box->setForceCloseDisabled();

		for (int i = 0; i < eventList.size(); i++) {
			Reference<ScreenPlayTask*> task = eventList.get(i);

			if (task == NULL)
				continue;

			String buffer = task->getScreenPlay() + ":" + task->getTaskKey();
			String args = task->getArgs();

			if (args != "")
				buffer += " (Args: " + args + ")";

			Time nextExecutionTime;
			Core::getTaskManager()->getNextExecutionTime(task, nextExecutionTime);
			uint64 miliDiff = nextExecutionTime.miliDifference();

			buffer += ", Execution (server time): " + nextExecutionTime.getFormattedTime() + " (" + getTimeString(-miliDiff) + " from now)";

			box->addMenuItem(buffer);
		}

		ghost->addSuiBox(box);
		creature->sendMessage(box->generateMessage());

		return SUCCESS;
	}

	String getTimeString(uint64 timestamp) const {
		int seconds = timestamp / 1000;

		int hours = seconds / 3600;
		seconds -= hours * 3600;

		int minutes = seconds / 60;
		seconds -= minutes * 60;

		StringBuffer buffer;

		if (hours > 0)
			buffer << hours << "h ";

		if (minutes > 0)
			buffer << minutes << "m ";

		if (seconds > 0)
			buffer << seconds << "s";

		return buffer.toString();
	}

	int sendVeteranRewardInfo(CreatureObject* creature, CreatureObject* target) const {
		ManagedReference<PlayerObject*> targetGhost = target->getPlayerObject();
		ManagedReference<PlayerObject*> ghost = creature->getPlayerObject();
		PlayerManager* playerManager = server->getZoneServer()->getPlayerManager();

		if (targetGhost == NULL || ghost == NULL || playerManager == NULL)
			return GENERALERROR;

		StringBuffer body;
		body << "Player Name:\t" << target->getFirstName() << endl;
		body << "Claimed Rewards:" << endl;
		body << "\tMilestone\tReward"<< endl;
		for( int i = 0; i < playerManager->getNumVeteranRewardMilestones(); i++ ){
			int milestone = playerManager->getVeteranRewardMilestone(i);
			body << "\t" << String::valueOf(milestone);
			String claimedReward = targetGhost->getChosenVeteranReward(milestone);
			if( claimedReward.isEmpty() ){
				body << "\t\t" << "Unclaimed" << endl;
			}
			else{
				body << "\t\t" << claimedReward << endl;
			}
		}

		ManagedReference<SuiMessageBox*> box = new SuiMessageBox(creature, 0);
		box->setPromptTitle("Veteran Reward Info");
		box->setPromptText(body.toString());
		box->setUsingObject(target);
		box->setForceCloseDisabled();

		ghost->addSuiBox(box);
		creature->sendMessage(box->generateMessage());

		return SUCCESS;

	}

	int sendFactionInfo(CreatureObject* creature, CreatureObject* target) const {
		ManagedReference<PlayerObject*> targetGhost = target->getPlayerObject();
		ManagedReference<PlayerObject*> ghost = creature->getPlayerObject();
		PlayerManager* playerManager = server->getZoneServer()->getPlayerManager();

		if (targetGhost == NULL || ghost == NULL || playerManager == NULL)
			return GENERALERROR;

		StringBuffer body;
		body << "Player Name:\t" << target->getFirstName() << endl;
		body << "Affiliation:\t";

		if (target->isImperial())
			body << "Imperial";
		else if (target->isRebel())
			body << "Rebel";
		else
			body << "Neutral" << endl;

		int rank = 0;

		if (target->isImperial() || target->isRebel()) {
			if (target->getFactionStatus() == FactionStatus::ONLEAVE)
				body << " (On Leave)" << endl;
			else if (target->getFactionStatus() == FactionStatus::OVERT)
				body << " (Overt)" << endl;
			else if (target->getFactionStatus() == FactionStatus::COVERT)
				body << " (Covert)" << endl;

			rank = target->getFactionRank();
			body << "Rank:\t" << FactionManager::instance()->getRankName(rank) << " (Rank " << rank + 1 << ")" << endl;
		}
		body << "Imperial Points:\t" << targetGhost->getFactionStanding("imperial") << " (Max: " << (target->isImperial() ? FactionManager::instance()->getFactionPointsCap(rank) : 1000) << ")" << endl;
		body << "Rebel Points:\t" << targetGhost->getFactionStanding("rebel") << " (Max: " << (target->isRebel() ? FactionManager::instance()->getFactionPointsCap(rank) : 1000) << ")" << endl;

		ManagedReference<SuiMessageBox*> box = new SuiMessageBox(creature, 0);
		box->setPromptTitle("Faction Information");
		box->setPromptText(body.toString());
		box->setUsingObject(target);
		box->setForceCloseDisabled();

		ghost->addSuiBox(box);
		creature->sendMessage(box->generateMessage());

		return SUCCESS;

	}


	int sendVendorInfo(CreatureObject* creature, CreatureObject* target) const {
		ManagedReference<PlayerObject*> targetGhost = target->getPlayerObject();
		ManagedReference<PlayerObject*> ghost = creature->getPlayerObject();
		ManagedReference<AuctionManager*> auctionManager = server->getZoneServer()->getAuctionManager();

		if (targetGhost == NULL || ghost == NULL || auctionManager == NULL)
			return GENERALERROR;

		ManagedReference<AuctionsMap*> auctionsMap = auctionManager->getAuctionMap();
		if(auctionsMap == NULL)
			return GENERALERROR;

		StringBuffer body;
		body << "Player Name:\t" << target->getFirstName() << endl;
		body << "Max # of vendors:\t" << target->getSkillMod("manage_vendor") << endl;
		body << "Max # of items:\t" << target->getSkillMod("vendor_item_limit") << endl;
		body << "Total # of items:\t" << auctionsMap->getPlayerItemCount(target) << endl << endl;
		body << "Vendors:" << endl;

		SortedVector<unsigned long long>* ownedVendors = targetGhost->getOwnedVendors();
		for (int i = 0; i < ownedVendors->size(); i++) {
			ManagedReference<SceneObject*> vendor = creature->getZoneServer()->getObject(ownedVendors->elementAt(i));

			int num = i + 1;
			body << endl << String::valueOf(num) << ". ";

			if (vendor == NULL) {
				body << "NULL Vendor" << endl << endl;
				continue;
			}

			body << "VendorID:\t" << vendor->getObjectID() << endl;

			DataObjectComponentReference* data = vendor->getDataObjectComponent();
			if(data == NULL || data->get() == NULL || !data->get()->isVendorData()) {
				body << "    NULL Data Component" << endl << endl;
				continue;
			}

			VendorDataComponent* vendorData = cast<VendorDataComponent*>(data->get());
			if(vendorData == NULL) {
				body << "    NULL Vendor Data Component" << endl << endl;
				continue;
			}

			bool init = false;
			if (vendorData->isInitialized())
				init = true;

			body << "    Initialized?\t" << (init ? "Yes" : "No");
			body << endl << "    # of items:\t" << auctionsMap->getVendorItemCount(vendor) << endl;

			body << "    ParentID:\t";

			ManagedReference<SceneObject*> parent = vendor->getParent().get();
			if (parent == NULL)
				body << "NULL" << endl;
			else
				body << parent->getObjectID() << endl;

			body << "    Zone:\t";

			Zone* zone = vendor->getZone();
			if (zone == NULL) {
				body << "NULL" << endl;
			} else {
				body << zone->getZoneName() << endl;
				body << "    World Position:\t" << vendor->getWorldPositionX() << ", " << vendor->getWorldPositionY() << endl;
			}

		}

		ManagedReference<SuiMessageBox*> box = new SuiMessageBox(creature, 0);

		box->setPromptTitle("Vendor Info");
		box->setPromptText(body.toString());
		box->setUsingObject(target);
		box->setForceCloseDisabled();

		ghost->addSuiBox(box);
		creature->sendMessage(box->generateMessage());

		return SUCCESS;
	}

	int sendLots(CreatureObject* creature, CreatureObject* target) const {
		ManagedReference<PlayerObject*> targetGhost = target->getPlayerObject();
		ManagedReference<PlayerObject*> ghost = creature->getPlayerObject();

		if (targetGhost == NULL || ghost == NULL)
			return GENERALERROR;

		int lotsRemaining = targetGhost->getLotsRemaining();

		StringBuffer body;

		body << "Player Name:\t" << target->getFirstName() << endl;
		body << "Unused Lots:\t" << String::valueOf(lotsRemaining) << endl << endl;
		body << "Player Structures:";

		for (int i = 0; i < targetGhost->getTotalOwnedStructureCount(); i++) {
			ManagedReference<StructureObject*> structure = creature->getZoneServer()->getObject(targetGhost->getOwnedStructure(i)).castTo<StructureObject*>();

			int num = i + 1;
			body << endl << String::valueOf(num) << ". ";

			if (structure == NULL) {
				body << "NULL Structure" << endl << endl;
				continue;
			}

			body << "StructureID:\t" << structure->getObjectID() << endl;
			body << "    Name:\t" << structure->getCustomObjectName().toString() << endl;
			body << "    Type:\t" << structure->getObjectTemplate()->getFullTemplateString() << endl;
			body << "    Lots:\t" << String::valueOf(structure->getLotSize()) << endl;

			body << "    Zone:\t";
			Zone* zone = structure->getZone();
			if (zone == NULL) {
				body << "NULL" << endl;
			} else {
				body << zone->getZoneName() << endl;
				body << "    World Position:\t" << structure->getWorldPositionX() << ", " << structure->getWorldPositionY() << endl;
			}
		}

		ManagedReference<SuiMessageBox*> box = new SuiMessageBox(creature, 0);
		box->setPromptTitle("Player Lots");
		box->setPromptText(body.toString());
		box->setUsingObject(target);
		box->setForceCloseDisabled();

		ghost->addSuiBox(box);
		creature->sendMessage(box->generateMessage());

		return SUCCESS;
	}

	int sendHam(CreatureObject* creature, CreatureObject* target) const {
		ManagedReference<PlayerObject*> ghost = creature->getPlayerObject();
		ManagedReference<PlayerObject*> targetGhost = target->getPlayerObject();

		if (ghost == NULL) {
			return GENERALERROR;
		}

		StringBuffer body;
		body << "Player Name:\t" << target->getFirstName() << endl << endl;
		body << "Min\tMax \tWounds\tMods\tEnc" << endl;

		for (int i = 0; i < 9; i++) {
			body << String::valueOf(target->getHAM(i)) + " / ";
			body << "\t" + String::valueOf(target->getMaxHAM(i) - target->getWounds(i));
			body << "   \t" + String::valueOf(target->getWounds(i)) << "\t";

			switch (i) {
			case 0:
			case 3:
			case 6:
				body << "\t" + String::valueOf(target->getMaxHAM(i) - target->getBaseHAM(i));
				body << "\t0" << endl;
				break;
			default:
				body << "\t" + String::valueOf(target->getMaxHAM(i) - target->getBaseHAM(i) + target->getEncumbrance(i / 3));
				body << "\t" + String::valueOf(target->getEncumbrance(i / 3)) << endl;
				break;
			}
		}

		if(targetGhost != NULL) { // if we're not a PlayerObject, we don't hold force values
			body << "Force Power:\t" << targetGhost->getForcePower() << " / " << targetGhost->getForcePowerMax() << endl;
			body << "Force Regen:\t" << target->getSkillMod("jedi_force_power_regen") << endl;
		}
		ManagedReference<SuiMessageBox*> box = new SuiMessageBox(creature, 0);
		box->setPromptTitle("Player HAM");
		box->setPromptText(body.toString());
		box->setUsingObject(target);
		box->setForceCloseDisabled();

		ghost->addSuiBox(box);
		creature->sendMessage(box->generateMessage());

		return SUCCESS;
	}

	int sendBuffs(CreatureObject* creature, CreatureObject* target) const {
		ManagedReference<PlayerObject*> ghost = creature->getPlayerObject();

		if (ghost == NULL) {
			return GENERALERROR;
		}

		BuffList* bList = target->getBuffList();
		if (bList == NULL || bList->getBuffListSize() == 0) {
			creature->sendSystemMessage("No Buffs to Display.");
			return SUCCESS;
		}

		StringBuffer buffText;

		for (int i = 0; i < bList->getBuffListSize(); i++) {
			Buff* buff = bList->getBuffByIndex(i);
			buffText << buff->getBuffName() << ":" <<endl;
			buffText << "\tCRC: 0x" << hex << buff->getBuffCRC() << endl;

			Vector<uint64>* secondaryCRCs = buff->getSecondaryBuffCRCs();
			if (secondaryCRCs != NULL && secondaryCRCs->size() > 0) {
				buffText << "\tSecondary CRCs: "<< endl;
				for (int j = 0; j < secondaryCRCs->size(); j++) {
					buffText << "\t\t 0x" << hex << buff->getSecondaryBuffCRCs() << endl;
				}
			}

			buffText << "\tDuration (" << buff->getBuffDuration() << ") Time Left (" << buff->getTimeLeft() << ")" << endl;

			buffText << "\tAttribute Mods: " << buff->getAttributeModifierString() << endl;
			buffText << "\tSkill Mods: " << buff->getSkillModifierString() << endl;
		}

		ManagedReference<SuiMessageBox*> box = new SuiMessageBox(creature, 0);
		box->setPromptTitle("Player HAM");
		box->setPromptText(buffText.toString());
		box->setUsingObject(target);
		box->setForceCloseDisabled();

		ghost->addSuiBox(box);
		creature->sendMessage(box->generateMessage());

		return SUCCESS;
	}
};

#endif //SNOOPCOMMAND_H_
