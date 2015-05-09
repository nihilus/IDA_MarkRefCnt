
// ****************************************************************************
// File: Main.cpp
// Desc: Function ref count in function comment plug-in by Sirmabus
//
// ****************************************************************************
#include "stdafx.h"

// === Function Prototypes ===
int idaapi IDAP_init();
void idaapi IDAP_term();
void idaapi IDAP_run(int arg);
extern void CORE_Init();
extern void CORE_Process(int iArg);
extern void CORE_Exit();


// === Data ===
const static char IDAP_name[] = "Mark reference counts";

// Plug-in description block
extern "C" ALIGN(16) plugin_t PLUGIN =
{
	IDP_INTERFACE_VERSION,	// IDA version plug-in is written for
	PLUGIN_UNL,				// Plug-in flags
	IDAP_init,	            // Initialization function
	IDAP_term,	            // Clean-up function
	IDAP_run,	            // Main plug-in body
    IDAP_name,	            // Comment - unused
    IDAP_name,	            // As above - unused
	IDAP_name,	            // Plug-in name shown in Edit->Plugins menu
	NULL	                // Hot key to run the plug-in
};

int idaapi IDAP_init()
{
    CORE_Init();
    return(PLUGIN_OK);
}

void idaapi IDAP_term()
{
    CORE_Exit();
}

void idaapi IDAP_run(int iArg)
{
    CORE_Process(iArg);
}



