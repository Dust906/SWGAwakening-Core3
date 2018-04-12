/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/

#ifndef NPCFORCELIGHTNINGSINGLE1COMMAND_H_
#define NPCFORCELIGHTNINGSINGLE1COMMAND_H_

#include "ForcePowersQueueCommand.h"

class NpcForceLightningSingle1Command : public ForcePowersQueueCommand {
public:

	NpcForceLightningSingle1Command(const String& name, ZoneProcessServer* server)
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

#endif //NPCFORCELIGHTNINGSINGLE1COMMAND_H_
