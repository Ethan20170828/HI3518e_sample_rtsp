/******************************************************************************
  A simple program of Hisilicon HI3531 video encode implementation.
  Copyright (C), 2010-2011, Hisilicon Tech. Co., Ltd.
 ******************************************************************************
    Modification:  2011-2 Created
******************************************************************************/
#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "sample_comm.h"

#if	RTSP_ENABLE

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <netdb.h>
#include <sys/socket.h>

#include <sys/ioctl.h>
#include <fcntl.h> 
#include <pthread.h>
#include <sys/ipc.h> 
#include <sys/msg.h>
#include <netinet/if_ether.h>
#include <net/if.h>

#include <linux/if_ether.h>
#include <linux/sockios.h>
#include <netinet/in.h> 
#include <arpa/inet.h> 

#endif


VIDEO_NORM_E gs_enNorm = VIDEO_ENCODING_MODE_PAL;


HI_U32 g_u32BlkCnt = 4;


#if	RTSP_ENABLE

RTP_FIXED_HEADER  *rtp_hdr;

NALU_HEADER		*nalu_hdr;
FU_INDICATOR	*fu_ind;
FU_HEADER		*fu_hdr;

// ȫ�ֱ���û�г�ʼ����Ĭ��ֵΪ0
RTSP_CLIENT g_rtspClients[MAX_RTSP_CLIENT];

int g_nSendDataChn = -1;
pthread_mutex_t g_mutex;
pthread_cond_t  g_cond;
pthread_mutex_t g_sendmutex;

pthread_t g_SendDataThreadId = 0;
//HAL_CLIENT_HANDLE hMainStreamClient = NULL,hSubStreamClient = NULL,hAudioClient = NULL;
char g_rtp_playload[20];
int   g_audio_rate = 8000;
//VIDEO_NORM_E gs_enNorm = VIDEO_ENCODING_MODE_NTSC;//30fps
int g_nframerate;

int exitok = 0;

int udpfd;

int count=0;


struct list_head RTPbuf_head = LIST_HEAD_INIT(RTPbuf_head);

static pthread_t gs_RtpPid;


static char const* dateHeader()
{
	static char buf[200];
#if !defined(_WIN32_WCE)
	time_t tt = time(NULL);
	strftime(buf, sizeof buf, "Date: %a, %b %d %Y %H:%M:%S GMT\r\n", gmtime(&tt));
#endif

	return buf;
}
static char* GetLocalIP(int sock)
{
	struct ifreq ifreq;
	struct sockaddr_in *sin;
	char * LocalIP = malloc(20);
	strcpy(ifreq.ifr_name,"wlan");
	if (!(ioctl (sock, SIOCGIFADDR,&ifreq)))
    	{ 
		sin = (struct sockaddr_in *)&ifreq.ifr_addr;
		sin->sin_family = AF_INET;
       	strcpy(LocalIP,inet_ntoa(sin->sin_addr)); 
		//inet_ntop(AF_INET, &sin->sin_addr,LocalIP, 16);
    	} 
	printf("--------------------------------------------%s\n",LocalIP);
	return LocalIP;
}

char* strDupSize(char const* str) 
{
  if (str == NULL) return NULL;
  size_t len = strlen(str) + 1;
  char* copy = malloc(len);

  return copy;
}

int ParseRequestString(char const* reqStr,
		       unsigned reqStrSize,
		       char* resultCmdName,
		       unsigned resultCmdNameMaxSize,
		       char* resultURLPreSuffix,
		       unsigned resultURLPreSuffixMaxSize,
		       char* resultURLSuffix,
		       unsigned resultURLSuffixMaxSize,
		       char* resultCSeq,
		       unsigned resultCSeqMaxSize) 
{
  // This parser is currently rather dumb; it should be made smarter #####

  // Read everything up to the first space as the command name:
  	int parseSucceeded = FALSE;
  	unsigned i;
	for (i = 0; i < resultCmdNameMaxSize-1 && i < reqStrSize; ++i) 
	{
    	char c = reqStr[i];
		// '\t'���Ʊ��
    	if (c == ' ' || c == '\t') 
		{
      		parseSucceeded = TRUE;
      		break;
		}
    	resultCmdName[i] = c;	// ͨ��forѭ����pRecvBuf�е�����д��resultCmdName�У�
	}
	resultCmdName[i] = '\0';	// ��ÿ������������涼�����'\0'�������ַ�����

	if (!parseSucceeded) 
		return FALSE;
// rtsp://192.168.1.10:554/stream_chn0.h264
  // Skip over the prefix of any "rtsp://" or "rtsp:/" URL that follows:
  	unsigned j = i + 1;
  	while (j < reqStrSize && (reqStr[j] == ' ' || reqStr[j] == '\t')) 
		++j; // skip over any additional white space
  	for (j = i+1; j < reqStrSize-8; ++j) 
	{
   		if ((reqStr[j] == 'r' || reqStr[j] == 'R')
			&& (reqStr[j+1] == 't' || reqStr[j+1] == 'T')
			&& (reqStr[j+2] == 's' || reqStr[j+2] == 'S')
			&& (reqStr[j+3] == 'p' || reqStr[j+3] == 'P')
			&& reqStr[j+4] == ':' && reqStr[j+5] == '/')
		{
      		j += 6;
      		if (reqStr[j] == '/') 
			{
			// This is a "rtsp://" URL; skip over the host:port part that follows:
				++j;
				while (j < reqStrSize && reqStr[j] != '/' && reqStr[j] != ' ') 
				++j;
      		} 
			else 
			{
			// This is a "rtsp:/" URL; back up to the "/":
				--j;
     		}
      		i = j;
      	break;
    	}
  	}

  // Look for the URL suffix (before the following "RTSP/"):
  	parseSucceeded = FALSE;
 	unsigned k;
  	for (k = i+1; k < reqStrSize-5; ++k) 
	{
    	if (reqStr[k] == 'R' && reqStr[k+1] == 'T' &&
		reqStr[k+2] == 'S' && reqStr[k+3] == 'P' && reqStr[k+4] == '/') 
		{
      		while (--k >= i && reqStr[k] == ' ') 
				{} // go back over all spaces before "RTSP/"
      		unsigned k1 = k;
      		while (k1 > i && reqStr[k1] != '/' && reqStr[k1] != ' ') 
				--k1;
      		// the URL suffix comes from [k1+1,k]

      		// Copy "resultURLSuffix":
      		if (k - k1 + 1 > resultURLSuffixMaxSize) 
				return FALSE; // there's no room
      		unsigned n = 0, k2 = k1+1;
      		while (k2 <= k) 
				resultURLSuffix[n++] = reqStr[k2++];
      			resultURLSuffix[n] = '\0';

      		// Also look for the URL 'pre-suffix' before this:
      		unsigned k3 = --k1;
      		while (k3 > i && reqStr[k3] != '/' && reqStr[k3] != ' ') 
				--k3;
      		// the URL pre-suffix comes from [k3+1,k1]

      		// Copy "resultURLPreSuffix":
      		if (k1 - k3 + 1 > resultURLPreSuffixMaxSize) 
				return FALSE; // there's no room
      		n = 0; k2 = k3+1;
      		while (k2 <= k1) 
				resultURLPreSuffix[n++] = reqStr[k2++];
      			resultURLPreSuffix[n] = '\0';

      		i = k + 7; // to go past " RTSP/"
      		parseSucceeded = TRUE;
      		break;
    	}
  	}
  	if (!parseSucceeded) 
		return FALSE;

 	// Look for "CSeq:", skip whitespace,
  	// then read everything up to the next \r or \n as 'CSeq':
  	parseSucceeded = FALSE;
  	for (j = i; j < reqStrSize-5; ++j) 
	{
    	if (reqStr[j] == 'C' && reqStr[j+1] == 'S' && reqStr[j+2] == 'e' &&
			reqStr[j+3] == 'q' && reqStr[j+4] == ':') 
		{
      		j += 5;
      		unsigned n;
      		while (j < reqStrSize && (reqStr[j] ==  ' ' || reqStr[j] == '\t')) 
				++j;
      		for (n = 0; n < resultCSeqMaxSize - 1 && j < reqStrSize; ++n, ++j) 
			{
				char c = reqStr[j];
				if (c == '\r' || c == '\n') 
				{
	  				parseSucceeded = TRUE;
	  				break;
				}
				resultCSeq[n] = c;
      		}
      		resultCSeq[n] = '\0';
      		break;
    	}
  	}
  	if (!parseSucceeded) 
		return FALSE;

  	return TRUE;
}

int OptionAnswer(char *cseq, int sock)
{
	if (sock != 0)
	{
		char buf[1024];
		memset(buf,0,1024);
		char *pTemp = buf;
		pTemp += sprintf(pTemp,"RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sPublic: %s\r\n\r\n",
			cseq,dateHeader(),"OPTIONS,DESCRIBE,SETUP,PLAY,PAUSE,TEARDOWN");

		// ���send�Ƿ��������ͻ��˷��͵ģ�
		// sock�ǿͻ��˵�������������
		// ���send����response�����Ƿ�����response���ͻ��˵ģ�
		int reg = send(sock, buf, strlen(buf), 0);
		if(reg <= 0)
		{
			return FALSE;
		}
		else
		{
			printf(">>>>>%s\n", buf);
		}
		return TRUE;
	}
	return FALSE;
}

int DescribeAnswer(char *cseq,int sock,char * urlSuffix,char* recvbuf)
{
	if (sock != 0)
	{
		char sdpMsg[1024];
		char buf[2048];
		memset(buf, 0, 2048);
		memset(sdpMsg, 0, 1024);
		char *localip;
		localip = GetLocalIP(sock);	// ��ȡ�ͻ���IP��ַ���ַ���
		
		char *pTemp = buf;
		pTemp += sprintf(pTemp, "RTSP/1.0 200 OK\r\nCSeq: %s\r\n",cseq);
		pTemp += sprintf(pTemp, "%s", dateHeader());
		pTemp += sprintf(pTemp, "Content-Type: application/sdp\r\n");
		
		char *pTemp2 = sdpMsg;
		pTemp2 += sprintf(pTemp2, "v=0\r\n");
		pTemp2 += sprintf(pTemp2, "o=StreamingServer 3331435948 1116907222000 IN IP4 %s\r\n",localip);
		pTemp2 += sprintf(pTemp2, "s=H.264\r\n");
		pTemp2 += sprintf(pTemp2, "c=IN IP4 0.0.0.0\r\n");
		pTemp2 += sprintf(pTemp2, "t=0 0\r\n");
		pTemp2 += sprintf(pTemp2, "a=control:*\r\n");
		
		/*H264 TrackID=0 RTP_PT 96*/
		pTemp2 += sprintf(pTemp2, "m=video 0 RTP/AVP 96\r\n");
		pTemp2 += sprintf(pTemp2, "a=control:trackID=0\r\n");
		pTemp2 += sprintf(pTemp2, "a=rtpmap:96 H264/90000\r\n");
		pTemp2 += sprintf(pTemp2, "a=fmtp:96 packetization-mode=1; sprop-parameter-sets=%s\r\n", "AAABBCCC");
#if 1
		/*G726*/
		
		pTemp2 += sprintf(pTemp2, "m=audio 0 RTP/AVP 97\r\n");
		pTemp2 += sprintf(pTemp2, "a=control:trackID=1\r\n");
		if(strcmp(g_rtp_playload, "AAC") == 0)
		{
			pTemp2 += sprintf(pTemp2, "a=rtpmap:97 MPEG4-GENERIC/%d/2\r\n", 16000);
			pTemp2 += sprintf(pTemp2, "a=fmtp:97 streamtype=5;profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1410\r\n");
		}
		else
		{
			pTemp2 += sprintf(pTemp2, "a=rtpmap:97 G726-32/%d/1\r\n", 8000);
			pTemp2 += sprintf(pTemp2, "a=fmtp:97 packetization-mode=1\r\n");
		}	
#endif
		pTemp += sprintf(pTemp, "Content-length: %d\r\n", strlen(sdpMsg));     
		pTemp += sprintf(pTemp, "Content-Base: rtsp://%s/%s/\r\n\r\n", localip, urlSuffix);

		//printf("mem ready\n");
		// ��sdpMsg�е���Ϣ׷�ӵ�pTemp�ĺ��棻
		strcat(pTemp, sdpMsg);
		free(localip);			// GetLocalIP������malloc�ˣ�
		//printf("Describe ready sent\n");
		// ��buf�е����ݷ��͵��ͻ����У�ʹ�õ���TCP/IPЭ��
		int re = send(sock, buf, strlen(buf), 0);
		if(re <= 0)
		{
			return FALSE;
		}
		else
		{
			printf(">>>>>%s\n", buf);
		}
	}
	return TRUE;
}
void ParseTransportHeader(char const* buf,
						  StreamingMode* streamingMode,
						 char**streamingModeString,
						 char**destinationAddressStr,
						 u_int8_t* destinationTTL,
						 portNumBits* clientRTPPortNum, // if UDP
						 portNumBits* clientRTCPPortNum, // if UDP
						 unsigned char* rtpChannelId, // if TCP
						 unsigned char* rtcpChannelId // if TCP
						 )
 {
	// Initialize the result parameters to default values:
	*streamingMode = RTP_UDP;
	*streamingModeString = NULL;
	*destinationAddressStr = NULL;
	*destinationTTL = 255;
	*clientRTPPortNum = 0;
	*clientRTCPPortNum = 1; 
	*rtpChannelId = *rtcpChannelId = 0xFF;
	
	portNumBits p1, p2;
	unsigned ttl, rtpCid, rtcpCid;
	
	// First, find "Transport:"
	while (1) {
		if (*buf == '\0') return; // not found
		if (strncasecmp(buf, "Transport: ", 11) == 0) break;
		++buf;
	}
	
	// Then, run through each of the fields, looking for ones we handle:
	char const* fields = buf + 11;
	char* field = strDupSize(fields);
	while (sscanf(fields, "%[^;]", field) == 1) {
		if (strcmp(field, "RTP/AVP/TCP") == 0) {
			*streamingMode = RTP_TCP;
		} else if (strcmp(field, "RAW/RAW/UDP") == 0 ||
			strcmp(field, "MP2T/H2221/UDP") == 0) {
			*streamingMode = RAW_UDP;
			//*streamingModeString = strDup(field);
		} else if (strncasecmp(field, "destination=", 12) == 0)
		{
			//delete[] destinationAddressStr;
			free(destinationAddressStr);
			//destinationAddressStr = strDup(field+12);
		} else if (sscanf(field, "ttl%u", &ttl) == 1) {
			destinationTTL = (u_int8_t)ttl;
		} else if (sscanf(field, "client_port=%hu-%hu", &p1, &p2) == 2) {
			*clientRTPPortNum = p1;
			*clientRTCPPortNum = p2;
		} else if (sscanf(field, "client_port=%hu", &p1) == 1) {
			*clientRTPPortNum = p1;
			*clientRTCPPortNum = streamingMode == RAW_UDP ? 0 : p1 + 1;
		} else if (sscanf(field, "interleaved=%u-%u", &rtpCid, &rtcpCid) == 2) {
			*rtpChannelId = (unsigned char)rtpCid;
			*rtcpChannelId = (unsigned char)rtcpCid;
		}
		
		fields += strlen(field);
		while (*fields == ';') ++fields; // skip over separating ';' chars
		if (*fields == '\0' || *fields == '\r' || *fields == '\n') break;
	}
	free(field);
}


int SetupAnswer(char *cseq, int sock, int SessionId, char * urlSuffix, char* recvbuf, int* rtpport, int* rtcpport)
{
	if (sock != 0)
	{
		char buf[1024];
		memset(buf, 0, 1024);

		StreamingMode streamingMode;
		char* streamingModeString; // set when RAW_UDP streaming is specified
		char* clientsDestinationAddressStr;
		u_int8_t clientsDestinationTTL;
		portNumBits clientRTPPortNum, clientRTCPPortNum;
		unsigned char rtpChannelId, rtcpChannelId;
		// recvbuf���Ǵӿͻ����Ǳߵõ���һ�����ݣ�ͨ��ParseTransportHeader�������õ�
		// һ����������Ҫ����Ϣ������ʣ�¼������ζ�������Ͳ�����
		ParseTransportHeader(recvbuf, &streamingMode, &streamingModeString,
			&clientsDestinationAddressStr, &clientsDestinationTTL,
			&clientRTPPortNum, &clientRTCPPortNum,
			&rtpChannelId, &rtcpChannelId);

		//Port clientRTPPort(clientRTPPortNum);
		//Port clientRTCPPort(clientRTCPPortNum);
		*rtpport = clientRTPPortNum;
		*rtcpport = clientRTCPPortNum;

		char *pTemp = buf;
		char *localip;
		localip = GetLocalIP(sock);		// �ͻ���IP��ַ��
		pTemp += sprintf(pTemp,"RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sTransport: RTP/AVP;unicast;destination=%s;client_port=%d-%d;server_port=%d-%d\r\nSession: %d\r\n\r\n",
			cseq,dateHeader(),localip,
			ntohs(htons(clientRTPPortNum)), 
			ntohs(htons(clientRTCPPortNum)), 
			ntohs(2000),
			ntohs(2001),
			SessionId);

		free(localip);
		// ���send���͵���buf�������е����ݣ����߿ͻ��ˣ����յ���ĸ�����������ˣ�
		int reg = send(sock, buf, strlen(buf), 0);		// ���������ͻ���response
		if(reg <= 0)
		{
			return FALSE;
		}
		else
		{
			printf(">>>>>%s", buf);
		}
		return TRUE;
	}
	return FALSE;
}

int PlayAnswer(char *cseq, int sock,int SessionId,char* urlPre,char* recvbuf)
{
	if (sock != 0)
	{
		char buf[1024];
		memset(buf,0,1024);
		char *pTemp = buf;
		char*localip;
		localip = GetLocalIP(sock);
		pTemp += sprintf(pTemp,"RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sRange: npt=0.000-\r\nSession: %d\r\nRTP-Info: url=rtsp://%s/%s;seq=0\r\n\r\n",
			cseq,dateHeader(),SessionId,localip,urlPre);

		free(localip);

		// send�ĵ�һ������: sock��ָ�����Ͷ��׽�������������ʶ������Ҫ��buf�е�����
		// ���͵�sockָ�����Ǹ��׽����������У�
		int reg = send(sock, buf, strlen(buf), 0);
		if(reg <= 0)
		{
			return FALSE;
		}
		else
		{
			printf(">>>>>%s",buf);
			// �ͻ�������һ��PLAY���ͻ��˻�Ӧ����֮��
			// �������Ὠ��һ��udp��socket����ͻ��˷������ݣ�����UDP��ʽ�������ݴ��䣻
			udpfd = socket(AF_INET, SOCK_DGRAM, 0);// UDP
			struct sockaddr_in server;
			server.sin_family = AF_INET;
		   	server.sin_port = htons(g_rtspClients[0].rtpport[0]);          
		   	server.sin_addr.s_addr = inet_addr(g_rtspClients[0].IP);
			// �������˽������ӣ���ʼ��ͻ��˷������ݣ�
			connect(udpfd, (struct sockaddr *)&server, sizeof(server));
    		printf("udp up\n");
		}
		return TRUE;
	}
	return FALSE;
}

int PauseAnswer(char *cseq,int sock,char *recvbuf)
{
	if (sock != 0)
	{
		char buf[1024];
		memset(buf, 0, 1024);
		char *pTemp = buf;
		pTemp += sprintf(pTemp, "RTSP/1.0 200 OK\r\nCSeq: %s\r\n%s\r\n\r\n",
			cseq,dateHeader());

		int reg = send(sock, buf, strlen(buf), 0);
		if(reg <= 0)
		{
			return FALSE;
		}
		else
		{
			printf(">>>>>%s", buf);
		}
		return TRUE;
	}
	return FALSE;
}

int TeardownAnswer(char *cseq,int sock,int SessionId,char *recvbuf)
{
	if (sock != 0)
	{
		char buf[1024];
		memset(buf,0,1024);
		char *pTemp = buf;
		pTemp += sprintf(pTemp,"RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sSession: %d\r\n\r\n",
			cseq,dateHeader(),SessionId);
	
		int reg = send(sock, buf,strlen(buf),0);
		if(reg <= 0)
		{
			return FALSE;
		}
		else
		{
			printf(">>>>>%s",buf);
			close(udpfd);			// �����������ݣ���UDP��ͨ���رռ��ɣ�
		}
		return TRUE;
	}
	return FALSE;
}

// ��������: ���ÿ�����ӹ����Ŀͻ��ˣ�����ȥ����һ���ͻ����̣߳�
void * RtspClientMsg(void *pParam)
{
	pthread_detach(pthread_self());		// �߳��Լ������Լ��������߳������̷߳����ˣ�
	int nRes;
	char pRecvBuf[RTSP_RECV_SIZE];		// �������recv�������յ������ݵĻ�����
	// pParam��ʵ����g_rtspClientsȫ�ֱ����е���Ϣ����Щ��Ϣ���ڸú������ڵ��̺߳����Ĵ����У�
	// ��Щ�����ж��������õ���ʵ����socket(�ͻ���)��status(�ͻ�������״̬)��IP(�ͻ��˵�IP��ַ)��
	RTSP_CLIENT * pClient = (RTSP_CLIENT*)pParam;	// RTSP�ͻ��ˣ��ͻ��˵���Ϣ��������ṹ����
	memset(pRecvBuf, 0, sizeof(pRecvBuf));
	printf("RTSP:-----Create Client %s\n", pClient->IP);	// ��ӡ���ͻ���(windows)��IP��ַ��
	while(pClient->status != RTSP_IDLE)
	{
		// rev�൱��read�������������ݣ�����ֵ��ʵ��������ȡ�Ĵ�С��
		// �ͻ��˽��շ��������͹��������ݣ�
		nRes = recv(pClient->socket, pRecvBuf, RTSP_RECV_SIZE, 0);
		printf("-------------------%d\n",nRes);
		if(nRes < 1)	// ˵�����ǿͻ���û�ж������ݣ�
		{
			//usleep(1000);
			printf("RTSP:Recv Error--- %d\n",nRes);
			g_rtspClients[pClient->index].status = RTSP_IDLE;
			g_rtspClients[pClient->index].seqnum = 0;
			g_rtspClients[pClient->index].tsvid = 0;
			g_rtspClients[pClient->index].tsaud = 0;
			close(pClient->socket);
			break;
		}

		char cmdName[PARAM_STRING_MAX];			// ���������ʽ�е�method(option��setup��)
		char urlPreSuffix[PARAM_STRING_MAX];	// URL��ǰ׺(rtsp://)
		char urlSuffix[PARAM_STRING_MAX];		// URL(192.168.1.10:554/stream_chn0.h264)
		char cseq[PARAM_STRING_MAX];			// �ͻ��˵ı����

		// �������Ƕ�����Ե��ĸ��ַ������е����ݽ��н�����
		// ���е���Ϣ����ŵ���pRecvBuf�������У�
		ParseRequestString(pRecvBuf, nRes, cmdName, sizeof(cmdName), urlPreSuffix, sizeof(urlPreSuffix),
			urlSuffix, sizeof(urlSuffix), cseq, sizeof(cseq));
		
		char *p = pRecvBuf;		// pRecvBuf����ֵ��ʾ������Ԫ�ص��׵�ַ
		// OPTIONS rtsp://192.168.1.10:554/stream_chn0.h264 RTSP/1.0
		// CSeq: 2
		// User-Agent: LibVLC/2.2.6 (LIVE555 Streaming Media v2016.02.22)
		printf("<<<<<%s\n", p);

		//printf("\--------------------------\n");
		//printf("%s %s\n",urlPreSuffix,urlSuffix);

		// ���������ж��ַ���"OPTIONS"�Ƿ���cmdName���Ӵ�������ǣ���ú�������"OPTIONS"
		// ��cmdName���״γ��ֵĵ�ַ�����򣬷���ΪNULL��
		// OPTIONS����: ��ȡ������/�ͻ���֧�ֵ���������
		if(strstr(cmdName, "OPTIONS"))
		{
			// ���ݿͻ��˷��͵�����������ظ���Ӧ��response��
			// pClient->socket�ǿͻ��˵������ļ���������
			// ������������"OPTIONS"ʱ������������ͻ��˷�����Ӧ�����߿ͻ��ˣ��ҽ��յ����������
			OptionAnswer(cseq, pClient->socket);
		}
		// DESCRIBE��Ҫ����: �ӷ�������ȡ��ý���ļ���ʽ��Ϣ�ʹ�����Ϣ��
		// �ؼ��ֶ�: Content-Type��һ����SDP��Content-length��һ����SDP�ĳ��ȣ�
		// ����˵��: ý����Ϣͨ��SDPЭ�������
		else if(strstr(cmdName, "DESCRIBE"))
		{
			DescribeAnswer(cseq, pClient->socket, urlSuffix, p);
			//printf("-----------------------------DescribeAnswer %s %s\n",
			//	urlPreSuffix,urlSuffix);
		}
		// SETUP��Ҫ����: �������Э����ý�崫�䷽ʽ���˹����У�����RTPͨ����
		// �ؼ��ֶ�: Transport�������䷽ʽ
		else if(strstr(cmdName, "SETUP"))
		{
			int rtpport, rtcpport;
			int trackID = 0;
			SetupAnswer(cseq, pClient->socket, pClient->sessionid, urlPreSuffix, p, &rtpport, &rtcpport);
 
			sscanf(urlSuffix, "trackID=%u", &trackID);
			//printf("----------------------------------------------TrackId %d\n",trackID);
			if(trackID < 0 || trackID >= 2)
				trackID=0;

			g_rtspClients[pClient->index].rtpport[trackID] = rtpport;
			g_rtspClients[pClient->index].rtcpport = rtcpport;
			g_rtspClients[pClient->index].reqchn = atoi(urlPreSuffix);

			if(strlen(urlPreSuffix) < 100)
				strcpy(g_rtspClients[pClient->index].urlPre, urlPreSuffix);
			//printf("-----------------------------SetupAnswer %s-%d-%d\n",
			//	urlPreSuffix,g_rtspClients[pClient->index].reqchn,rtpport);
		}
		// PLAY��Ҫ����: �������Э��ý�岥�ţ�
		// �ؼ��ֶ�: Range��������ʱ�䣻
		else if(strstr(cmdName, "PLAY"))
		{
			PlayAnswer(cseq, pClient->socket, pClient->sessionid, g_rtspClients[pClient->index].urlPre, p);
			g_rtspClients[pClient->index].status = RTSP_SENDING;
			printf("Start Play\n", pClient->index);
			//printf("-----------------------------PlayAnswer %d %d\n",pClient->index);
			//usleep(100);
		}
		else if(strstr(cmdName, "PAUSE"))
		{
			PauseAnswer(cseq, pClient->socket, p);
		}
		else if(strstr(cmdName, "TEARDOWN"))
		{
			TeardownAnswer(cseq,pClient->socket,pClient->sessionid,p);
			g_rtspClients[pClient->index].status = RTSP_IDLE;
			g_rtspClients[pClient->index].seqnum = 0;
			g_rtspClients[pClient->index].tsvid = 0;
			g_rtspClients[pClient->index].tsaud = 0;
			// �رյ��Ƿ�����Ϊ�ͻ������������߳�
			close(pClient->socket);
		}
		//if(exitok){ exitok++;return NULL; } 
	}
	printf("RTSP:-----Exit Client %s\n",pClient->IP);
	return NULL;
}

// ��������ã�����TCP���紫��Э�飻����������Ľ�����Ȼ��һֱ��������������Σ���󴴽�һ���ͻ����߳�
void * RtspServerListen(void *pParam)
{
	int s32Socket;
	// �������ṹ��
	struct sockaddr_in servaddr;		// IPv4���������ĵ�ַ
	int s32CSocket;
    int s32Rtn;
    int s32Socket_opt_value = 1;
	int nAddrLen;
	// �ͻ��˽ṹ��
	struct sockaddr_in addrAccept;
	int bResult;

	memset(&servaddr, 0, sizeof(servaddr));
	// ���紫�������У�����������Ҫ����Ϣ��һ����IP��ַ��һ���Ƕ˿ںţ�
    servaddr.sin_family = AF_INET;					// ��ʾ��IPv4������
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);	// ������(Ҳ���ǿ�����)�˵�IP��ַ
    // htons��������: ���������޷��Ŷ�������ת���������ֽ�˳��
    // �����h����������Ҳ���ǿ������������
    servaddr.sin_port = htons(RTSP_SERVER_PORT); 	// �������Ķ˿ں�

	// ��һ��: ��socket���ļ�������
	// ����ļ����������ڼ����ļ���������һ�����������Լ����ܶ���ͻ��ˣ�
	s32Socket = socket(AF_INET, SOCK_STREAM, 0);	// TCP����Э��

	if (setsockopt(s32Socket, SOL_SOCKET, SO_REUSEADDR, &s32Socket_opt_value, sizeof(int)) == -1)     
    {
        return (void *)(-1);
    }
	// �ڶ�����bind��sockfd�͵�ǰ�������˵ĵ�ַ�Ͷ˿ں�
    s32Rtn = bind(s32Socket, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));
    if(s32Rtn < 0)
    {
        return (void *)(-2);
    }

	// �������еĳ��ȵĳ���Ϊ50��
	// ����������bindʱ�󶨵ĵ�ַ��
    s32Rtn = listen(s32Socket, 50);   
    if(s32Rtn < 0)
    {
         return (void *)(-2);
    }

	nAddrLen = sizeof(struct sockaddr_in);
	int nSessionId = 1000;

	printf("before accept...............\n");
	// �����������ʱ��������ӣ����û������������ô�����������
	// ����whileѭ����֤�����˷������������ˣ�
	// s32CSocket�ļ��������������������������������������Ϳͻ��˽������ӵģ����ж�д�����ģ�
    while ((s32CSocket = accept(s32Socket, (struct sockaddr*)&addrAccept, &nAddrLen)) >= 0)
    {
		// inet_ntoa: ��������IP��ַת��Ϊ���ʮ����IP��ַ
		// inet_ntoa(addrAccept.sin_addr)��ӡ�����ľ��ǿͻ��˵�IP��ַ������windows��IP��ַ��
		printf("<<<<RTSP Client %s Connected...\n", inet_ntoa(addrAccept.sin_addr));

		int nMaxBuf = 10 * 1024;
		if(setsockopt(s32CSocket, SOL_SOCKET, SO_SNDBUF, (char*)&nMaxBuf, sizeof(nMaxBuf)) == -1)
			printf("RTSP:!!!!!! Enalarge socket sending buffer error !!!!!!\n");
		int i;
		int bAdd = FALSE;
		// forѭ����Ϊ�˼���Ƿ񳬹�����������Ŀ��
		printf("wo shi mayue.\n");
		for(i=0; i<MAX_RTSP_CLIENT; i++)
		{
			printf("111111111111111.\n");
			if(g_rtspClients[i].status == RTSP_IDLE)
			{
				printf("22222222222222222.\n");
				memset(&g_rtspClients[i], 0, sizeof(RTSP_CLIENT));
				g_rtspClients[i].index = i;
				g_rtspClients[i].socket = s32CSocket;		// �����ļ�������
				g_rtspClients[i].status = RTSP_CONNECTED;	//RTSP_SENDING;
				g_rtspClients[i].sessionid = nSessionId++;
				// �ѿͻ��˵�IP�ŵ�g_rtspClients��
				strcpy(g_rtspClients[i].IP, inet_ntoa(addrAccept.sin_addr));
				pthread_t threadIdlsn = 0;

				struct sched_param sched;
				sched.sched_priority = 1;
				//to return ACKecho
				// ����һ���̣߳��߳�ID����threadIdlsn��
				printf("3333333333333333333333.\n");
				pthread_create(&threadIdlsn, NULL, RtspClientMsg, &g_rtspClients[i]);
				printf("4444444444444444444444.\n");
				//pthread_setschedparam(threadIdlsn,SCHED_RR,&sched);

				bAdd = TRUE;
				break;
			}
		}
		printf("wo shi song xiaohui.\n");
#if 0
		if(bAdd == FALSE)
		{
			memset(&g_rtspClients[0], 0, sizeof(RTSP_CLIENT));
			g_rtspClients[0].index = 0;
			g_rtspClients[0].socket = s32CSocket;
			g_rtspClients[0].status = RTSP_CONNECTED;	//RTSP_SENDING;
			g_rtspClients[0].sessionid = nSessionId++;
			strcpy(g_rtspClients[0].IP, inet_ntoa(addrAccept.sin_addr));
			pthread_t threadIdlsn = 0;
			struct sched_param sched;
			sched.sched_priority = 1;
			//to return ACKecho
			pthread_create(&threadIdlsn, NULL, RtspClientMsg, &g_rtspClients[0]);
			//pthread_setschedparam(threadIdlsn,SCHED_RR,&sched);
			bAdd = TRUE;
		}
#endif
		//if(exitok){ exitok++;return NULL; }
    }
    if(s32CSocket < 0)
    {
       // HI_OUT_Printf(0, "RTSP listening on port %d,accept err, %d\n", RTSP_SERVER_PORT, s32CSocket);
    }

	printf("----- INIT_RTSP_Listen() Exit !! \n");
	
	return NULL;
}

// ���������������ִ�з��͵ĺ���
HI_S32 VENC_Sent(char *buffer,int buflen)
{
    HI_S32 i;
	int is = 0;
	int nChanNum = 0;

	for(is=0; is<MAX_RTSP_CLIENT; is++)
	{
		if(g_rtspClients[is].status! = RTSP_SENDING)
		{
		    continue;	// ������������ô��ֱ������������������ݣ�ֱ�ӻص�forѭ���ʼ���֣�
		}
		int heart = g_rtspClients[is].seqnum % 10000;	// �ۼӵ�10000�������¿�ʼ��
		
		char* nalu_payload;
		int nAvFrmLen = 0;
		int nIsIFrm = 0;
		int nNaluType = 0;
		char sendbuf[500 * 1024 + 32];

		// ������������Ƶ�ģ�����Ƶ���ݵ�һ֡���ȣ�
		nAvFrmLen = buflen;

		// ʹ��UDP���ͣ���Ҫ�����������Ϳͻ��˵���Ч��Ϣ��
		// �����Ƿ���������Ϣ��ָ��Ҫ�����������������Ŀͻ��˵Ķ˿ںź�IP��ַ��
		struct sockaddr_in server;
		server.sin_family = AF_INET;
	   	server.sin_port = htons(g_rtspClients[is].rtpport[0]);	// �����client�Ǳߵ�port�˿ں�  
	   	server.sin_addr.s_addr = inet_addr(g_rtspClients[is].IP);	// ���Ҳ��client�Ǳߵ�address��ַ
		int	bytes = 0;
		unsigned int timestamp_increse = 0;	// ��ʱ��Ƶ�ʣ�clock rate�����������ʾʱ���

		timestamp_increse = (unsigned int)(90000.0 / 25);	// timestamp_increse=ʱ��Ƶ�� / ֡��

		// RTP�̶���ͷ��Ϊ12�ֽڣ�
		rtp_hdr = (RTP_FIXED_HEADER *)&sendbuf[0];

		rtp_hdr->payload = RTP_H264;   
		rtp_hdr->version = 2;          	// RTP�İ汾��
		rtp_hdr->marker = 0;           	// ��ǵ�ǰλ���ǲ���һ֡���ݵ����һ��
		rtp_hdr->ssrc = htonl(10);		// ��Դ��ǣ������ڱ�RTP�Ự��ȫ��Ψһ

		if(nAvFrmLen <= nalu_sent_len)
		{
			rtp_hdr->marker = 1;		// ���һ�������е�marker�ǵ���1�ģ�
			rtp_hdr->seq_no = htons(g_rtspClients[is].seqnum++); // ���кţ�ÿ����һ��RTP����1
			nalu_hdr = (NALU_HEADER*)&sendbuf[12]; 
			nalu_hdr->F = 0; 
			nalu_hdr->NRI = nIsIFrm; 
			nalu_hdr->TYPE = nNaluType;
			nalu_payload = &sendbuf[13];
			memcpy(nalu_payload, buffer, nAvFrmLen);
            g_rtspClients[is].tsvid = g_rtspClients[is].tsvid + timestamp_increse;            
			rtp_hdr->timestamp = htonl(g_rtspClients[is].tsvid);
			bytes = nAvFrmLen + 13;
			// ͨ��������������ݷ��͵ģ��ӷ��������͵��ͻ��ˣ�
			// sendbuf�а�������Ƶ���Լ���ͷ��Ϣ��
			sendto(udpfd, sendbuf, bytes, 0, (struct sockaddr *)&server, sizeof(server));
		}
		else if(nAvFrmLen > nalu_sent_len)
		{
			int k = 0, l = 0;
			k = nAvFrmLen / nalu_sent_len;		// ��������
			l = nAvFrmLen % nalu_sent_len;		// �������Ĵ�С
			int t = 0;        

            g_rtspClients[is].tsvid = g_rtspClients[is].tsvid + timestamp_increse;
			// ��tsvidת���������ֽ�˳��
            rtp_hdr->timestamp = htonl(g_rtspClients[is].tsvid); 
			while(t <= k)
			{
				rtp_hdr->seq_no = htons(g_rtspClients[is].seqnum++);
				if(t == 0)	// ��������ĵ�һ����
				{
					rtp_hdr->marker = 0;
					fu_ind = (FU_INDICATOR*)&sendbuf[12];	// ��Ϊrtp_hdrռ����0-11��λ��
					fu_ind->F = 0; 
					fu_ind->NRI = nIsIFrm;
					fu_ind->TYPE = 28;		// FU-A����Ƭ�ĵ�Ԫ��

					fu_hdr = (FU_HEADER*)&sendbuf[13];
					fu_hdr->E = 0;
					fu_hdr->R = 0;
					fu_hdr->S = 1;
					fu_hdr->TYPE = nNaluType;

					nalu_payload = &sendbuf[14];
					// buffer�д�ŵ���H.264 Stream�����ݣ�
					// �������͵�һ�������оͰ�����RTP��ͷ��H.264�����ݣ�
					memcpy(nalu_payload, buffer, nalu_sent_len);

					bytes = nalu_sent_len + 14;
					// ����������ˣ�����sendto�����з������ݣ�
					// ���͵�һ�����ݣ�
					sendto(udpfd, sendbuf, bytes, 0, (struct sockaddr *)&server, sizeof(server));
					t++;
				}
				else if(k == t)	// ������������һ��������
				{
					rtp_hdr->marker = 1;		// =1��ʾ�������֡�����һ�����ݰ���
					fu_ind = (FU_INDICATOR*)&sendbuf[12]; 
					fu_ind->F = 0 ;
					fu_ind->NRI = nIsIFrm ;
					fu_ind->TYPE = 28;

					fu_hdr =(FU_HEADER*)&sendbuf[13];
					fu_hdr->R = 0;
					fu_hdr->S = 0;
					fu_hdr->TYPE = nNaluType;
					fu_hdr->E = 1;
					nalu_payload = &sendbuf[14];
					memcpy(nalu_payload, buffer + t*nalu_sent_len, l);
					bytes = l + 14;
					sendto(udpfd, sendbuf, bytes, 0, (struct sockaddr *)&server, sizeof(server));
					t++;
				}
				else if(t < k && t != 0)	// ����ǳ��˵�һ�������һ����ʣ�µ���Щ������
				{
					rtp_hdr->marker = 0;	// �������һ������ʱmarker����0��
					fu_ind = (FU_INDICATOR*)&sendbuf[12]; 
					fu_ind->F = 0; 
					fu_ind->NRI = nIsIFrm;
					fu_ind->TYPE = 28;
					fu_hdr = (FU_HEADER*)&sendbuf[13];
					//fu_hdr->E=0;
					fu_hdr->R = 0;
					fu_hdr->S = 0;
					fu_hdr->E = 0;
					fu_hdr->TYPE = nNaluType;
					nalu_payload = &sendbuf[14];
					memcpy(nalu_payload, buffer + t * nalu_sent_len, nalu_sent_len);
					bytes = nalu_sent_len + 14;	
					sendto(udpfd, sendbuf, bytes, 0, (struct sockaddr *)&server, sizeof(server));
					t++;
				}
			}
		}

	}

	//------------------------------------------------------------
}
/******************************************************************************
* funciton : sent H264 stream
******************************************************************************/

HI_S32 SAMPLE_COMM_VENC_Sentjin(VENC_STREAM_S *pstStream)
{
    HI_S32 i, flag = 0;

    for(i=0; i<MAX_RTSP_CLIENT; i++)//have atleast a connect
    {
		if(g_rtspClients[i].status == RTSP_SENDING)
		{
		    flag = 1;
		    break;
		}
    }
    if(flag)	// flag����1�Ż���з��ͣ�Ҳ���ǽ���if��
    {
	    for (i = 0; i < pstStream->u32PackCount; i++)
	    {
			HI_S32 lens=0, j, lastadd = 0, newadd = 0, showflap = 0;
			char sendbuf[320 * 1024];
			//char tmp[640*1024];
			lens = pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset;
			memcpy(&sendbuf[0], pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset, lens);
			//printf("lens = %d, count= %d\n",lens,count++);
			VENC_Sent(sendbuf, lens);		
			lens = 0;
	    }
    }
    return HI_SUCCESS;
}

// ��״buffer��������
HI_S32 saveStream(VENC_STREAM_S *pstStream)
{
    HI_S32 i, j, lens = 0;

    for(j=0; j<MAX_RTSP_CLIENT; j++) //have atleast a connect
    {
		if(g_rtspClients[j].status == RTSP_SENDING)	// �������playanswear������Ϊ����ģʽ�ģ�
		{
		    for (i = 0; i < pstStream->u32PackCount; i++)
		    {
				// RTPbuf_s����������ʵ�ֵģ�����ʹ��ǰҪ�����ڴ棬����һ������ڵ㣻
				RTPbuf_s *p = (RTPbuf_s *)malloc(sizeof(RTPbuf_s));
				INIT_LIST_HEAD(&(p->list));	// �����������ڵ���г�ʼ����

				// ��RTPbuf_s�ṹ���е����ݽ�����䣻
				lens = pstStream->pstPack[i].u32Len-pstStream->pstPack[i].u32Offset;
				// �����������洢���ݵ���������ڴ����룻
				p->buf = (char *)malloc(lens);
				p->len = lens;
				// ����Ч���ݸ��Ƶ����Ǹո����뵽���ڴ������У�
				memcpy(p->buf, pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset, lens);

				// ����������ڵ���ӵ����������β�ڵ��У�
				list_add_tail(&(p->list), &RTPbuf_head);
				count++;		// ������¼��ǰ��״buffer�м�����Ч���ݣ�
				//printf("count = %d\n",count);
		    }
    	}
    }

    return HI_SUCCESS;
}


void RtspServer_init(void)
{
	int i;
	pthread_t threadId = 0;

	// RTSPʵ����������ͨ��RTP�������з��͵ģ�
	memset(&g_rtp_playload, 0, sizeof(g_rtp_playload));
	strcpy(&g_rtp_playload, "G726-32");	// "G726-32"�Ǵ�live555����ֲ������
//	g_audio_rate = 8000;
	pthread_mutex_init(&g_sendmutex, NULL);
	pthread_mutex_init(&g_mutex, NULL);
	pthread_cond_init(&g_cond, NULL);
	memset(&g_rtspClients, 0, sizeof(RTSP_CLIENT) * MAX_RTSP_CLIENT);

	//pthread_create(&g_SendDataThreadId, NULL, SendDataThread, NULL);

//	struct sched_param thdsched;
//	thdsched.sched_priority = 2;	// �̵߳������ȼ�
	//to listen visiting ����һ�������������߳�
	// ����������̣߳�����ֻ�Ǹ�����һ��UDPͨ����
	pthread_create(&threadId, NULL, RtspServerListen, NULL);
	//pthread_setschedparam(threadId,SCHED_RR,&thdsched);
	printf("RTSP:-----Init Rtsp server\n");

	// ����һ��RTP�����̣߳��̺߳���vdRTPSendThread�����if��䣬�������в�Ϊ��ʱ��
	// ˵���������Ǳ��пͻ��������ˣ����������Ҫ���������ˣ����������Ϊ��ʱ��˵��
	// �������Ǳ߻�û�����ݲ��������Բ��ᷢ�����ݣ����if����ڲ�Ҳ����ִ�У�
	pthread_create(&gs_RtpPid, 0, vdRTPSendThread, NULL);
	printf("after rtp send.\n");
	
	//exitok++;
}
void RtspServer_exit(void)
{
	return;
}

// ��������: ������ý������
HI_VOID* vdRTPSendThread(HI_VOID *p)
{
	while(1)
	{
		if(!list_empty(&RTPbuf_head))	// �жϻ�״buffer�Ƿ�Ϊ�գ���Ϊ����ִ��if�е���䣻
		{
			printf("before rtp send thread..................\n");
			// ���������ҵ���һ���ǿսڵ㣻
			RTPbuf_s *p = get_first_item(&RTPbuf_head, RTPbuf_s, list);
			// ����VENC_Sent����һ���ǿսڵ��е����ݷ��ͳ�ȥ��
			VENC_Sent(p->buf, p->len);
			list_del(&(p->list));	// �������˺�Ҫ���������ڵ������������ȥ����
			free(p->buf);			// ��������ڴ涼�ͷŵ�
			free(p);				// ��������ڴ涼�ͷŵ�
			p = NULL;				// ָ��NULL����ֹҰָ��
			count--;
			printf("after rtp send thread..................\n");
			//printf("count = %d\n",count);
		}
		usleep(5000);
	}
}

#endif


/******************************************************************************
* function :  H.264@1080p@30fps+H.264@VGA@30fps


******************************************************************************/
HI_S32 SAMPLE_VENC_720P_CLASSIC(HI_VOID)
{
	PAYLOAD_TYPE_E enPayLoad[3] = {PT_H264, PT_H264,PT_H264};
	PIC_SIZE_E enSize[3] = {PIC_HD720, PIC_VGA,PIC_QVGA};
	HI_U32 u32Profile = 0;

	VB_CONF_S stVbConf;
	SAMPLE_VI_CONFIG_S stViConfig = {0};

	VPSS_GRP VpssGrp;
	VPSS_CHN VpssChn;
	VPSS_GRP_ATTR_S stVpssGrpAttr;
	VPSS_CHN_ATTR_S stVpssChnAttr;
	VPSS_CHN_MODE_S stVpssChnMode;

	VENC_CHN VencChn;
	SAMPLE_RC_E enRcMode = SAMPLE_RC_FIXQP;

	HI_S32 s32ChnNum = 1;

	HI_S32 s32Ret = HI_SUCCESS;
	HI_U32 u32BlkSize;
	SIZE_S stSize;


	/******************************************
	 step  1: init sys variable 
	******************************************/
	memset(&stVbConf, 0, sizeof(VB_CONF_S));
	printf("s32ChnNum = %d\n", s32ChnNum);

	stVbConf.u32MaxPoolCnt = 128;

	/*video buffer*/

	u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm,\
	            enSize[0], SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);
	stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
	stVbConf.astCommPool[0].u32BlkCnt = g_u32BlkCnt;

	/******************************************
	 step 2: mpp system init. 
	******************************************/
	s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("system init failed with %d!\n", s32Ret);
		goto END_VENC_720P_CLASSIC_0;
	}

	/******************************************
	 step 3: start vi dev & chn to capture
	******************************************/
	stViConfig.enViMode   = SENSOR_TYPE;
	stViConfig.enRotate   = ROTATE_NONE;
	stViConfig.enNorm     = VIDEO_ENCODING_MODE_AUTO;
	stViConfig.enViChnSet = VI_CHN_SET_NORMAL;
	stViConfig.enWDRMode  = WDR_MODE_NONE;
	s32Ret = SAMPLE_COMM_VI_StartVi(&stViConfig);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("start vi failed!\n");
		goto END_VENC_720P_CLASSIC_1;
	}
    
	/******************************************
	 step 4: start vpss and vi bind vpss
	******************************************/
	s32Ret = SAMPLE_COMM_SYS_GetPicSize(gs_enNorm, enSize[0], &stSize);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
		goto END_VENC_720P_CLASSIC_1;
	}

	VpssGrp = 0;
	stVpssGrpAttr.u32MaxW = stSize.u32Width;
	stVpssGrpAttr.u32MaxH = stSize.u32Height;
	stVpssGrpAttr.bIeEn = HI_FALSE;
	stVpssGrpAttr.bNrEn = HI_TRUE;
	stVpssGrpAttr.bHistEn = HI_FALSE;
	stVpssGrpAttr.bDciEn = HI_FALSE;
	stVpssGrpAttr.enDieMode = VPSS_DIE_MODE_NODIE;
	stVpssGrpAttr.enPixFmt = PIXEL_FORMAT_YUV_SEMIPLANAR_420;

	s32Ret = SAMPLE_COMM_VPSS_StartGroup(VpssGrp, &stVpssGrpAttr);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("Start Vpss failed!\n");
		goto END_VENC_720P_CLASSIC_2;
	}

	s32Ret = SAMPLE_COMM_VI_BindVpss(stViConfig.enViMode);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("Vi bind Vpss failed!\n");
		goto END_VENC_720P_CLASSIC_3;
	}

	VpssChn = 0;
	stVpssChnMode.enChnMode      = VPSS_CHN_MODE_USER;
	stVpssChnMode.bDouble        = HI_FALSE;
	stVpssChnMode.enPixelFormat  = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
	stVpssChnMode.u32Width       = stSize.u32Width;
	stVpssChnMode.u32Height      = stSize.u32Height;
	stVpssChnMode.enCompressMode = COMPRESS_MODE_SEG;
	memset(&stVpssChnAttr, 0, sizeof(stVpssChnAttr));
	stVpssChnAttr.s32SrcFrameRate = -1;
	stVpssChnAttr.s32DstFrameRate = -1;
	enRcMode = SAMPLE_RC_VBR;
	s32Ret = SAMPLE_COMM_VPSS_EnableChn(VpssGrp, VpssChn, &stVpssChnAttr, &stVpssChnMode, HI_NULL);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("Enable vpss chn failed!\n");
		goto END_VENC_720P_CLASSIC_4;
	}
	
	/******************************************
	step 5: start stream venc H.264���벿��
	******************************************/
	/*** enSize[0] **/

	VpssGrp = 0;
	VpssChn = 0;
	VencChn = 0;
	s32Ret = SAMPLE_COMM_VENC_Start(VencChn, enPayLoad[0],\
	                               gs_enNorm, enSize[0], enRcMode,u32Profile);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("Start Venc failed!\n");
		goto END_VENC_720P_CLASSIC_5;
	}

	s32Ret = SAMPLE_COMM_VENC_BindVpss(VencChn, VpssGrp, VpssChn);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("Start Venc failed!\n");
		goto END_VENC_720P_CLASSIC_5;
	}

	/******************************************
	 step 6: stream venc process -- get stream, then save it to file. 
	 H.264��Ƶ����������벥�Ų��֣�
	******************************************/
	s32Ret = SAMPLE_COMM_VENC_StartGetStream(s32ChnNum);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("Start Venc failed!\n");
		goto END_VENC_720P_CLASSIC_5;
	}

	printf("please press twice ENTER to exit this sample\n");
	
       //return s32Ret;  

	getchar();
	getchar();

	/******************************************
	 step 7: exit process
	******************************************/
	SAMPLE_COMM_VENC_StopGetStream();

END_VENC_720P_CLASSIC_5:	
	VpssGrp = 0;
	VpssChn = 0;  
	VencChn = 0;
	SAMPLE_COMM_VENC_UnBindVpss(VencChn, VpssGrp, VpssChn);
	SAMPLE_COMM_VENC_Stop(VencChn);
	SAMPLE_COMM_VI_UnBindVpss(stViConfig.enViMode);
END_VENC_720P_CLASSIC_4:	//vpss stop
	VpssGrp = 0;
	VpssChn = 0;
	SAMPLE_COMM_VPSS_DisableChn(VpssGrp, VpssChn);	
END_VENC_720P_CLASSIC_3:    //vpss stop       
	SAMPLE_COMM_VI_UnBindVpss(stViConfig.enViMode);
END_VENC_720P_CLASSIC_2:    //vpss stop   
	SAMPLE_COMM_VPSS_StopGroup(VpssGrp);
END_VENC_720P_CLASSIC_1:	//vi stop
   	SAMPLE_COMM_VI_StopVi(&stViConfig);
END_VENC_720P_CLASSIC_0:	//system exit
    	SAMPLE_COMM_SYS_Exit();

    return s32Ret;    
}

/******************************************************************************
* function    : main()
* Description : video venc sample
******************************************************************************/
int main(int argc, char *argv[])
{
	HI_S32 s32Ret;
	MPP_VERSION_S mppVersion;

	HI_MPI_SYS_GetVersion(&mppVersion);

	printf("MPP Ver  %s\n", mppVersion.aVersion);
	printf("before server init.\n");
	RtspServer_init();
	printf("after server init.\n");
	s32Ret = SAMPLE_VENC_720P_CLASSIC();
	printf("after sample venc.\n");
	return HI_FAILURE;

#if 0
	if (HI_SUCCESS == s32Ret)
	    	printf("program exit normally!\n");
	else
	    	printf("program exit abnormally!\n");
	while(1)
	{usleep(1000);}
	return s32Ret;
#endif
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
