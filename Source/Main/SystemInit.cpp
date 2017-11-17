/*
Copyright (C) 2009 Howard Su (howard0su@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "Base/Daedalus.h"
#include "Main/SystemInit.h"

#include "Base/Macros.h"
#include "Core/CPU.h"
#include "Core/FramerateLimiter.h"
#include "Core/Memory.h"
#include "Core/PIF.h"
#include "Core/ROMBuffer.h"
#include "Core/RomSettings.h"
#include "Core/Save.h"
#include "Debug/DBGConsole.h"
#include "Debug/DebugLog.h"
#include "Debug/Synchroniser.h"
#include "Graphics/GraphicsContext.h"
#include "HLEAudio/AudioPlugin.h"
#include "HLEGraphics/GraphicsPlugin.h"
#include "Input/InputManager.h"
#include "Interface/Preferences.h"
#include "Interface/RomDB.h"
#include "Interface/SaveState.h"
#include "Interface/UI.h"

#if defined(DAEDALUS_OSX) || defined(DAEDALUS_W32)
#include "Debug/WebDebug.h"
#include "HLEGraphics/TextureCacheWebDebug.h"
#include "HLEGraphics/DisplayListDebugger.h"
#endif

struct SysEntityEntry
{
	const char* name;
	bool (*init)();
	void (*final)();
};

#ifdef DAEDALUS_ENABLE_PROFILING
static void ProfilerVblCallback(void* arg)
{
	CProfiler::Get()->Update();
	CProfiler::Get()->Display();
}

static bool Profiler_Init()
{
	if (!CProfiler::Create()) return false;

	CPU_RegisterVblCallback(&ProfilerVblCallback, NULL);

	return true;
}

static void Profiler_Fini()
{
	CPU_UnregisterVblCallback(&ProfilerVblCallback, NULL);
	CProfiler::Destroy();
}
#endif

static const SysEntityEntry gSysInitTable[] = {
#ifdef DAEDALUS_DEBUG_CONSOLE
	{"DebugConsole", CDebugConsole::Create, CDebugConsole::Destroy},
#endif
#ifdef DAEDALUS_LOG
	{"Logger", Debug_InitLogging, Debug_FinishLogging},
#endif
#ifdef DAEDALUS_ENABLE_PROFILING
	{"Profiler", Profiler_Init, Profiler_Fini},
#endif
	{"ROM Database", CRomDB::Create, CRomDB::Destroy},
	{"ROM Settings", CRomSettingsDB::Create, CRomSettingsDB::Destroy},
	{"InputManager", InputManager_Create, InputManager_Destroy},
	{"GraphicsContext", CGraphicsContext::Create, CGraphicsContext::Destroy},
#ifdef DAEDALUS_PSP
	{"Language", Translate_Init, NULL},
#endif
	{"Preference", CPreferences::Create, CPreferences::Destroy},
	{"Memory", Memory_Init, Memory_Fini},

	{"Controller", CController::Create, CController::Destroy},
	{"RomBuffer", RomBuffer::Create, RomBuffer::Destroy},

#if defined(DAEDALUS_OSX) || defined(DAEDALUS_W32)
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	{"WebDebug", WebDebug_Init, WebDebug_Fini},
	{"TextureCacheWebDebug", TextureCache_RegisterWebDebug, NULL},
	{"DLDebuggerWebDebug", DLDebugger_RegisterWebDebug, NULL},
#endif
#endif

	{"SaveState", SaveState_Initialise, SaveState_Finalise},
	{"UI", UI_Init, UI_Finalise},
};

struct RomEntityEntry
{
	const char* name;
	bool (*open)();
	void (*close)();
};

static const RomEntityEntry gRomInitTable[] = {
	{"RomBuffer", RomBuffer::Open, RomBuffer::Close},
	{"Settings", ROM_LoadFile, ROM_UnloadFile},
	{"Memory", Memory_Reset, Memory_Cleanup},
	{"Audio", CreateAudioPlugin, DestroyAudioPlugin},
	{"Graphics", CreateGraphicsPlugin, DestroyGraphicsPlugin},
	{"FramerateLimiter", FramerateLimiter_Reset, NULL},
	//{"RSP", RSP_Reset, NULL},
	{"CPU", CPU_RomOpen, CPU_RomClose},
	{"ROM", ROM_ReBoot, ROM_Unload},
	{"Controller", CController::Reset, CController::RomClose},
	{"Save", Save_Reset, Save_Fini},
#ifdef DAEDALUS_ENABLE_SYNCHRONISATION
	{"CSynchroniser", CSynchroniser::InitialiseSynchroniser, CSynchroniser::Destroy},
#endif
};

bool System_Init()
{
	for (u32 i = 0; i < ARRAYSIZE(gSysInitTable); i++)
	{
		const SysEntityEntry& entry = gSysInitTable[i];

		if (entry.init == NULL) continue;

		if (entry.init())
		{
			DBGConsole_Msg(0, "==>Initialized %s", entry.name);
		}
		else
		{
			DBGConsole_Msg(0, "==>Initialize %s Failed", entry.name);
			return false;
		}
	}

	return true;
}

bool System_Open(const std::string& filename)
{
	g_ROM.FileName = filename;
	for (u32 i = 0; i < ARRAYSIZE(gRomInitTable); i++)
	{
		const RomEntityEntry& entry = gRomInitTable[i];

		if (entry.open == NULL) continue;

		DBGConsole_Msg(0, "==>Open %s", entry.name);
		if (!entry.open())
		{
			DBGConsole_Msg(0, "==>Open %s [RFAILED]", entry.name);
			return false;
		}
	}

	return true;
}

void System_Close()
{
	for (s32 i = ARRAYSIZE(gRomInitTable) - 1; i >= 0; i--)
	{
		const RomEntityEntry& entry = gRomInitTable[i];

		if (entry.close == NULL) continue;

		DBGConsole_Msg(0, "==>Close %s", entry.name);
		entry.close();
	}
}

void System_Finalize()
{
	for (s32 i = ARRAYSIZE(gSysInitTable) - 1; i >= 0; i--)
	{
		const SysEntityEntry& entry = gSysInitTable[i];

		if (entry.final == NULL) continue;

		DBGConsole_Msg(0, "==>Finalize %s", entry.name);
		entry.final();
	}
}
