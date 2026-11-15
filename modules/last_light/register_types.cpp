#include "register_types.h"
#include "core/object/class_db.h"
#include "last_light_game.h"
void initialize_last_light_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		ClassDB::register_class<LastLightGame>();
	}
}
void uninitialize_last_light_module(ModuleInitializationLevel) {}
