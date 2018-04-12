/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/

#ifndef NPCFORCELIGHTNINGCONE2COMMAND_H_
#define NPCFORCELIGHTNINGCONE2COMMAND_H_

#include "ForcePowersQueueCommand.h"

class NpcForceLightningCone2Command : public ForcePowersQueueCommand {
public:

	NpcForceLightningCone2Command(const String& name, ZoneProcessServer* server)
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

		return doCombatAction(creature, target);
	}

};

#endif //NPCFORCELIGHTNINGCONE2COMMAND_H_
