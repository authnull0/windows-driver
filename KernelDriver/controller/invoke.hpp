#pragma once

#include<windows.h>

#define IO_GET_ADDRESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x666, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
