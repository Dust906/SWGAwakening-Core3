/*
 				Copyright <SWGEmu>
		See file COPYING for copying conditions. */

#ifndef THROWTRAPCOMMAND_H_
#define THROWTRAPCOMMAND_H_

#include "server/zone/objects/creature/events/ThrowTrapTask.h"
#include "templates/tangible/TrapTemplate.h"

#include "server/zone/managers/collision/CollisionManager.h"

class ThrowTrapCommand: public CombatQueueCommand {
public:

	ThrowTrapCommand(const String& name, ZoneProcessServer* server) :
		CombatQueueCommand(name, server) {

	}

	int doQueueCommand(CreatureObject* creature, const uint64& target,
			const UnicodeString& arguments) const {

		if (!checkStateMask(creature))
			return INVALIDSTATE;

		if (!checkInvalidLocomotions(creature))
			return INVALIDLOCOMOTION;

		int skillLevel = creature->getSkillMod("trapping");
		if (skillLevel < 1 || !creature->hasSkill("outdoors_scout_novice")) {
			creature->sendSystemMessage("@trap/trap:trap_no_skill");
			return GENERALERROR;
		}

		StringTokenizer tokenizer(arguments.toString());

		if (!tokenizer.hasMoreTokens())
			return INVALIDPARAMETERS;

		try {

			uint64 trapId = tokenizer.getLongToken();
			ManagedReference<TangibleObject*> trap =
					server->getZoneServer()->getObject(trapId).castTo<TangibleObject*>();

			if (trap == NULL)
				return INVALIDPARAMETERS;

			if (!trap->isTrapObject())
				return INVALIDPARAMETERS;

			if (!trap->isASubChildOf(creature))
				return GENERALERROR;

			ManagedReference<CreatureObject*> targetCreature =
					server->getZoneServer()->getObject(target).castTo<CreatureObject*>();

			if (targetCreature == NULL) {
				creature->sendSystemMessage("Invalid Target");
				return GENERALERROR;
			}

			if (!targetCreature->isAttackableBy(creature)) {
				creature->sendSystemMessage("Invalid Target");
				return GENERALERROR;
			}

			if (targetCreature->isPlayerCreature()) {
				if (!creature->hasBountyMissionFor(targetCreature) || !creature->hasSkill("combat_bountyhunter_master")) {
					creature->sendSystemMessage("@trap/trap:sys_creatures_only");
					return GENERALERROR;
				}
			} else if (targetCreature->isPet()) {
				ManagedReference<CreatureObject*> owner = targetCreature->getLinkedCreature().get();

				if (!creature->hasBountyMissionFor(owner) || !creature->hasSkill("combat_bountyhunter_master")) {
					creature->sendSystemMessage("@trap/trap:sys_creatures_only");
					return GENERALERROR;
				}
			} else if (!targetCreature->isCreature()) {
				creature->sendSystemMessage("@trap/trap:sys_creatures_only");
				return GENERALERROR;
			}

			if (targetCreature->hasTrapImmunity()) {
				creature->sendSystemMessage("Your target is immune to traps.");
				return GENERALERROR;
			}

			SharedObjectTemplate* templateData =
					TemplateManager::instance()->getTemplate(
							trap->getServerObjectCRC());
			if (templateData == NULL) {
				error("No template for: " + String::valueOf(trap->getServerObjectCRC()));
				return GENERALERROR;
			}

			TrapTemplate* trapData = cast<TrapTemplate*> (templateData);
			if (trapData == NULL) {
				error("No TrapTemplate for: " + String::valueOf(trap->getServerObjectCRC()));
				return GENERALERROR;
			}

			/// Check Range
			if(!checkDistance(creature, targetCreature, trapData->getMaxRange()))
			{
				StringIdChatParameter tooFar("cmd_err", "target_range_prose");
				tooFar.setTO("Throw Trap");

				creature->sendSystemMessage(tooFar);
				return GENERALERROR;
			}

			if (!CollisionManager::checkLineOfSight(creature, targetCreature)) {
				creature->sendSystemMessage("@combat_effects:cansee_fail");
				return GENERALERROR;
			}

			int effectType = 0;

			// No skill Check
			int trappingSkill = creature->getSkillMod("trapping");
			if(trappingSkill < 1) {
				creature->sendSystemMessage("@trap/trap:trap_no_skill");
				return GENERALERROR;
			}

			// MBH able to use adhesive trap without Scout x4xx
			if (creature->hasSkill("combat_bountyhunter_master") && trappingSkill < 30)
				trappingSkill = 30;

			/// Skill too low check
			if(trappingSkill < trapData->getSkillRequired()) {
				creature->sendSystemMessage("@trap/trap:trap_no_skill_this");
				return GENERALERROR;
			}

			int targetDefense = targetCreature->getSkillMod(trapData->getDefenseMod());
			Time* cooldown = creature->getCooldownTime("throwtrap");
			if((cooldown != NULL && !cooldown->isPast()) ||
					creature->getPendingTask("throwtrap") != NULL) {
				creature->sendSystemMessage("@trap/trap:sys_not_ready");
				return GENERALERROR;
			}

			float hitChance = CombatManager::instance()->hitChanceEquation(trappingSkill, System::random(199) + 1, targetDefense, System::random(199) + 1);

			if (hitChance > 100)
				hitChance = 100.0;
			else if (hitChance < 0)
				hitChance = 0;

			int roll = System::random(100);
			uint64 state = trapData->getState();
			bool hit = roll < hitChance && (state == 0 || (state != 0 && !targetCreature->hasState(state)));

			String animation = trapData->getAnimation();
			uint32 crc = String(animation).hashCode();
			CombatAction* action = new CombatAction(creature, targetCreature, crc, hit, 0L);
			creature->broadcastMessage(action, true);

			if (targetCreature->isPlayerCreature() || targetCreature->isPet()) {
				creature->addCooldown("throwtrap", 15000); //15 Seconds

				Locker locker(targetCreature);
				targetCreature->updateTrapImmunityTime(20000); //20 Seconds
			} else {
				creature->addCooldown("throwtrap", 1500);
			}

			Locker clocker(trap, creature);

			trap->decreaseUseCount();

			StringIdChatParameter message;
			ManagedReference<Buff*> buff = NULL;
			int damage = 0;

			if (hit) {
				message.setStringId("trap/trap" , trapData->getSuccessMessage());

				buff = new Buff(targetCreature, crc, trapData->getDuration(), BuffType::STATE);

				Locker locker(buff);
				Locker pLocker(targetCreature);

				if (state != 0) {
					if (state == CreatureState::FROZEN && (targetCreature->isPlayerCreature() || targetCreature->isPet())){
						state = CreatureState::IMMOBILIZED;
						uint32 forceRun1CRC = BuffCRC::JEDI_FORCE_RUN_1;
						uint32 forceRun2CRC = BuffCRC::JEDI_FORCE_RUN_2;
						uint32 forceRun3CRC = BuffCRC::JEDI_FORCE_RUN_3;

						if (targetCreature->hasBuff(forceRun1CRC))
							targetCreature->removeBuff(forceRun1CRC);
						if (targetCreature->hasBuff(forceRun2CRC))
							targetCreature->removeBuff(forceRun2CRC);
						if (targetCreature->hasBuff(forceRun3CRC))
							targetCreature->removeBuff(forceRun3CRC);
						if (creature->hasBuff(STRING_HASHCODE("burstrun")) || creature->hasBuff(STRING_HASHCODE("retreat"))) {
							creature->removeBuff(STRING_HASHCODE("burstrun"));
							creature->removeBuff(STRING_HASHCODE("retreat"));
						}
						buff->setSpeedMultiplierMod(0.1f);
						buff->setAccelerationMultiplierMod(0.1f);
					}
					buff->addState(state);
				}

				VectorMap<String, int>* skillMods = trapData->getSkillMods();
				for(int i = 0; i < skillMods->size(); ++i) {
					buff->setSkillModifier(skillMods->elementAt(i).getKey(), skillMods->get(i));
				}

				String startSpam = trapData->getStartSpam();
				if(!startSpam.isEmpty())
					buff->setStartFlyText("trap/trap", startSpam,  0, 0xFF, 0);

				String stopSpam = trapData->getStopSpam();
				if(!stopSpam.isEmpty())
					buff->setEndFlyText("trap/trap", stopSpam,  0xFF, 0, 0);

				damage = System::random(trapData->getMaxDamage() - trapData->getMinDamage()) + trapData->getMinDamage();

			} else {
				if(!trapData->getFailMessage().isEmpty()) {
					message.setStringId("trap/trap" , trapData->getFailMessage());
				}
			}

			message.setTT(targetCreature->getDisplayedName());


			Reference<ThrowTrapTask*> trapTask = new ThrowTrapTask(creature, targetCreature, buff, message, trapData->getPoolToDamage(), damage, hit);
			creature->addPendingTask("throwtrap", trapTask, 2300);

			//Reduce cost based upon player's strength, quickness, and focus if any are over 300
			int healthCost = creature->calculateCostAdjustment(CreatureAttribute::STRENGTH, trapData->getHealthCost());
			int actionCost = creature->calculateCostAdjustment(CreatureAttribute::QUICKNESS, trapData->getActionCost());
			int mindCost = creature->calculateCostAdjustment(CreatureAttribute::FOCUS, trapData->getMindCost());

			creature->inflictDamage(creature, CreatureAttribute::HEALTH, healthCost, false);
			creature->inflictDamage(creature, CreatureAttribute::ACTION, actionCost, false);
			creature->inflictDamage(creature, CreatureAttribute::MIND, mindCost, false);

			return SUCCESS;

		} catch (Exception& e) {

		}

		return GENERALERROR;
	}

	float getCommandDuration(CreatureObject* object, const UnicodeString& arguments) const {
		return defaultTime;
	}

};

#endif //THROWTRAPCOMMAND_H_
