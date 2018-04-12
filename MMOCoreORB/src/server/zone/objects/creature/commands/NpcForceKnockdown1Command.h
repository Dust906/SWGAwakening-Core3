/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/

#ifndef NPCFORCEKNOCKDOWN1COMMAND_H_
#define NPCFORCEKNOCKDOWN1COMMAND_H_

#include "ForcePowersQueueCommand.h"

class NpcForceKnockdown1Command : public ForcePowersQueueCommand {
public:

	NpcForceKnockdown1Command(const String& name, ZoneProcessServer* server)
		: ForcePowersQueueCommand(name, server) {

	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {

		// not intended for player use
		if (creature->isPlayerCreature()){
			return GENERALERROR;
		}

		if (!checkStateMask(creature))
			return INVALIDSTATE;

		if (!checkInvalidLocomotions(creature))
			return INVALIDLOCOMOTION;

		if (isWearingArmor(creature)) {
			return NOJEDIARMOR;
		}

		ManagedReference<SceneObject*> targetObject = server->getZoneServer()->getObject(target);

		if (targetObject == NULL || !targetObject->isCreatureObject()) {
			return INVALIDTARGET;
		}

		return doCombatAction(creature, target);
	}

};

#endif //NPCFORCEKNOCKDOWN1COMMAND_H_
