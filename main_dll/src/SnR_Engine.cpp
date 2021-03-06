#include "SnR_Engine.h"

namespace SnR_Engine {
	uint GetModuleSize(HMODULE hModule)
	{
		HANDLE hSnap;
		MODULEENTRY32 mod;
		hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
		mod.dwSize = sizeof(MODULEENTRY32);
		if (Module32First(hSnap, &mod)) {
			do
			{
				if (mod.hModule == hModule)
				{
					CloseHandle(hSnap);
					return uint(mod.modBaseSize);
				}
			} while (Module32Next(hSnap, &mod));
		}
		CloseHandle(hSnap);
		return 0;
	}

	SnR_Engine::SnR_Engine()
	{
		init_module(GetModuleHandle(nullptr));
	}


	SnR_Engine::~SnR_Engine()
	{
		// Do nothing :3
	}

	uint SnR_Engine::calcRuleSize(ubyte rule[])
	{
		ubyte *lpRule = rule;
		uint len;
		uint rSize = 0;

		while (true)
		{
			switch (*(lpRule++))
			{
			case SearchMode_Skip:
				rSize += *(lpRule++);
				break;

			case SearchMode_Search:
			case SearchMode_Replace:
				len = *(lpRule++);
				rSize += len;
				lpRule += len;
				break;

			case SearchMode_EOF:
				return rSize;

			default:
				dprintf("Unexpected char: 0x%02x\n", *(lpRule - 1));
				assert(true);
				return 0;
			}
		}
	}

	uAddr SnR_Engine::findNext(uAddr offset, ubyte rule[])
	{
		uint ruleSize = calcRuleSize(rule);

		assert(ruleSize);

		if (!ruleSize)
			return 0;

		ubyte *src = reinterpret_cast<ubyte *>(baseAddr);
		ubyte *eof = src + dataSize - ruleSize;
		src += offset;
		printf("RuleSize: %d\n", ruleSize);
#ifdef _WIN64
		dprintf("Search range[size: %d]: %016llx ~ %016llx\n",
			dataSize - ruleSize, baseAddr, uAddr(eof));
#else
		dprintf("Search range[size: %d]: %08x ~ %08x\n",
			dataSize - ruleSize, baseAddr, uint(eof));
#endif
		while (src < eof)
		{
			if (checkRule(src, rule))
			{
				return uAddr(src - baseAddr);
			}

			src++;
		}

		return 0;
	}

	uAddr SnR_Engine::findPrev(uAddr offset, ubyte rule[])
	{
		uint ruleSize = calcRuleSize(rule);

		assert(ruleSize);

		if (!ruleSize)
			return 0;

		ubyte *src = reinterpret_cast<ubyte *>(baseAddr);
		ubyte *eof = src + offset;

		dprintf("RuleSize: %d\n", ruleSize);
#ifdef _WIN64
		dprintf("Search range[size: %d]: %016llx ~ %016llx\n",
			dataSize - ruleSize, baseAddr, uAddr(eof));
#else
		dprintf("Search range[size: %d]: %08x ~ %08x\n",
			dataSize - ruleSize, baseAddr, uint(eof));
#endif
		while (src < eof)
		{
			if (checkRule(eof, rule))
			{
				return uAddr(eof - baseAddr);
			}

			eof--;
		}

		return 0;
	}

	uint SnR_Engine::doSearchAndReplace(ubyte rule[], ubyte replacement[])
	{
		uint ruleSize = calcRuleSize(rule);
		uint replSize = calcRuleSize(replacement);

		assert(ruleSize);
		assert(replSize);

		if (!ruleSize || !replSize) {
			return 0;
		}

		uint count = 0;
		ubyte *src = reinterpret_cast<ubyte *>(baseAddr);
		ubyte *eof = src + dataSize - max(ruleSize, replSize);
		dprintf("RuleSize: %d\t\tReplSize: %d\n", ruleSize, replSize);
#ifdef _WIN64
		dprintf("Search range[size: %d]: %016llx ~ %016llx\n",
			dataSize - max(ruleSize, replSize), baseAddr, uAddr(eof));
#else
		dprintf("Search range[size: %d]: %08x ~ %08x\n",
			dataSize - max(ruleSize, replSize), baseAddr, uint(eof));
#endif
		while (src < eof)
		{
			if (checkRule(src, rule))
			{
				doReplace(src, replacement);
				count++;
			}

			src++;
		}

		return count;
	}

	bool SnR_Engine::doSearchAndReplaceOnce(ubyte rule[], ubyte replacement[])
	{
		uint ruleSize = calcRuleSize(rule);
		uint replSize = calcRuleSize(replacement);

		assert(ruleSize);
		assert(replSize);

		if (!ruleSize || !replSize) {
			return 0;
		}

		ubyte *src = reinterpret_cast<ubyte *>(baseAddr);
		ubyte *eof = src + dataSize - max(ruleSize, replSize);
		dprintf("RuleSize: %d\t\tReplSize: %d\n", ruleSize, replSize);
#ifdef _WIN64
		dprintf("Search range[size: %d]: %016llx ~ %016llx\n",
			dataSize - max(ruleSize, replSize), baseAddr, uAddr(eof));
#else
		dprintf("Search range[size: %d]: %08x ~ %08x\n",
			dataSize - max(ruleSize, replSize), baseAddr, uint(eof));
#endif
		while (src < eof)
		{
			if (checkRule(src, rule))
			{
				doReplace(src, replacement);
				return true;
			}

			src++;
		}

		return false;
	}

	void SnR_Engine::doReplace(ubyte *src, ubyte replacement[])
	{
#ifdef _WIN64
		dprintf("Apply Patch at %016llx\n", reinterpret_cast<uAddr>(src));
#else
		dprintf("Apply Patch at %08x\n", reinterpret_cast<uAddr>(src));
#endif

		ubyte *lpReplc = replacement;
		uint len;
		DWORD oldProtect;

		while (true)
		{
			switch (*(lpReplc++))
			{
			case SearchMode_Skip:
				src += *(lpReplc++);
				break;

			case SearchMode_Replace:
				len = *(lpReplc++);
				VirtualProtect(src, len, PAGE_EXECUTE_READWRITE, &oldProtect);
				memcpy(src, lpReplc, len);
				VirtualProtect(src, len, oldProtect, &oldProtect);
				src += len;
				lpReplc += len;
				break;

			case SearchMode_EOF:
				return;

			default:
				dprintf("Unexpected char: 0x%02x\n", *(lpReplc - 1));
				assert(true);
				return;
			}
		}
	}

	bool SnR_Engine::checkRule(ubyte *src, ubyte rule[])
	{
		uint len;
		ubyte *lpRule = rule;
		while (true)
		{
			switch (*(lpRule++))
			{
			case SearchMode_Skip:
				src += *(lpRule++);
				break;

			case SearchMode_Search:
				len = *(lpRule++);
				if (memcmp(src, lpRule, len) != 0)
					return false;

				lpRule += len;
				src += len;

				break;

			case SearchMode_EOF:
				return true;

			default:
				dprintf("Unexpected char: 0x%02x\n", *(lpRule - 1));
				assert(true);
				return false;
			}
		}
	}

	SnR_Engine::SnR_Engine(HMODULE hModule)
	{
		init_module(hModule);
	}

	SnR_Engine::SnR_Engine(uAddr baseAddr, uint bufSize)
	{
		this->baseAddr = baseAddr;
		this->dataSize = bufSize;
	}

	void SnR_Engine::init_module(HMODULE hModule)
	{
		this->baseAddr = uAddr(hModule);
		this->dataSize = GetModuleSize(hModule);
	}
}