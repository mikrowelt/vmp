/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "Hooking.h"

#include <optick.h>

struct sysIpcThreadStartInfo
{
	void* startRoutine;
	// more members
};

static HANDLE CreateThreadWrapper(_In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes, _In_ SIZE_T dwStackSize, _In_ LPTHREAD_START_ROUTINE lpStartAddress,
								  _In_opt_ __drv_aliasesMem LPVOID lpParameter, _In_ DWORD dwCreationFlags, _Out_opt_ LPDWORD lpThreadId)
{
	// find the name parameter by frobbling the parent stack
	char* parentStackPtr = reinterpret_cast<char*>(_AddressOfReturnAddress());
	char* threadName = *reinterpret_cast<char**>(parentStackPtr + 0x50 /* offset from base pointer to argument */ + 0x60 /* offset from function stack frame stack to base pointer */ + 8 /* return address offset */);

	// create metadata for passing to the thread
	struct WrapThreadMeta
	{
		char* threadName;
		LPTHREAD_START_ROUTINE origRoutine;
		sysIpcThreadStartInfo* originalData;
	};

	WrapThreadMeta* parameter = new WrapThreadMeta{ threadName, lpStartAddress, reinterpret_cast<sysIpcThreadStartInfo*>(lpParameter) };

	// create a thread with 'our' callback
	HANDLE hThread = CreateThread(lpThreadAttributes, dwStackSize, [] (void* arguments)
	{
		// get and free metadata
		WrapThreadMeta* metaPtr = reinterpret_cast<WrapThreadMeta*>(arguments);
		WrapThreadMeta meta = *metaPtr;
		delete metaPtr;

		// set thread name, if any
		if (meta.threadName)
		{
			SetThreadName(-1, meta.threadName);

			OPTICK_START_THREAD(meta.threadName);
		}
		else
		{
			SetThreadName(-1, va("sysThread (0x%x)", hook::get_unadjusted(meta.originalData->startRoutine)));
		}

		// invoke original thread start
		return meta.origRoutine(meta.originalData);
	}, parameter, dwCreationFlags, lpThreadId);

	return hThread;
}

static HookFunction hookFunction([] ()
{
	// RAGE thread creation function: CreateThread call
	void* createThread = hook::pattern("48 89 44 24 28 33 C9 44 89 7C 24 20").count(1).get(0).get<void>(12);

	hook::nop(createThread, 6); // as it's an indirect call
	hook::call(createThread, CreateThreadWrapper);
});

// Guard for the RAGE dependency worker job dispatch (mov rcx, rdi; call qword [rdi+60h]).
// On our current fused b3258 image, some dependency job entries carry function pointers into
// uncommitted memory (see debug-box session 2026-08-21: deterministic NX fault ~60-100s after
// HS_HOSTED on any server). Log the job and skip it instead of crashing the process.

typedef char (*JobRunFn)(void* job);

static char JobRunGuard(void* job)
{
	JobRunFn fn = *reinterpret_cast<JobRunFn*>(reinterpret_cast<char*>(job) + 0x60);

	MEMORY_BASIC_INFORMATION mbi = { 0 };
	if (fn && VirtualQuery(reinterpret_cast<void*>(fn), &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT &&
		(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
	{
		return fn(job);
	}

	void* f50 = *reinterpret_cast<void**>(reinterpret_cast<char*>(job) + 0x50);
	void* f58 = *reinterpret_cast<void**>(reinterpret_cast<char*>(job) + 0x58);
	uint32_t f68 = *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(job) + 0x68);
	uint32_t f6c = *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(job) + 0x6c);
	uint8_t f92 = *reinterpret_cast<uint8_t*>(reinterpret_cast<char*>(job) + 0x92);

	trace("JobRunGuard: skipping dependency job %p with invalid fn %p (f50=%p f58=%p f68=%08x f6c=%08x f92=%02x)\n",
		job, reinterpret_cast<void*>(fn), f50, f58, f68, f6c, f92);

	return 1;
}

static HookFunction hookFunctionJobGuard([] ()
{
	// mov rcx, rdi; call qword [rdi+60h]; test al, al; je <requeue>; xor edi, edi; lea rbx, [rbp]
	char* location = hook::pattern("48 8B CF FF 57 60 84 C0 74 43 33 FF 48 8D 5D 00").count(1).get(0).get<char>();

	// trampoline: mov rcx, rdi; mov rax, JobRunGuard; jmp rax
	char* stub = static_cast<char*>(hook::AllocateStubMemory(32));

	DWORD oldProtect;
	VirtualProtect(stub, 32, PAGE_EXECUTE_READWRITE, &oldProtect);

	stub[0] = 0x48; stub[1] = 0x89; stub[2] = 0xF9; // mov rcx, rdi
	stub[3] = 0x48; stub[4] = 0xB8;                 // mov rax, imm64
	*reinterpret_cast<void**>(stub + 5) = reinterpret_cast<void*>(&JobRunGuard);
	stub[13] = 0xFF; stub[14] = 0xE0;               // jmp rax

	// jump to the stub over the 6 original bytes (48 8B CF FF 57 60)
	hook::nop(location, 6);
	hook::jump(location, stub);
});
