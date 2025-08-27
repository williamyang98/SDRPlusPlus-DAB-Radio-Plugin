#include <config.h>
#include <core.h>
#include <options.h>
#include <string>

#include "./dab_module.h"

SDRPP_MOD_INFO{
    /* Name:            */ "dab_decoder",
    /* Description:     */ "DAB Radio Decoder",
    /* Author:          */ "William Yang",
    /* Version:         */ 1, 0, 0,
    /* Max instances    */ -1
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    config.setPath(options::opts.root + "/dab_plugin_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new DABModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete reinterpret_cast<DABModule*>(instance);
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}