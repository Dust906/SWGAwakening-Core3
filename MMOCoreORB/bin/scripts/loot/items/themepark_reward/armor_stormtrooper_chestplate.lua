--Automatically generated by SWGEmu Spawn Tool v0.12 loot editor.

armor_stormtrooper_chestplate = {
	minimumLevel = 0,
	maximumLevel = -1,
	customObjectName = "",
	directObjectTemplate = "object/tangible/wearables/armor/stormtrooper/armor_stormtrooper_chest_plate_quest.iff",
	craftingValues = {
		{"armor_rating",1,1,0},
	        {"armor_effectiveness",30,30,10},
	        {"armor_integrity",45000,45000,0},
	        {"armor_health_encumbrance",150,150,0},
	        {"armor_action_encumbrance",45,45,0},
	        {"armor_mind_encumbrance",19,19,0},
	},
	customizationStringNames = {},
	customizationValues = {},
	skillMods = {{"stun_defense", 5}, {"melee_defense", 3}}
}

addLootItemTemplate("armor_stormtrooper_chestplate", armor_stormtrooper_chestplate)
