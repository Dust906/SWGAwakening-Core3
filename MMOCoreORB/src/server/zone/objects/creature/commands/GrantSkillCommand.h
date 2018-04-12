/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/

#ifndef GRANTSKILLCOMMAND_H_
#define GRANTSKILLCOMMAND_H_

#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/managers/skill/SkillManager.h"

#include "server/zone/managers/frs/FrsManager.h"
#include "templates/faction/Factions.h"

class GrantSkillCommand : public QueueCommand {
public:

	GrantSkillCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {
		if (!checkStateMask(creature))
			return INVALIDSTATE;

		if (!checkInvalidLocomotions(creature))
			return INVALIDLOCOMOTION;

		ManagedReference<SceneObject*> object = server->getZoneServer()->getObject(target);

		if (object == NULL || !object->isCreatureObject())
			return INVALIDTARGET;

		CreatureObject* targetCreature = cast<CreatureObject*>( object.get());

		Locker clocker(targetCreature, creature);

		SkillManager* skillManager = SkillManager::instance();
		skillManager->surrenderSkill(arguments.toString(), targetCreature, true, false);
		bool skillGranted = skillManager->awardSkill(arguments.toString(), targetCreature, true, true, true);

		if (skillGranted) {
			StringIdChatParameter params;
			params.setTO(arguments.toString());
			params.setStringId("skill_teacher", "skill_terminal_grant");

			targetCreature->sendSystemMessage(params);

			creature->sendSystemMessage("Granted skill " + arguments.toString()
					+ "to " + targetCreature->getFirstName());

			PlayerObject* targetGhost = targetCreature->getPlayerObject();

			if (targetGhost != nullptr) {
				if (arguments.toString() == "force_title_jedi_rank_03") {
					ManagedReference<FrsManager*> frsManager = creature->getZoneServer()->getFrsManager();
					FrsData* frsData = targetGhost->getFrsData();

					if (frsManager == nullptr)
						return false;

					if (creature->getFaction() == Factions::FACTIONREBEL)
						frsData->setCouncilType(1);
					else if (creature->getFaction() == Factions::FACTIONIMPERIAL)
						frsData->setCouncilType(2);
					else
						return false;

					Locker locker(frsManager);
					frsManager->setPlayerRank(creature, 0, true);
				}
			}
		} else {
			StringIdChatParameter params;
			params.setTO(arguments.toString());
			params.setStringId("skill_teacher", "prose_train_failed");

			targetCreature->sendSystemMessage(params);

			creature->sendSystemMessage("Failed to grant skill " + arguments.toString()
					+ "to " + targetCreature->getFirstName());
		}
		return SUCCESS;
	}

};

#endif //GRANTSKILLCOMMAND_H_
