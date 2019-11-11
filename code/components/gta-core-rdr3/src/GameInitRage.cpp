/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include <GameInit.h>

#include <Hooking.h>

#include <CoreConsole.h>
#include <console/OptionTokenizer.h>
#include <gameSkeleton.h>

RageGameInit g_gameInit;
fwEvent<const char*> OnKillNetwork;
fwEvent<> OnKillNetworkDone;

void RageGameInit::KillNetwork(const wchar_t* errorString)
{
	if (errorString == (wchar_t*)1)
	{
		OnKillNetwork("Reloading game.");
	}
	else
	{
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> converter;
		std::string smallReason = converter.to_bytes(errorString);

		OnKillNetwork(smallReason.c_str());
	}
}

bool RageGameInit::GetGameLoaded()
{
	return m_gameLoaded;
}

void RageGameInit::SetGameLoaded()
{
	m_gameLoaded = true;
}

void RageGameInit::SetPreventSavePointer(bool* preventSaveValue)
{

}

void RageGameInit::ReloadGame()
{
	assert(!"not implemented!");
}

static bool canContinueLoad;

void RageGameInit::LoadGameFirstLaunch(bool(*callBeforeLoad)())
{
	OnGameRequestLoad();

	canContinueLoad = true;
}

bool RageGameInit::TryDisconnect()
{
	return true;
}

bool RageGameInit::TriggerError(const char* message)
{
	return (!OnTriggerError(message));
}

static hook::cdecl_stub<void()> _doLookAlive([]()
{
	return hook::get_pattern("40 8A FB 38 1D", -0x29);
});

// share this(!)
static std::vector<ProgramArguments> g_argumentList;

static InitFunction initFunction([]()
{
	// initialize console arguments
	std::vector<std::pair<std::string, std::string>> setList;

	auto commandLine = GetCommandLineW();

	{
		wchar_t* s = commandLine;

		if (*s == L'"')
		{
			++s;
			while (*s)
			{
				if (*s++ == L'"')
				{
					break;
				}
			}
		}
		else
		{
			while (*s && *s != L' ' && *s != L'\t')
			{
				++s;
			}
		}

		while (*s == L' ' || *s == L'\t')
		{
			s++;
		}

		try
		{
			std::tie(g_argumentList, setList) = TokenizeCommandLine(ToNarrow(s));
		}
		catch (std::runtime_error & e)
		{
			trace("couldn't parse command line: %s\n", e.what());
		}
	}

	se::ScopedPrincipal principalScope(se::Principal{ "system.console" });

	for (const auto& set : setList)
	{
		console::GetDefaultContext()->ExecuteSingleCommandDirect(ProgramArguments{ "set", set.first, set.second });
	}
});

static InitFunction initFunctionTwo([]()
{
	rage::OnInitFunctionStart.Connect([](rage::InitFunctionType type)
	{
		if (type == rage::INIT_SESSION)
		{
			while (!canContinueLoad)
			{
				Sleep(0);

				_doLookAlive();

				// todo: add critical servicing
			}
		}
	}, -99999);

	// early init command stuff
	rage::OnInitFunctionStart.Connect([](rage::InitFunctionType type)
	{
		if (type == rage::InitFunctionType::INIT_CORE)
		{
			// run command-line initialization
			se::ScopedPrincipal principalScope(se::Principal{ "system.console" });

			for (const auto& bit : g_argumentList)
			{
				console::GetDefaultContext()->ExecuteSingleCommandDirect(bit);
			}
		}
	});

	rage::OnInitFunctionEnd.Connect([](rage::InitFunctionType type)
	{
		if (type == rage::INIT_SESSION)
		{
			g_gameInit.SetGameLoaded();
		}
	});

	static ConsoleCommand assertCmd("_assert", []()
	{
		assert(!"_assert command used");
	});

	static ConsoleCommand crashCmd("_crash", []()
	{
		*(volatile int*)0 = 0;
	});

	Instance<ICoreGameInit>::Set(&g_gameInit);
});
