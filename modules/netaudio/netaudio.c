#include <re.h>
#include <rem.h>
#include <baresip.h>
#include "netaudio.h"

static struct auplay *auplay;
static struct ausrc *ausrc;

static int module_init(void)
{
	int err;

	printf( "Hello from netaudio!\n");

	err  = auplay_register(&auplay, "netaudio", netaudio_player_alloc);
	err |= ausrc_register(&ausrc, "netaudio", netaudio_recorder_alloc);

	printf( "err = %d\n", err);

	return err;
}


static int module_close(void)
{
	auplay = mem_deref(auplay);
	ausrc  = mem_deref(ausrc);

	return 0;
}


EXPORT_SYM const struct mod_export DECL_EXPORTS(netaudio) = {
	"netaudio",
	"audio",
	module_init,
	module_close,
};
