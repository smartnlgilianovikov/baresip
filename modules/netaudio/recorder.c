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

#define PORT 20113

struct ausrc_st {
	const struct ausrc *as;

	pthread_t thread;
	bool run;
	int16_t *sampv;
	size_t sampc;
	ausrc_read_h *rh;
	void *arg;

	struct ausrc_prm prm;
	struct aubuf *aubuf;
	size_t psize;
};

static void ausrc_destructor(void *arg)
{
	struct ausrc_st *st = arg;

	if (st->run) {
		debug("netaudio: stopping record thread\n");
		st->run = false;
		(void)pthread_join(st->thread, NULL);
	}

	mem_deref(st->sampv);
}

static void *read_thread( void *arg)
{
	struct ausrc_st *st = arg;
	struct sockaddr_in si_me, si_other;
	int s, i, slen=sizeof( si_other);
	const size_t num_bytes = 1400;
	char buffer[ num_bytes];

	if( ( s = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		printf( "socket error\n");
		return;
	}

	int enable = 1;

	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
	{
		printf( "setsockopt error\n");
		return;
	}

	memset(( char *) &si_me, 0, sizeof( si_me));
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons( PORT);
	si_me.sin_addr.s_addr = htonl( INADDR_ANY);
	printf( "bind\n");
	if( bind(s, &si_me, sizeof(si_me))==-1)
	{
		printf( "bind error\n");
		return;
	}

	while( st->run)
	{
		int recvBytes = recvfrom( s, buffer, num_bytes, 0, &si_other, &slen);
		if( recvBytes == -1)
		{
			printf( "recvfrom error\n");
			return;
		}

		aubuf_write(st->aubuf, buffer, recvBytes);

		while (aubuf_cur_size(st->aubuf) >= st->psize) {
			play_packet(st);
		}

		//printf("Received packet from %s:%d\nData: %s\n\n", inet_ntoa( si_other.sin_addr), ntohs( si_other.sin_port), st->sampv);
	}
}

void play_packet(struct ausrc_st *st)
{
	int16_t buf[st->sampc];

	/* timed read from audio-buffer */
	if (aubuf_get_samp(st->aubuf, st->prm.ptime, buf, st->sampc))
		return;

	/* call read handler */
	if (st->rh)
		st->rh(buf, st->sampc, st->arg);
}

int netaudio_recorder_alloc(struct ausrc_st **stp, const struct ausrc *as,
			 struct media_ctx **ctx,
			 struct ausrc_prm *prm, const char *device,
			 ausrc_read_h *rh, ausrc_error_h *errh, void *arg)
{
	struct ausrc_st *st;
	int err;

	st = mem_zalloc(sizeof(*st), ausrc_destructor);

	if (!st)
		return ENOMEM;

	st->as  = as;
	st->rh  = rh;
	st->arg = arg;

	st->sampv = mem_alloc(2 * st->sampc, NULL);
	if (!st->sampv) {
		err = ENOMEM;
		goto out;
	}

	st->prm   = *prm;
	st->sampc = prm->srate * prm->ch * prm->ptime / 1000;
	st->psize = 2 * st->sampc;
	err = aubuf_alloc(&st->aubuf, st->psize, 0);
	if (err)
		goto out;

	st->run = true;
	err = pthread_create(&st->thread, NULL, read_thread, st);
	if (err) {
		st->run = false;
		goto out;
	}

out:
	if (err)
		mem_deref(st);
	else
		*stp = st;

	return err;
}
