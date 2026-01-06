#include <string.h>
#include <extdll.h>
#include <meta_api.h>

enginefuncs_t g_engfuncs;
globalvars_t  *gpGlobals;

C_DLLEXPORT void WINAPI GiveFnptrsToDll(enginefuncs_t *pengfuncsFromEngine, globalvars_t *pGlobals)
{
	memcpy(&g_engfuncs, pengfuncsFromEngine, sizeof(enginefuncs_t));
	gpGlobals = pGlobals;
}
