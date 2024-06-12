#ifndef KERNELUSER_H
#define KERNELUSER_H

#include <ntddk.h>

#define DebugMessage(x, ...) DbgPrintEx(0, 0, x, __VA_ARGS__)


UNICODE_STRING g_SymbolicLink;
HANDLE g_EventHandle = NULL;
PKEVENT g_EventObject = NULL;
#define DebugMessage(x, ...) DbgPrintEx(0, 0, x, __VA_ARGS__)

NTSYSAPI NTSTATUS NTAPI ZwCreateEvent(PHANDLE EventHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, EVENT_TYPE EventType, BOOLEAN InitialState);




#endif
