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

// 全局变量没有初始化，默认值为0
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
		// '\t'是制表符
    	if (c == ' ' || c == '\t') 
		{
      		parseSucceeded = TRUE;
      		break;
		}
    	resultCmdName[i] = c;	// 通过for循环把pRecvBuf中的内容写到resultCmdName中；
	}
	resultCmdName[i] = '\0';	// 在每个命令命令后面都添加上'\0'，当做字符串；

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

		// 这个send是服务器给客户端发送的；
		// sock是客户端的网络描述符；
		// 这个send就是response；就是服务器response给客户端的；
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
		localip = GetLocalIP(sock);	// 获取客户端IP地址的字符串
		
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
		// 把sdpMsg中的信息追加到pTemp的后面；
		strcat(pTemp, sdpMsg);
		free(localip);			// GetLocalIP函数中malloc了；
		//printf("Describe ready sent\n");
		// 把buf中的数据发送到客户端中；使用的是TCP/IP协议
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
		// recvbuf就是从客户端那边得到的一包数据；通过ParseTransportHeader函数来得到
		// 一包数据中想要的信息，它的剩下几个传参都是输出型参数；
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
		localip = GetLocalIP(sock);		// 客户端IP地址；
		pTemp += sprintf(pTemp,"RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sTransport: RTP/AVP;unicast;destination=%s;client_port=%d-%d;server_port=%d-%d\r\nSession: %d\r\n\r\n",
			cseq,dateHeader(),localip,
			ntohs(htons(clientRTPPortNum)), 
			ntohs(htons(clientRTCPPortNum)), 
			ntohs(2000),
			ntohs(2001),
			SessionId);

		free(localip);
		// 这个send发送的是buf缓冲区中的内容，告诉客户端，我收到你的该命令的请求了；
		int reg = send(sock, buf, strlen(buf), 0);		// 服务器给客户端response
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

		// send的第一个参数: sock是指定发送端套接字描述符，意识就是我要将buf中的数据
		// 发送到sock指定的那个套接字描述符中；
		int reg = send(sock, buf, strlen(buf), 0);
		if(reg <= 0)
		{
			return FALSE;
		}
		else
		{
			printf(">>>>>%s",buf);
			// 客户端请求一个PLAY，客户端回应请求之后，
			// 服务器会建立一个udp的socket，向客户端发送数据，采用UDP方式进行数据传输；
			udpfd = socket(AF_INET, SOCK_DGRAM, 0);// UDP
			struct sockaddr_in server;
			server.sin_family = AF_INET;
		   	server.sin_port = htons(g_rtspClients[0].rtpport[0]);          
		   	server.sin_addr.s_addr = inet_addr(g_rtspClients[0].IP);
			// 服务器端建立连接，开始向客户端发送数据；
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
			close(udpfd);			// 结束发送数据，把UDP的通道关闭即可；
		}
		return TRUE;
	}
	return FALSE;
}

// 函数作用: 针对每个连接过来的客户端，都会去创建一个客户端线程；
void * RtspClientMsg(void *pParam)
{
	pthread_detach(pthread_self());		// 线程自己回收自己，该子线程与主线程分离了；
	int nRes;
	char pRecvBuf[RTSP_RECV_SIZE];		// 用来存放recv函数接收到的数据的缓冲区
	// pParam其实就是g_rtspClients全局变量中的信息，这些信息是在该函数所在的线程函数的传参中；
	// 这些传参中对我们有用的其实就是socket(客户端)，status(客户端所处状态)，IP(客户端的IP地址)；
	RTSP_CLIENT * pClient = (RTSP_CLIENT*)pParam;	// RTSP客户端，客户端的信息都在这个结构体中
	memset(pRecvBuf, 0, sizeof(pRecvBuf));
	printf("RTSP:-----Create Client %s\n", pClient->IP);	// 打印出客户端(windows)的IP地址；
	while(pClient->status != RTSP_IDLE)
	{
		// rev相当于read，用来接收数据，返回值是实际真正读取的大小；
		// 客户端接收服务器发送过来的数据；
		nRes = recv(pClient->socket, pRecvBuf, RTSP_RECV_SIZE, 0);
		printf("-------------------%d\n",nRes);
		if(nRes < 1)	// 说明我们客户端没有读到数据；
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

		char cmdName[PARAM_STRING_MAX];			// 就是请求格式中的method(option、setup等)
		char urlPreSuffix[PARAM_STRING_MAX];	// URL的前缀(rtsp://)
		char urlSuffix[PARAM_STRING_MAX];		// URL(192.168.1.10:554/stream_chn0.h264)
		char cseq[PARAM_STRING_MAX];			// 客户端的标记码

		// 函数就是对上面对的四个字符数组中的内容进行解析；
		// 所有的信息都存放到了pRecvBuf缓冲区中；
		ParseRequestString(pRecvBuf, nRes, cmdName, sizeof(cmdName), urlPreSuffix, sizeof(urlPreSuffix),
			urlSuffix, sizeof(urlSuffix), cseq, sizeof(cseq));
		
		char *p = pRecvBuf;		// pRecvBuf做右值表示数组首元素的首地址
		// OPTIONS rtsp://192.168.1.10:554/stream_chn0.h264 RTSP/1.0
		// CSeq: 2
		// User-Agent: LibVLC/2.2.6 (LIVE555 Streaming Media v2016.02.22)
		printf("<<<<<%s\n", p);

		//printf("\--------------------------\n");
		//printf("%s %s\n",urlPreSuffix,urlSuffix);

		// 函数用于判断字符串"OPTIONS"是否是cmdName的子串。如果是，则该函数返回"OPTIONS"
		// 在cmdName中首次出现的地址；否则，返回为NULL；
		// OPTIONS功能: 获取服务器/客户端支持的能力集；
		if(strstr(cmdName, "OPTIONS"))
		{
			// 根据客户端发送的命令，服务器回复相应的response；
			// pClient->socket是客户端的网络文件描述符；
			// 当服务器遇到"OPTIONS"时，服务器会给客户端发送响应，告诉客户端，我接收到了你的请求；
			OptionAnswer(cseq, pClient->socket);
		}
		// DESCRIBE主要功能: 从服务器获取流媒体文件格式信息和传输信息；
		// 关键字段: Content-Type：一般是SDP；Content-length：一般是SDP的长度；
		// 特殊说明: 媒体信息通过SDP协议给出；
		else if(strstr(cmdName, "DESCRIBE"))
		{
			DescribeAnswer(cseq, pClient->socket, urlSuffix, p);
			//printf("-----------------------------DescribeAnswer %s %s\n",
			//	urlPreSuffix,urlSuffix);
		}
		// SETUP主要功能: 与服务器协商流媒体传输方式，此过程中，建立RTP通道；
		// 关键字段: Transport——传输方式
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
		// PLAY主要功能: 与服务器协商媒体播放；
		// 关键字段: Range——播放时间；
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
			// 关闭的是服务器为客户端所建立的线程
			close(pClient->socket);
		}
		//if(exitok){ exitok++;return NULL; } 
	}
	printf("RTSP:-----Exit Client %s\n",pClient->IP);
	return NULL;
}

// 服务端设置，采用TCP网络传输协议；网络服务器的建立，然后一直到服务器监听这段，最后创建一个客户端线程
void * RtspServerListen(void *pParam)
{
	int s32Socket;
	// 服务器结构体
	struct sockaddr_in servaddr;		// IPv4，服务器的地址
	int s32CSocket;
    int s32Rtn;
    int s32Socket_opt_value = 1;
	int nAddrLen;
	// 客户端结构体
	struct sockaddr_in addrAccept;
	int bResult;

	memset(&servaddr, 0, sizeof(servaddr));
	// 网络传输数据中，有两个很重要的信息，一个是IP地址，一个是端口号；
    servaddr.sin_family = AF_INET;					// 表示用IPv4的网络
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);	// 服务器(也就是开发板)端的IP地址
    // htons函数作用: 将主机的无符号短整形数转换成网络字节顺序；
    // 这里的h就是主机，也就是开发板服务器；
    servaddr.sin_port = htons(RTSP_SERVER_PORT); 	// 服务器的端口号

	// 第一步: 先socket打开文件描述符
	// 这个文件描述符属于监听文件描述符，一个服务器可以监听很多个客户端；
	s32Socket = socket(AF_INET, SOCK_STREAM, 0);	// TCP传输协议

	if (setsockopt(s32Socket, SOL_SOCKET, SO_REUSEADDR, &s32Socket_opt_value, sizeof(int)) == -1)     
    {
        return (void *)(-1);
    }
	// 第二步：bind绑定sockfd和当前服务器端的地址和端口号
    s32Rtn = bind(s32Socket, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));
    if(s32Rtn < 0)
    {
        return (void *)(-2);
    }

	// 监听队列的长度的长度为50；
	// 监听我们在bind时绑定的地址；
    s32Rtn = listen(s32Socket, 50);   
    if(s32Rtn < 0)
    {
         return (void *)(-2);
    }

	nAddrLen = sizeof(struct sockaddr_in);
	int nSessionId = 1000;

	printf("before accept...............\n");
	// 有连接请求的时候进行连接；如果没有连接请求，那么就阻塞在这里；
	// 进入while循环，证明有人发送连接请求了；
	// s32CSocket文件描述符是属于连接描述符，用来将服务器和客户端进行连接的，进行读写操作的；
    while ((s32CSocket = accept(s32Socket, (struct sockaddr*)&addrAccept, &nAddrLen)) >= 0)
    {
		// inet_ntoa: 将二进制IP地址转换为点分十进制IP地址
		// inet_ntoa(addrAccept.sin_addr)打印出来的就是客户端的IP地址，就是windows的IP地址；
		printf("<<<<RTSP Client %s Connected...\n", inet_ntoa(addrAccept.sin_addr));

		int nMaxBuf = 10 * 1024;
		if(setsockopt(s32CSocket, SOL_SOCKET, SO_SNDBUF, (char*)&nMaxBuf, sizeof(nMaxBuf)) == -1)
			printf("RTSP:!!!!!! Enalarge socket sending buffer error !!!!!!\n");
		int i;
		int bAdd = FALSE;
		// for循环是为了检查是否超过最大可连接数目；
		printf("wo shi mayue.\n");
		for(i=0; i<MAX_RTSP_CLIENT; i++)
		{
			printf("111111111111111.\n");
			if(g_rtspClients[i].status == RTSP_IDLE)
			{
				printf("22222222222222222.\n");
				memset(&g_rtspClients[i], 0, sizeof(RTSP_CLIENT));
				g_rtspClients[i].index = i;
				g_rtspClients[i].socket = s32CSocket;		// 连接文件描述符
				g_rtspClients[i].status = RTSP_CONNECTED;	//RTSP_SENDING;
				g_rtspClients[i].sessionid = nSessionId++;
				// 把客户端的IP放到g_rtspClients中
				strcpy(g_rtspClients[i].IP, inet_ntoa(addrAccept.sin_addr));
				pthread_t threadIdlsn = 0;

				struct sched_param sched;
				sched.sched_priority = 1;
				//to return ACKecho
				// 创建一个线程，线程ID号是threadIdlsn；
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

// 这个函数是真正的执行发送的函数
HI_S32 VENC_Sent(char *buffer,int buflen)
{
    HI_S32 i;
	int is = 0;
	int nChanNum = 0;

	for(is=0; is<MAX_RTSP_CLIENT; is++)
	{
		if(g_rtspClients[is].status! = RTSP_SENDING)
		{
		    continue;	// 如果进到这里，那么就直接跳过下面的所有内容，直接回到for循环最开始部分；
		}
		int heart = g_rtspClients[is].seqnum % 10000;	// 累加到10000后，又重新开始了
		
		char* nalu_payload;
		int nAvFrmLen = 0;
		int nIsIFrm = 0;
		int nNaluType = 0;
		char sendbuf[500 * 1024 + 32];

		// 可以用于音视频的，音视频数据的一帧长度；
		nAvFrmLen = buflen;

		// 使用UDP发送，需要包括服务器和客户端的有效信息；
		// 这里是服务器的信息，指定要连接你的这个服务器的客户端的端口号和IP地址；
		struct sockaddr_in server;
		server.sin_family = AF_INET;
	   	server.sin_port = htons(g_rtspClients[is].rtpport[0]);	// 这个是client那边的port端口号  
	   	server.sin_addr.s_addr = inet_addr(g_rtspClients[is].IP);	// 这个也是client那边的address地址
		int	bytes = 0;
		unsigned int timestamp_increse = 0;	// 用时钟频率（clock rate）计算而来表示时间的

		timestamp_increse = (unsigned int)(90000.0 / 25);	// timestamp_increse=时钟频率 / 帧率

		// RTP固定包头，为12字节；
		rtp_hdr = (RTP_FIXED_HEADER *)&sendbuf[0];

		rtp_hdr->payload = RTP_H264;   
		rtp_hdr->version = 2;          	// RTP的版本号
		rtp_hdr->marker = 0;           	// 标记当前位置是不是一帧数据的最后一包
		rtp_hdr->ssrc = htonl(10);		// 信源标记，并且在本RTP会话中全局唯一

		if(nAvFrmLen <= nalu_sent_len)
		{
			rtp_hdr->marker = 1;		// 最后一包数据中的marker是等于1的；
			rtp_hdr->seq_no = htons(g_rtspClients[is].seqnum++); // 序列号，每发送一个RTP包增1
			nalu_hdr = (NALU_HEADER*)&sendbuf[12]; 
			nalu_hdr->F = 0; 
			nalu_hdr->NRI = nIsIFrm; 
			nalu_hdr->TYPE = nNaluType;
			nalu_payload = &sendbuf[13];
			memcpy(nalu_payload, buffer, nAvFrmLen);
            g_rtspClients[is].tsvid = g_rtspClients[is].tsvid + timestamp_increse;            
			rtp_hdr->timestamp = htonl(g_rtspClients[is].tsvid);
			bytes = nAvFrmLen + 13;
			// 通过这个来进行数据发送的，从服务器发送到客户端；
			// sendbuf中包括纯视频流以及包头信息；
			sendto(udpfd, sendbuf, bytes, 0, (struct sockaddr *)&server, sizeof(server));
		}
		else if(nAvFrmLen > nalu_sent_len)
		{
			int k = 0, l = 0;
			k = nAvFrmLen / nalu_sent_len;		// 整包个数
			l = nAvFrmLen % nalu_sent_len;		// 余留包的大小
			int t = 0;        

            g_rtspClients[is].tsvid = g_rtspClients[is].tsvid + timestamp_increse;
			// 将tsvid转换成网络字节顺序；
            rtp_hdr->timestamp = htonl(g_rtspClients[is].tsvid); 
			while(t <= k)
			{
				rtp_hdr->seq_no = htons(g_rtspClients[is].seqnum++);
				if(t == 0)	// 整包里面的第一个包
				{
					rtp_hdr->marker = 0;
					fu_ind = (FU_INDICATOR*)&sendbuf[12];	// 因为rtp_hdr占用了0-11个位置
					fu_ind->F = 0; 
					fu_ind->NRI = nIsIFrm;
					fu_ind->TYPE = 28;		// FU-A，分片的单元；

					fu_hdr = (FU_HEADER*)&sendbuf[13];
					fu_hdr->E = 0;
					fu_hdr->R = 0;
					fu_hdr->S = 1;
					fu_hdr->TYPE = nNaluType;

					nalu_payload = &sendbuf[14];
					// buffer中存放的是H.264 Stream流数据；
					// 这样发送的一包数据中就包含了RTP报头和H.264流数据；
					memcpy(nalu_payload, buffer, nalu_sent_len);

					bytes = nalu_sent_len + 14;
					// 数据填充完了，调用sendto来进行发送数据；
					// 发送第一包数据；
					sendto(udpfd, sendbuf, bytes, 0, (struct sockaddr *)&server, sizeof(server));
					t++;
				}
				else if(k == t)	// 整包里面的最后一个整数包
				{
					rtp_hdr->marker = 1;		// =1表示这个是整帧的最后一个数据包了
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
				else if(t < k && t != 0)	// 这个是除了第一包和最后一包，剩下的那些整数包
				{
					rtp_hdr->marker = 0;	// 不是最后一包数据时marker等于0；
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
    if(flag)	// flag等于1才会进行发送，也就是进入if中
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

// 环状buffer的生产者
HI_S32 saveStream(VENC_STREAM_S *pstStream)
{
    HI_S32 i, j, lens = 0;

    for(j=0; j<MAX_RTSP_CLIENT; j++) //have atleast a connect
    {
		if(g_rtspClients[j].status == RTSP_SENDING)	// 这块是在playanswear后设置为发送模式的；
		{
		    for (i = 0; i < pstStream->u32PackCount; i++)
		    {
				// RTPbuf_s是用链表来实现的，所以使用前要申请内存，生成一个链表节点；
				RTPbuf_s *p = (RTPbuf_s *)malloc(sizeof(RTPbuf_s));
				INIT_LIST_HEAD(&(p->list));	// 对申请的链表节点进行初始化；

				// 对RTPbuf_s结构体中的数据进行填充；
				lens = pstStream->pstPack[i].u32Len-pstStream->pstPack[i].u32Offset;
				// 对用来真正存储数据的区域进行内存申请；
				p->buf = (char *)malloc(lens);
				p->len = lens;
				// 把有效数据复制到我们刚刚申请到的内存区域中；
				memcpy(p->buf, pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset, lens);

				// 最后将这个链表节点添加到已有链表的尾节点中；
				list_add_tail(&(p->list), &RTPbuf_head);
				count++;		// 用来记录当前环状buffer中几个有效数据；
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

	// RTSP实际上里面是通过RTP包来进行发送的；
	memset(&g_rtp_playload, 0, sizeof(g_rtp_playload));
	strcpy(&g_rtp_playload, "G726-32");	// "G726-32"是从live555中移植过来的
//	g_audio_rate = 8000;
	pthread_mutex_init(&g_sendmutex, NULL);
	pthread_mutex_init(&g_mutex, NULL);
	pthread_cond_init(&g_cond, NULL);
	memset(&g_rtspClients, 0, sizeof(RTSP_CLIENT) * MAX_RTSP_CLIENT);

	//pthread_create(&g_SendDataThreadId, NULL, SendDataThread, NULL);

//	struct sched_param thdsched;
//	thdsched.sched_priority = 2;	// 线程调度优先级
	//to listen visiting 创建一个服务器监听线程
	// 这个创建的线程，最终只是负责建立一个UDP通道；
	pthread_create(&threadId, NULL, RtspServerListen, NULL);
	//pthread_setschedparam(threadId,SCHED_RR,&thdsched);
	printf("RTSP:-----Init Rtsp server\n");

	// 创建一个RTP发送线程，线程函数vdRTPSendThread里面的if语句，当链表中不为空时，
	// 说明服务器那边有客户端连接了，服务器这边要发送数据了；如果链表中为空时，说明
	// 服务器那边还没有数据产生，所以不会发送数据，这个if语句内部也不会执行；
	pthread_create(&gs_RtpPid, 0, vdRTPSendThread, NULL);
	printf("after rtp send.\n");
	
	//exitok++;
}
void RtspServer_exit(void)
{
	return;
}

// 函数功能: 发送流媒体数据
HI_VOID* vdRTPSendThread(HI_VOID *p)
{
	while(1)
	{
		if(!list_empty(&RTPbuf_head))	// 判断环状buffer是否为空，不为空则执行if中的语句；
		{
			printf("before rtp send thread..................\n");
			// 从链表中找到第一个非空节点；
			RTPbuf_s *p = get_first_item(&RTPbuf_head, RTPbuf_s, list);
			// 调用VENC_Sent将第一个非空节点中的数据发送出去；
			VENC_Sent(p->buf, p->len);
			list_del(&(p->list));	// 发送完了后，要将这个链表节点从整个链表中去掉；
			free(p->buf);			// 将申请的内存都释放掉
			free(p);				// 将申请的内存都释放掉
			p = NULL;				// 指向NULL，防止野指针
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
	step 5: start stream venc H.264编码部分
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
	 H.264视频流传输与解码播放部分；
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
