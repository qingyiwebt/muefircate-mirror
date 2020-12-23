#include <efi.h>
#include <efilib.h>
#include "efi-stuff.h"
#include "loader.h"

void wait_and_exit(EFI_STATUS status)
{
	Output(u"press a key to exit\r\n");
	Pause();
	exit_fb_con();
	Exit(status, 0, NULL);
}

void error_with_status(IN CONST CHAR16 *msg, EFI_STATUS status)
{
	Print(u"error: %s: %d\r\n", msg, (INT32)status);
	wait_and_exit(status);
}

void error(IN CONST CHAR16 *msg)
{
	Print(u"error: %s\r\n", msg);
	wait_and_exit(EFI_ABORTED);
}
