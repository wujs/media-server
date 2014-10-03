/*
C->S:
SETUP rtsp://example.com/foo/bar/baz.rm RTSP/1.0
CSeq: 302
Transport: RTP/AVP;unicast;client_port=4588-4589

S->C: 
RTSP/1.0 200 OK
CSeq: 302
Date: 23 Jan 1997 15:35:06 GMT
Session: 47112344
Transport: RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257
*/
#include "rtsp-client-internal.h"

static const char* sc_rtsp_tcp = "SETUP %s RTSP/1.0\r\n"
								"CSeq: %u\r\n"
								"%s" // Session: %s\r\n
								"Transport: RTP/AVP/TCP;interleaved=%u-%u\r\n"
								"User-Agent: %s\r\n"
								"\r\n";

static const char* sc_rtsp_udp = "SETUP %s RTSP/1.0\r\n"
								"CSeq: %u\r\n"
								"%s" // Session: %s\r\n
								"Transport: RTP/AVP;unicast;client_port=%u-%u\r\n"
								"User-Agent: %s\r\n"
								"\r\n";

static void rtsp_client_media_setup_onreply(void* rtsp, int r, void* parser)
{
	const char *session;
	const char *transport;
	struct rtsp_client_context_t* ctx;

	ctx = (struct rtsp_client_context_t*)rtsp;
	assert(RTSP_SETUP == ctx->status);
	assert(ctx->progress < ctx->media_count);

	if(0 != r)
	{
		ctx->client.onopen(ctx->param, r, NULL, 0);
		return;
	}

	session = rtsp_get_header_by_name(parser, "Session");
	transport = rtsp_get_header_by_name(parser, "Transport");
	if(!session || !transport || 0!=rtsp_header_transport(transport, &ctx->media[ctx->progress].transport))
	{
		// clear 
		ctx->client.onopen(ctx->param, -1, NULL, 0);
		return;
	}

	assert(strlen(session) < sizeof(ctx->media[0].session));
	strncpy(ctx->media[ctx->progress].session, session, sizeof(ctx->media[0].session)-1);

	if(ctx->media_count == ++ctx->progress)
	{
		int i;
		// TODO: media
		rtsp_client_transport_t transports[N_MEDIA];
		for(i = 0; i < ctx->media_count && i < N_MEDIA; i++)
		{
			// Transport: RTP/AVP/TCP;interleaved=0-1
			// Transport: RTP/AVP;multicast;destination=224.2.0.1;port=3456-3457;ttl=16
			// 224.0.0.0 ~ 239.255.255.255
			transports[i].transport = (RTSP_TRANSPORT_RTP==ctx->media[i].transport.transport) ? (RTSP_TRANSPORT_UDP==ctx->media[i].transport.lower_transport?1:2) : 3;
			transports[i].multicasat = ctx->media[i].transport.multicast;
			transports[i].destination = ctx->media[i].transport.destination;
			transports[i].ttl = ctx->media[i].transport.ttl;
			transports[i].client_port[0] = ctx->media[i].transport.client_port1;
			transports[i].client_port[1] = ctx->media[i].transport.client_port2;
			transports[i].server_port[0] = ctx->media[i].transport.server_port1;
			transports[i].server_port[1] = ctx->media[i].transport.server_port1;
		}

		ctx->client.onopen(ctx->param, 0, transports, ctx->media_count);
	}
	else
	{
		if(0 != rtsp_client_media_setup(ctx))
		{
			ctx->client.onopen(ctx->param, -1, NULL, 0);
			return;
		}
	}
}

int rtsp_client_media_setup(struct rtsp_client_context_t* ctx)
{
	struct rtsp_media_t *media;
	char session[sizeof(media->session)];

	assert(RTSP_SETUP == ctx->status);
	assert(ctx->progress < ctx->media_count);
	media = rtsp_get_media(ctx, ctx->progress);

	if(ctx->aggregate && ctx->session[0])
	{
		snprintf(session, sizeof(session), "Session: %s\r\n", ctx->session);
		snprintf(ctx->req, sizeof(ctx->req), 
			RTSP_TRANSPORT_TCP==media->transport.lower_transport?sc_rtsp_tcp:sc_rtsp_udp,
			media->uri, ctx->cseq++, session, media->transport.client_port1, media->transport.client_port2, USER_AGENT);
	}
	else
	{
		snprintf(ctx->req, sizeof(ctx->req), 
			RTSP_TRANSPORT_TCP==media->transport.lower_transport?sc_rtsp_tcp:sc_rtsp_udp,
			media->uri, media->cseq++, "", media->transport.client_port1, media->transport.client_port2, USER_AGENT);
	}

	return ctx->client.request(ctx->param, media->uri, ctx->req, strlen(ctx->req), ctx, rtsp_client_media_setup_onreply);
}
