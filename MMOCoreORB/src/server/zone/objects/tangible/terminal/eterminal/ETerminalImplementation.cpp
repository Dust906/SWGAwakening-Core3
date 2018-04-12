#include "conf/ServerSettings.h"
#include "server/zone/objects/tangible/terminal/eterminal/ETerminal.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/creature/ai/AiAgent.h"
#include "server/zone/objects/player/sui/eterminalbox/SuiETerminalBox.h"
#include "templates/tangible/ETerminalTemplate.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/managers/player/PlayerManager.h"

void ETerminalImplementation::loadTemplateData(SharedObjectTemplate* templateData) {
	TangibleObjectImplementation::loadTemplateData(templateData);

	ETerminalTemplate* terminalData = dynamic_cast<ETerminalTemplate*>(templateData);

	if (terminalData == NULL)
		return;

	rootNode = terminalData->getItemList();

	//info("loaded " + String::valueOf(itemList.size()));
}

void ETerminalImplementation::initializeTransientMembers() {
	TerminalImplementation::initializeTransientMembers();

	setLoggingName("ETerminal");
}

int ETerminalImplementation::handleObjectMenuSelect(CreatureObject* player, byte selectedID) {
	//info("entering start terminal radial call", true);

	if (selectedID != 20) // not use object
		return 1;

	sendInitialChoices(player);

	return 0;
}

void ETerminalImplementation::sendInitialChoices(CreatureObject* player) {
	//info("entering sendInitialChoices", true);

	if (rootNode == NULL) {
		player->sendSystemMessage("There was an error initializing the menu for this enhancement terminal. Sorry for the inconvenience.");
		return;
	}

	if (ServerSettings::instance()->getEnhancementTerminalEnabled()) {
		ManagedReference<SuiETerminalBox*> sui = new SuiETerminalBox(player, rootNode);
		sui->setUsingObject(_this.getReferenceUnsafeStaticCast());

		player->sendMessage(sui->generateMessage());
		player->getPlayerObject()->addSuiBox(sui);
	} else {
		player->sendSystemMessage("The Enhancement Terminal is disabled.");
	}
}

void ETerminalImplementation::terminalEnhanceCharacter(CreatureObject* player) {
	PlayerManager* pm = player->getZoneServer()->getPlayerManager();

	pm->terminalEnhanceCharacter(player);

	ManagedReference<PlayerObject*> ghost = player->getPlayerObject();

	if (ghost == NULL)
		return;

	for (int i = 0; i < ghost->getActivePetsSize(); i++) {
		ManagedReference<AiAgent*> pet = ghost->getActivePet(i);

		if (pet != NULL) {
			Locker crossLocker(pet, player);

			pm->terminalEnhanceCharacter(pet);
		}
	}
}
