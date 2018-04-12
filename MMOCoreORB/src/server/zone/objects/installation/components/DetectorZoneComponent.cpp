/*
 * DetectorZoneComponent.cpp
 *
 *  Created on: Dec 17, 2012
 *      Author: root
 */

#include "DetectorZoneComponent.h"
#include "DetectorDataComponent.h"
#include "server/zone/objects/creature/CreatureObject.h"
#include "server/zone/objects/player/PlayerObject.h"
#include "server/zone/objects/scene/SceneObject.h"
#include "server/zone/packets/scene/PlayClientEffectLocMessage.h"
#include "server/zone/Zone.h"

#include "conf/ServerSettings.h"
#include "server/zone/objects/player/FactionStatus.h"

void DetectorZoneComponent::notifyPositionUpdate(SceneObject* sceneObject, QuadTreeEntry* entry) const {
	ManagedReference<SceneObject*> target = cast<SceneObject*>(entry);
	if (!sceneObject->isTangibleObject() || target == NULL){
		return;
	}

	ManagedReference<TangibleObject*> tano = cast<TangibleObject*>(sceneObject);

	DataObjectComponentReference* ref = sceneObject->getDataObjectComponent();
	if (ref == NULL){
		info("data is null",true);
		return;
	}

	DetectorDataComponent* scannerData = cast<DetectorDataComponent*>(ref->get());

	if (scannerData == NULL || !scannerData->canScan())
		return;

	if (sceneObject->isDetector() && target->isPlayerCreature() && sceneObject->isInRange(target,65)){
		ManagedReference<CreatureObject*> player = cast<CreatureObject*>(entry);
		if (player == NULL)
			return;

		if (tano->getFaction() != player->getFaction() && player->getFaction() != 0 ) {
			scannerData->updateCooldown();
			PlayClientEffectLoc* explodeLoc = new PlayClientEffectLoc("clienteffect/survey_effect.cef", tano->getZone()->getZoneName(), tano->getPositionX(), tano->getPositionZ(), tano->getPositionY());
			tano->broadcastMessage(explodeLoc, false);

			if (ServerSettings::instance()->getOvertScannersEnabled())
				player->setFactionStatus(FactionStatus::OVERT);
		}

	}

	return;
}
