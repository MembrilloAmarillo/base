#ifndef _THREADING_H_
#define _THREADING_H_

#include "types.h"

typedef LPDWORD OS_ThreadId;
typedef HANDLE  OS_ThreadHandle;
typedef DWORD   OS_ThreadError;

template <typename T>
fn_internal OS_ThreadId OS_CreateThread(void* (*func_ptr)(T* args), bool InitSuspend);

fn_internal OS_ThreadError OS_ResumeThread(OS_ThreadHandle Hndl);

fn_internal OS_ThreadError OS_SuspendThread(OS_ThreadHandle Hndl);

fn_internal OS_ThreadError OS_TerminateThread(OS_ThreadHandle Hndl);

#endif // _THREADING_H_

#ifdef THREADING_IMPL

template<typename T>
fn_internal OS_ThreadHandle OS_CreateThread(void* (*func_ptr)(T* args), bool InitSuspend, OS_ThreadId OutId)
{
	DWORD CreateFlag = 0;

	if (InitSuspend) {
		CreateFlag = CREATE_SUSPENDED;
	}

	OS_ThreadHandle Hndl = CreateThread(
		NULL,
		0,
		func_ptr,
		args,
		CreateFlag,
		OutId
	);

	return Hndl;
}

fn_internal OS_ThreadError OS_ResumeThread(OS_ThreadHandle Hndl) {
	return ResumeThread(Hndl);
}

fn_internal OS_ThreadError OS_SuspendThread(OS_ThreadHandle Hndl) {
	return SuspendThread(Hndl);
}

fn_internal OS_ThreadError OS_TerminateThread(OS_ThreadHandle Hndl) {
	DWORD Code;
	bool err = GetExitCodeThread(Hndl, (LPDWORD)&Code);
	if (err == 0) return GetLastError();
	err = TerminateThread(Hndl, Code);
	if (err == 0) return GetLastError();

	return 0;
}

#endif