
// ****************************************************************************
// File: Core.cpp
// Desc: Show function ref count plug-in.
//
// ****************************************************************************
#include "stdafx.h"
#include <list>
#include "resource.h"
#include <WaitBoxEx.h>
#include <SegSelect.h>

// === Function Prototypes ===
static void processFunction(func_t *f);
static void processDataItem(ea_t address);

// UI options bit flags
const static WORD PROCESS_FUNCTIONS	= (1 << 0);
const static WORD PROCESS_DATA      = (1 << 1);
const static WORD AUDIO_NOTIFY_DONE = (1 << 2);

// === Data ===
static BOOL processFunctions = TRUE;
static BOOL processData      = TRUE;
static BOOL audioOnDone      = TRUE;
static HMODULE myModule      = NULL;
static SegSelect::segments *segSelect = NULL;

#ifdef __EA64__
#pragma message("** __EA64__ build **")
#endif

// Main dialog
static const char mainDialog[] =
{
	"BUTTON YES* Continue\n" // "Continue" instead of an "okay" button

	// Title
	"Mark Reference Counts\n"
    #ifdef _DEBUG
    "** DEBUG BUILD **\n"
    #endif

	// Message text
    "-Version: %Aby Sirmabus-\n"
    "<#Click to open site.#www.macromonkey.com:k:1:1::>\n\n"
	"Creates new or prefixes exiting comments with function references, \nand, or, data refes-to-code counts.\n\n"

	// ** Order must match option bit flags above
	"Processing options:\n"
	"<#Add reference counts to all functions.#Do function references.                                             :C>\n"
	"<#Add references counts to all data-to-code references that are greater then one.#Do data references.:C>\n"
	"<#Notification sound when finished.#Audio on completion.:C>>\n"
	" \n"

	"<#Choose the data segments to scan.\nElse will use default 'DATA' segments by default.#Choose DATA segments:B:2:19::>\n \n"
};

// Initialize
void CORE_Init()
{
}

// Uninitialize
void CORE_Exit()
{
    if (segSelect)
    {
        SegSelect::free(segSelect);
        segSelect = NULL;
    }

    if(myModule)
        PlaySound(NULL, 0, 0);
}

static void idaapi doHyperlink(TView *fields[], int code) { open_url("http://www.macromonkey.com/bb/"); }

// Handler for choose code data segment button
static void idaapi chooseBtnHandler(TView *fields[], int code)
{
    if (segSelect = SegSelect::select((SegSelect::DATA_HINT | SegSelect::RDATA_HINT), "Choose data segments"))
    {
        msg("Chosen segments: ");
        for (SegSelect::segments::iterator it = segSelect->begin(); it != segSelect->end(); ++it)
        {
            char buffer[64];
            if (get_true_segm_name(*it, buffer, SIZESTR(buffer)) <= 0)
                strcpy(buffer, "????");

            SegSelect::segments::iterator it2 = it; ++it2;
            if (it2 != segSelect->end())
                msg("\"%s\", ", buffer);
            else
                msg("\"%s\"", buffer);
        }
        msg("\n");
        refreshUI();
    }
}

// Plug-in process
void CORE_Process(int arg)
{
    char version[16];
    sprintf(version, "%u.%u", HIBYTE(MY_VERSION), LOBYTE(MY_VERSION));
    msg("\n>> Mark Reference Counts: v: %s, built: %s, By Sirmabus\n", version, __DATE__);
    refreshUI();
	if(autoIsOk())
	{
		WORD optionFlags = 0;
		if(processFunctions) optionFlags |= PROCESS_FUNCTIONS;
		if(processData)	     optionFlags |= PROCESS_DATA;
		if(audioOnDone)	     optionFlags |= AUDIO_NOTIFY_DONE;

        int iUIResult = AskUsingForm_c(mainDialog, version, doHyperlink, &optionFlags, chooseBtnHandler);
		if(!iUIResult)
		{
			msg(" - Canceled -\n");
			return;
		}

		processFunctions = ((optionFlags & PROCESS_FUNCTIONS) > 0);
		processData	     = ((optionFlags & PROCESS_DATA) > 0);
		audioOnDone	     = ((optionFlags & AUDIO_NOTIFY_DONE) > 0);

        TIMESTAMP startTime = getTimeStamp();
        WaitBox::show();
        BOOL aborted = FALSE;

		if(processData)
		{
            // Use data segment defaults if none selected
            if (!segSelect || (segSelect && segSelect->empty()))
            {
                if (segSelect = new SegSelect::segments())
                {
                    for (int i = 0; i < get_segm_qty(); i++)
                    {
                        if (segment_t *seg = getnseg(i))
                        {
                            if (seg->type == SEG_DATA)
                                segSelect->push_front(seg);
                        }
                    }
                }
            }

            // Verify there are data segments to process
            if (!segSelect || (segSelect && segSelect->empty()))
            {
                msg("\nNo data segments found or selected!\n");
                msg("* Aborted *\n");
                return;
            }
		}

		if(processFunctions)
		{
			// Iterate through functions..
			UINT functionCount = get_func_qty();
            char buffer[32];
            msg("Processing %s functions.\n", prettyNumberString(functionCount, buffer));
            refreshUI();
            float mult = ((processData && (segSelect && !segSelect->empty())) ? 50.0f : 100.0f);

			for(UINT i = 0; i < functionCount; i++)
			{
                processFunction(getn_func(i));

                if (WaitBox::isUpdateTime())
                {
                    if (WaitBox::updateAndCancelCheck((int) (((float) i / (float) functionCount) * mult)))
                    {
                        msg("* Aborted *\n\n");
                        aborted = TRUE;
                        break;
                    }
                }
            }
		}

        if (!aborted && processData && (segSelect && !segSelect->empty()))
		{
            float mult = ((processFunctions ? 50.0f : 100.0f) / (float) segSelect->size());
			int   add  = (processFunctions ? 50 : 0);

			// Iterate through data segments
            while (segment_t *seg = segSelect->back())
			{
				char buffer[64] = {"unknown"}; buffer[SIZESTR(buffer)] = 0;
				get_true_segm_name(seg, buffer, SIZESTR(buffer));
                msg("Processing data segment: \"%s\" "EAFORMAT" - "EAFORMAT"\n", buffer, seg->startEA, seg->endEA);
                refreshUI();
				ea_t  startEA = seg->startEA;
				ea_t  endEA   = seg->endEA;
				float range   = (float) (endEA - startEA);

				ea_t address = startEA;
				while(address <= endEA)
				{
					processDataItem(address);
					if((address = nextaddr(address)) == BADADDR)
						break;

                    if (WaitBox::isUpdateTime())
                    {
                        if (WaitBox::updateAndCancelCheck(add + (int)(((float) (address - startEA) / range) * mult)))
                        {
                            msg("* Aborted *\n\n");
                            aborted = TRUE;
                            break;
                        }
                    }
				};

				add += (int) mult;
                segSelect->pop_back();
			};
		}

        if (!aborted && (processFunctions || processData))
		{
			refresh_idaview_anyway();
            TIMESTAMP endTime = (getTimeStamp() - startTime);
            msg("Done: Processing time %s.\n", timeString(endTime));
			msg("-----------------------------------------------------------------\n\n");

			if(audioOnDone)
			{
                if (endTime > (TIMESTAMP) 2.5)
                {
                    if (!myModule)
                        GetModuleHandleEx((GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS), (LPCTSTR)&CORE_Process, &myModule);
                    if (myModule)
                        PlaySound(MAKEINTRESOURCE(IDR_DONE_WAVE), myModule, (SND_RESOURCE | SND_SYNC));
                }
			}
		}

        WaitBox::hide();
	}
	else
	{
		warning("Auto-analysis must finish first before you run this plug-in!");
		msg("\n** Aborted **\n");
	}
}


// Process function
static void processFunction(func_t *f)
{
	xrefblk_t xb;
	if(xb.first_to(f->startEA, XREF_ALL))
	{
		// Per IDA doc code refs come first, then data refs
		if(xb.type >= fl_CF)
		{
			UINT count = 1;
			while(xb.next_to())
			{
				// Break on first data ref
				if(xb.type >= fl_CF)
					count++;
				else
					break;
			};

			// Append to existing comment if there is one
            char buffer[MAXSTR]; buffer[SIZESTR(buffer)] = 0;
			if(LPSTR currentComment = get_func_cmt(f, true))
			{
				_snprintf(buffer, SIZESTR(buffer), "%u %s", count, currentComment);
                qfree(currentComment);
			}
			else
			    // Create new comment
			    _snprintf(buffer, SIZESTR(buffer), "%u", count);

			#if 1
			if(!set_func_cmt(f, buffer, true))
                msg(EAFORMAT" *** Failed to set function comment! ***\n", f->startEA);
			#endif
		}
	}
}


// Place a data comment at given address
static void placeDataComment(ea_t address, LPSTR comment)
{
	//msg(EAFORMAT" '%s'\n", eaAddress, pszComment);
	if(!set_cmt(address, comment, true))
        msg(EAFORMAT" *** Failed to set data comment! ***\n", address);
}

// Process an item for data references
static void processDataItem(ea_t address)
{
    xrefblk_t xb;
	if(xb.first_to(address, XREF_ALL))
	{
		UINT count = 0;
		do
		{
			// Skip the data to data refs
			if((xb.type > 0) && !((xb.type == dr_O) && !isCode(getFlags(xb.from))))
				count++;

		}while(xb.next_to());

		if(count)
		{
			// Has a comment already?
			BOOL placed = FALSE;
			flags_t flags = getFlags(address);
			if(has_cmt(flags))
			{
				// Yes, a repeatable type?
				int commentLen = get_cmt(address, TRUE, NULL, 0);
				if(commentLen > 0)
				{
					// Don't add count if it's only one
					if(count > 1)
					{
                        char buffer[MAXSTR * 2]; buffer[SIZESTR(buffer)] = 0;
						int prefixSize = _snprintf(buffer, SIZESTR(buffer), "%u ", count);
                        int bufferFree = (sizeof(buffer) - prefixSize);
						if(get_cmt(address, TRUE, (buffer + prefixSize), bufferFree) > 0)
							placeDataComment(address, buffer);
						else
                            msg(EAFORMAT" ** Failed to read comment! **\n", address);
					}

					// Flag we handled it here
					placed = TRUE;
				}
			}

			if(!placed)
			{
				// Is the data a string?
				if(isASCII(flags))
				{
					// Don't add count if it's only one
					if(count > 1)
					{
                        // ** This string can be greater then MAXSTR (1024)!
						int len = get_max_ascii_length(address, ASCSTR_C, FALSE);
						if(len > 0)
						{
                            char buffer[MAXSTR * 2]; buffer[SIZESTR(buffer)] = 0;
							int prefixSize = _snprintf(buffer, SIZESTR(buffer), "%u \"", count);
                            int bufferFree = (sizeof(buffer) - (sizeof("\"") + prefixSize));
							get_ascii_contents2(address, len, ASCSTR_C, (buffer + prefixSize), bufferFree);

							strcat(buffer, "\"");
							placeDataComment(address, buffer);
						}
						else
                            msg(EAFORMAT" *** Get string length failed! ***\n", address);
					}
				}
				else
				// Add a new comment with just ref count
				{
                    char buffer[32];
					_ultoa(count, buffer, 10);
					placeDataComment(address, buffer);
				}
			}
		}
	}
}
