#include <config.h>
#include <core.h>
#include <string>

#include "dab_module.h"

#include <easylogging++.h>
#include "modules/dab/logging.h"

INITIALIZE_EASYLOGGINGPP 

SDRPP_MOD_INFO{
    /* Name:            */ "dab_decoder",
    /* Description:     */ "DAB Radio Decoder",
    /* Author:          */ "William Yang",
    /* Version:         */ 1, 0, 0,
    /* Max instances    */ -1
};

MOD_EXPORT void _INIT_() {
    bool is_logging = true;

    auto dab_loggers = RegisterLogging();
    auto basic_radio_logger = el::Loggers::getLogger("basic-radio");
    auto basic_scraper_logger = el::Loggers::getLogger("basic-scraper");

    el::Configurations defaultConf;
    const char* logging_level = is_logging ? "true" : "false";
    defaultConf.setToDefault();
    defaultConf.setGlobally(el::ConfigurationType::Enabled, logging_level);
    defaultConf.setGlobally(el::ConfigurationType::Format, "[%level] [%thread] [%logger] %msg");
    el::Loggers::reconfigureAllLoggers(defaultConf);
    el::Helpers::setThreadName("main-thread");

    el::Configurations scraper_conf; 
    scraper_conf.setToDefault();
    scraper_conf.setGlobally(el::ConfigurationType::Enabled, "true");
    scraper_conf.setGlobally(el::ConfigurationType::Format, "[%level] [%thread] [%logger] %msg");
    basic_scraper_logger->configure(scraper_conf);

    json def = json({});
    config.setPath(core::args["root"].s() + "/dab_plugin_config.json");
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