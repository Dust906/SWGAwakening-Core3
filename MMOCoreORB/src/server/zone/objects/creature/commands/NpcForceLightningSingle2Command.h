/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/

#ifndef NPCFORCELIGHTNINGSINGLE2COMMAND_H_
#define NPCFORCELIGHTNINGSINGLE2COMMAND_H_

#include "ForcePowersQueueCommand.h"

class NpcForceLightningSingle2Command : public ForcePowersQueueCommand {
public:

	NpcForceLightningSingle2Command(const String& name, ZoneProcessServer* server)
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

#endif //NPCFORCELIGHTNINGSINGLE2COMMAND_H_
