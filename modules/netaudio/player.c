#include <pthread.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>

//#define SEND_TO_IP "192.168.3.101"
#define SEND_TO_IP "192.168.0.114"
#define PORT 20113

struct auplay_st {
	const struct auplay *ap;

	pthread_t thread;
	bool run;
	int16_t *sampv;
	size_t sampc;
	auplay_write_h *wh;
	void *arg;
	struct auplay_prm prm;
};

static void auplay_destructor(void *arg)
{
	struct auplay_st *st = arg;

	/* Wait for termination of other thread */
	if (st->run) {
		debug("netaudio: stopping playback thread\n");
		st->run = false;
		(void)pthread_join(st->thread, NULL);
	}

	mem_deref(st->sampv);
}


static void *write_thread(void *arg)
{
	struct auplay_st *st = arg;
	struct sockaddr_in si_other;
	int s, slen=sizeof( si_other);
	const size_t num_bytes = st->sampc * 2;

	if( ( s = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		printf( "socket error\n");
		return;
	}

	memset( ( char *) &si_other, 0, sizeof( si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons( PORT);
	if( inet_aton( SEND_TO_IP, &si_other.sin_addr) == 0)
	{
		printf( "inet_aton error\n");
		return;
	}

	while( st->run)
	{
		st->wh( st->sampv, st->sampc, st->arg);

		int sendBytes = sendto( s, st->sampv, num_bytes, 0, &si_other, slen);
		//printf( "s = %d\n", sendBytes);

		if( sendBytes == -1)
		{
			printf( "sendto error\n");
			return;
		}

		usleep( st->prm.ptime * 1000);
	}
}

int netaudio_player_alloc(struct auplay_st **stp, const struct auplay *ap,
		       struct auplay_prm *prm, const char *device,
		       auplay_write_h *wh, void *arg)
{

	struct auplay_st *st;
	int err = 0;

	//printf( "pulse_player_alloc\n");

	if (!stp || !ap || !prm || !wh)
		return EINVAL;

	debug("netaudio: opening player (%u Hz, %d channels, device '%s')\n",
		  prm->srate, prm->ch, device);

	st = mem_zalloc(sizeof(*st), auplay_destructor);
	if (!st)
		return ENOMEM;

	st->ap  = ap;
	st->wh  = wh;
	st->arg = arg;
	st->prm   = *prm;

	st->sampc = prm->srate * prm->ch * prm->ptime / 1000;

	st->sampv = mem_alloc(2 * st->sampc, NULL);
	if (!st->sampv) {
		err = ENOMEM;
		goto out;
	}

	st->run = true;
	err = pthread_create(&st->thread, NULL, write_thread, st);
	if (err) {
		st->run = false;
		goto out;
	}

	debug("netaudio: playback started\n");

 out:
	if (err)
		mem_deref(st);
	else
		*stp = st;

	return err;
}
