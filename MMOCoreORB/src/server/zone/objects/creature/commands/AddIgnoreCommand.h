/*
				Copyright <SWGEmu>
		See file COPYING for copying conditions.*/

#ifndef ADDIGNORECOMMAND_H_
#define ADDIGNORECOMMAND_H_

#include "server/zone/objects/player/PlayerObject.h"
#include "server/chat/StringIdChatParameter.h"
#include "server/zone/managers/player/PlayerManager.h"

class AddIgnoreCommand : public QueueCommand {
public:

	AddIgnoreCommand(const String& name, ZoneProcessServer* server)
		: QueueCommand(name, server) {

	}

	int doQueueCommand(CreatureObject* creature, const uint64& target, const UnicodeString& arguments) const {

		if (!checkStateMask(creature))
			return INVALIDSTATE;

		if (!checkInvalidLocomotions(creature))
			return INVALIDLOCOMOTION;

		if (!creature->isPlayerCreature())
			return GENERALERROR;

		String nameLower = arguments.toString().toLowerCase();

		Reference<PlayerObject*> ghost = creature->getSlottedObject("ghost").castTo<PlayerObject*>();

		if (ghost->isIgnoring(nameLower)) {
			StringIdChatParameter param("cmnty", "ignore_duplicate");
			param.setTT(nameLower);
			creature->sendSystemMessage(param);

			return GENERALERROR;
		}

		PlayerManager* playerManager = server->getPlayerManager();

		bool canIgnoreSelf = false;
		String nameParam = nameLower;
		try{
			Database::escapeString(nameParam);
			String query = "SELECT firstname FROM self_ignore_permits WHERE LOWER(firstname) = \""
					+ nameParam + "\"";
			Reference<ResultSet*> resultSet = ServerDatabase::instance()->executeQuery(query);
			canIgnoreSelf = resultSet->next();
		} catch (DatabaseException& e){
			canIgnoreSelf = false;
		}

		bool validName = (playerManager->existsName(nameLower)
				&& (creature->getFirstName().toLowerCase().compareTo( nameLower ) != 0)) || canIgnoreSelf;


		if (!validName) {
			StringIdChatParameter param("cmnty", "ignore_not_found");
			param.setTT(nameLower);
			creature->sendSystemMessage(param);

			return GENERALERROR;
		}

		ghost->addIgnore(nameLower, true);

		return SUCCESS;
	}

};

#endif //ADDIGNORECOMMAND_H_
