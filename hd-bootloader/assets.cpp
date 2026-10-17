// Stub of assets.efi, the file that holds the assets of the hyprdebian
// bootloader.
//
// update-bootloader.py adds the config, the menu rows, the DKMS MOK and the
// assets of the frontend to this stub as PE sections. The bootloader loads assets.efi with LoadImage, so that
// the firmware checks its Secure Boot signature, reads the sections from the
// loaded image and never starts it. Started on its own, it does nothing.

#include <efi.h>

extern "C" EFI_STATUS efi_main(EFI_HANDLE, EFI_SYSTEM_TABLE*)
{
    return EFI_UNSUPPORTED;
}
