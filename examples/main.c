#include <efi.h>

EFI_STATUS EFIAPI main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable)
{
	SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
	SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Hello, World!");
	UINTN index;
	SystemTable->BootServices->WaitForEvent(1, &SystemTable->ConIn->WaitForKey, &index);
	return EFI_SUCCESS;
}
