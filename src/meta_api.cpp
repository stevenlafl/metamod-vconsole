#include <extdll.h>
#include <meta_api.h>
#include "vconsole_server.hpp"
#include "config.hpp"

#define METAMOD_VCONSOLE_VERSION "0.1.0"

static VConsoleConfig g_config;

meta_globals_t *gpMetaGlobals;
gamedll_funcs_t *gpGamedllFuncs;
mutil_funcs_t *gpMetaUtilFuncs;

plugin_info_t Plugin_info =
{
	META_INTERFACE_VERSION,
	"MetamodVConsole",
	METAMOD_VCONSOLE_VERSION,
	__DATE__,
	"StevenLafl",
	"https://github.com/stevenlafl/metamod-vconsole",
	"MVCON",
	PT_ANYTIME,
	PT_ANYTIME,
};

extern DLL_FUNCTIONS g_DllFunctionTable;
extern DLL_FUNCTIONS g_DllFunctionTable_Post;

extern enginefuncs_t g_EngineFunctionsTable;
extern enginefuncs_t g_EngineFunctionsTable_Post;

void executeServerCommand(const std::string& cmd) {
	if (cmd.empty()) {
		return;
	}
	std::string cmdWithNewline = cmd;
	if (cmdWithNewline.back() != '\n') {
		cmdWithNewline += '\n';
	}
	g_engfuncs.pfnServerCommand(cmdWithNewline.c_str());
	g_engfuncs.pfnServerExecute();
}

C_DLLEXPORT int Meta_Query(char *interfaceVersion, plugin_info_t **plinfo, mutil_funcs_t *pMetaUtilFuncs)
{
	*plinfo = &Plugin_info;
	gpMetaUtilFuncs = pMetaUtilFuncs;
	return TRUE;
}

META_FUNCTIONS gMetaFunctionTable =
{
	NULL,
	NULL,
	GetEntityAPI2,
	GetEntityAPI2_Post,
	GetNewDLLFunctions,
	GetNewDLLFunctions_Post,
	GetEngineFunctions,
	GetEngineFunctions_Post,
};

C_DLLEXPORT int Meta_Attach(PLUG_LOADTIME now, META_FUNCTIONS *pFunctionTable, meta_globals_t *pMGlobals, gamedll_funcs_t *pGamedllFuncs)
{
	gpMetaGlobals = pMGlobals;
	gpGamedllFuncs = pGamedllFuncs;

	g_engfuncs.pfnServerPrint("\n##########################\n# MetamodVConsole Loaded #\n##########################\n\n");

	std::string pluginDir = getPluginDirectory();
	std::string configPath = pluginDir + "config.ini";

	if (loadConfig(configPath, g_config)) {
		char msg[256];
		snprintf(msg, sizeof(msg), "MetamodVConsole: Loaded config from %s\n", configPath.c_str());
		g_engfuncs.pfnServerPrint(msg);
	} else {
		char msg[256];
		snprintf(msg, sizeof(msg), "MetamodVConsole: Config not found at %s, using defaults\n", configPath.c_str());
		g_engfuncs.pfnServerPrint(msg);
	}

	VConsoleServer::getInstance().setMaxConnections(g_config.max_connections);
	VConsoleServer::getInstance().setLogging(g_config.logging);

	if (VConsoleServer::getInstance().initialize(g_config.port, g_config.bind)) {
		char msg[128];
		snprintf(msg, sizeof(msg), "MetamodVConsole: VConsole server listening on %s:%d (max %d conn)\n",
		         g_config.bind.c_str(), VConsoleServer::getInstance().getPort(),
		         g_config.max_connections > 0 ? g_config.max_connections : -1);
		g_engfuncs.pfnServerPrint(msg);
	} else {
		char msg[128];
		snprintf(msg, sizeof(msg), "MetamodVConsole: Failed to start VConsole server on %s:%d!\n",
		         g_config.bind.c_str(), g_config.port);
		g_engfuncs.pfnServerPrint(msg);
	}

	memcpy(pFunctionTable, &gMetaFunctionTable, sizeof(META_FUNCTIONS));
	return TRUE;
}

C_DLLEXPORT int Meta_Detach(PLUG_LOADTIME now, PL_UNLOAD_REASON reason)
{
	g_engfuncs.pfnServerPrint("MetamodVConsole: Shutting down...\n");
	VConsoleServer::getInstance().shutdown();
	return TRUE;
}
