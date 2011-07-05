/* IRC handle script by XXLTomate */
#include "irc.h"

#ifdef WIN32
#include <windows.h>
#include <winsock.h>
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <stdarg.h>

#include <base/system.h>
#include <engine/shared/config.h>

int IRC::Connect(const char *remote_host_p, unsigned short remote_port)
{
	struct sockaddr_in sin;    /* a structure which tells our socket where it's connecting to */
	struct hostent *hostent_p; /* a structure which will store results from the DNS query we do
								  for remote_host_p */

	/* perform a DNS query to find the IP address of remote_host_p */
	if (!(hostent_p = gethostbyname(remote_host_p)))
		return 0;

	mem_zero(&sin, sizeof(sin));
	sin.sin_family = PF_INET; /* IPv4 */
	sin.sin_addr.s_addr = *(long *) hostent_p->h_addr; /* take the IP address returned */
	sin.sin_port = htons(remote_port); /* convert remote_port to a network order byte */

	if (g_Config.m_IRCDebug)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), " - Resolved %s to %s\r\n", remote_host_p, inet_ntoa(sin.sin_addr));
		dbg_msg("IRC", aBuf);
	}

	//connect with socket
	if (connect(m_Sock, (struct sockaddr *) &sin, sizeof(sin)) == -1)
		return 0;

	return 1;
}

int IRC::SendLine(char *format_p)
{
	char buffer[IRC_BUFFER_SIZE] = {0};

	str_copy(buffer, format_p, sizeof(buffer));
	strncat(buffer, "\r\n", (sizeof(buffer) - strlen(buffer)));
	return (send(m_Sock, buffer, strlen(buffer), 0));
}

int IRC::RecvLine(char *line_p, unsigned int line_size)
{
	char byte = 0;
	/* recv one byte at a time from the socket
	untill you reach a newline (\n) character */

	while (byte != '\n' && strlen(line_p) < line_size)
	{
		if (!recv(m_Sock, (char *) &byte, 1, 0))
			return 0;

		if (byte != '\r' && byte != '\n' && byte != '\0')
		{
			strncat(line_p, (char *) &byte, 1);
		}
	}

	if (g_Config.m_IRCDebug)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "<< %s", line_p);
		dbg_msg("IRC", aBuf);
	}

	return 1;
}

void IRC::Send(const char * pMsg)
{
	char aBuf[IRC_BUFFER_SIZE];
	str_format(aBuf, sizeof(aBuf), "PRIVMSG %s :%s", m_IRCData.m_Channel, pMsg);
	SendLine(aBuf);
}

void IRC::Names()
{
	char aBuf[IRC_BUFFER_SIZE];
	str_format(aBuf, sizeof(aBuf), "NAMES %s", m_IRCData.m_Channel);
	SendLine(aBuf);
}

void IRC::Quit()
{
	SendLine("QUIT");
}

void IRC::Topic()
{
	char aBuf[IRC_BUFFER_SIZE];
	str_format(aBuf, sizeof(aBuf), "TOPIC %s", m_IRCData.m_Channel);
	SendLine(aBuf);
}

void IRC::OutFormat(char* pOut, char* pArg[IRC_MAX_ARGS], int argumentCount, int offset, char* prefix, bool skipFirstChar)
{			char aBuf[IRC_BUFFER_SIZE];
			char bBuf[IRC_BUFFER_SIZE];

			if (skipFirstChar)
				str_format(aBuf, sizeof(aBuf), "%s%s", prefix, pArg[offset]+1);
			else
				str_format(aBuf, sizeof(aBuf), "%s%s", prefix, pArg[offset]);

			//pointer ping pong
			for(int i = 1; i< argumentCount-offset; i++)
				if (i % 2 == 0)
					str_format(aBuf, sizeof(aBuf), "%s %s", bBuf, pArg[offset+i]);
				else
					str_format(bBuf, sizeof(bBuf), "%s %s", aBuf, pArg[offset+i]);

			if ((argumentCount-offset) % 2 == 0)
				str_copy(pOut, bBuf, sizeof(bBuf));
			else
				str_copy(pOut, aBuf, sizeof(aBuf));
			return;
}

void IRC::MainParser(char *pOut)
{
	char aBuf[IRC_BUFFER_SIZE];
	str_copy(pOut, "", 1);
	mem_zero(m_Buffer, sizeof(m_Buffer));

	if (RecvLine(m_Buffer, sizeof(m_Buffer)) == 0)
		return;

	pToken = strtok(m_Buffer, " ");
	m_ArgumentCount = 0;

	while (pToken != NULL)
	{
		pArgument[m_ArgumentCount] = pToken;
		pToken = strtok(NULL, " ");
		m_ArgumentCount++;
	}

	if (m_ArgumentCount == 2)
	{
		if (str_comp(pArgument[0], "PING") == 0)
		{
			str_format(aBuf, sizeof(aBuf), "PONG %s", pArgument[1]);
			SendLine(aBuf);
			return;
		}
	}

	if (m_ArgumentCount > 2)
	{
		if (str_comp(pArgument[1], "001") == 0) //Welcome msg
		{
			str_format(aBuf, sizeof(aBuf), "JOIN %s :%s", m_IRCData.m_Channel, m_IRCData.m_ChannelKey);
			SendLine(aBuf);

			str_format(aBuf, sizeof(aBuf), "*** You joined %s", m_IRCData.m_Channel);
			str_copy(pOut, aBuf, sizeof(aBuf));
			return;
		}
		else if (str_comp(pArgument[1], "433") == 0) //Nick already in use
		{
			strncat(m_Nick, "_", 1);
			str_format(aBuf, sizeof(aBuf), "NICK %s", m_Nick);
			SendLine(aBuf);
			str_copy(m_IRCData.m_Nick, m_Nick, sizeof(m_IRCData.m_Nick));
			return;
		}
		else if (str_comp(pArgument[1], "353") == 0) //Users at channel
		{
			str_format(aBuf, sizeof(aBuf), "*** Users at %s: ", m_IRCData.m_Channel);
			OutFormat(pOut, pArgument, m_ArgumentCount, 5, aBuf);
			return;
		}
		else if (str_comp(pArgument[1], "332") == 0) //Topic
		{
			str_format(aBuf, sizeof(aBuf), "*** Topic at %s: ", pArgument[3]);
			OutFormat(pOut, pArgument, m_ArgumentCount, 4, aBuf);
		}
		else if (g_Config.m_IRCMotd && str_comp(pArgument[1], "372") == 0) //Motd
		{
			str_format(aBuf, sizeof(aBuf), "*** ");
			OutFormat(pOut, pArgument, m_ArgumentCount, 3, aBuf);
			return;
		}
	}

	if (m_ArgumentCount == 3)
	{
		if (str_comp(pArgument[1], "NICK") == 0)
		{
			m_Sender = strtok(pArgument[0], "!")+1;
			str_format(aBuf, sizeof(aBuf), "*** %s is now known as %s", m_Sender, pArgument[2]+1);
			str_copy(pOut, aBuf, sizeof(aBuf));
			return;
		}
		else if (str_comp(pArgument[1], "PART") == 0)
		{
			m_Sender = strtok(pArgument[0], "!")+1;
			str_format(aBuf, sizeof(aBuf), "*** %s has left %s", m_Sender, pArgument[2]);
			str_copy(pOut, aBuf, sizeof(aBuf));
			return;
		}
		else if (str_comp(pArgument[1], "JOIN") == 0)
		{
			m_Sender = strtok(pArgument[0], "!")+1;
			str_format(aBuf, sizeof(aBuf), "*** %s has joined %s", m_Sender, pArgument[2]+1);
			str_copy(pOut, aBuf, sizeof(aBuf));
			return;
		}
	}

	if(m_ArgumentCount >= 3)
	{
		if (str_comp(pArgument[1], "QUIT") == 0)
		{
			m_Sender = strtok(pArgument[0], "!")+1;
			str_format(aBuf, sizeof(aBuf), "*** %s has quit: ", m_Sender);
			OutFormat(pOut, pArgument, m_ArgumentCount, 2, aBuf);
			return;
		}
	}

	if(m_ArgumentCount >= 4)
	{
		if (str_comp(pArgument[1], "PRIVMSG") == 0)
		{
			m_Sender = strtok(pArgument[0], "!")+1;
			str_format(aBuf, sizeof(aBuf), "%s: ", m_Sender);
			OutFormat(pOut, pArgument, m_ArgumentCount, 3, aBuf);
			return;
		}
		else if (str_comp(pArgument[1], "MODE") == 0)
		{
			m_Sender = strtok(pArgument[0], "!")+1;
			if (str_comp(pArgument[2], m_IRCData.m_Channel) == 0)
				str_format(aBuf, sizeof(aBuf), "*** Mode of %s/%s was set to %s by %s", pArgument[2], m_IRCData.m_Nick, pArgument[3] ,m_Sender);
			else
				str_format(aBuf, sizeof(aBuf), "*** Mode of %s/%s was set to %s by %s", m_IRCData.m_Channel, pArgument[2], pArgument[3] ,m_Sender);
			str_copy(pOut, aBuf, sizeof(aBuf));
			return;
		}
		else if (str_comp(pArgument[1], "432") == 0) //Erroneus Nickname
		{
			str_format(aBuf, sizeof(aBuf), "*** %s: ", pArgument[3]);
			OutFormat(pOut, pArgument, m_ArgumentCount, 4, aBuf);
			return;
		}
		else if (str_comp(pArgument[1], "474") == 0) //mode is +b/can't join channel
		{
			str_format(aBuf, sizeof(aBuf), "*** You can't join to %s (You are banned) ", pArgument[3]);
			str_copy(pOut, aBuf, sizeof(aBuf));
			return;
		}
		else if (str_comp(pArgument[1], "KICK") == 0)
		{
			m_Connected = false;
			m_Sender = strtok(pArgument[0], "!")+1;
			str_format(aBuf, sizeof(aBuf), "*** You got kicked out of %s by %s: ", pArgument[2], m_Sender);
			OutFormat(pOut, pArgument, m_ArgumentCount, 4, aBuf);
			return;
		}
	}

	return;
}

void IRC::Init(char *pOut)
{
	char aBuf[128];
	str_copy(pOut, "", 1);

#if defined(CONF_FAMILY_WINDOWS)
	//setup winsockets
	WSADATA wsa_data;

	if (WSAStartup(MAKEWORD(2, 2), &wsa_data))
	{
		str_copy(pOut, "*** Fatal Error: failed to initialize winsock", sizeof(IRC_BUFFER_SIZE));
		return;
	}
#endif

	/* create a socket using the INET protocol family (IPv4), and make it a streaming TCP socket */
	if ((m_Sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		str_copy(pOut, "*** Fatal Error: socket() failed.", sizeof(IRC_BUFFER_SIZE));
		return;
	}

	if (g_Config.m_IRCDebug)
		dbg_msg("IRC", "*** Socket created");

	if (!Connect(m_IRCData.m_Server, m_IRCData.m_Port))
	{
		str_format(aBuf, sizeof(aBuf), "*** Error: Failed to connect to %s:%i", m_IRCData.m_Server, m_IRCData.m_Port);
		str_copy(pOut, aBuf, sizeof(aBuf));
		return;
	}

	if (g_Config.m_IRCDebug)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "*** Connected to %s:%i", m_IRCData.m_Server, m_IRCData.m_Port);
		dbg_msg("IRC", aBuf);
	}

	str_format(m_Nick, sizeof(m_Nick), "%s", m_IRCData.m_Nick);

	str_format(aBuf, sizeof(aBuf), "USER %s 127.0.0.1 localhost :%s", m_IRCData.m_Nick, m_IRCData.m_RealName);
	SendLine(aBuf);

	str_format(aBuf, sizeof(aBuf), "NICK %s", m_Nick);
	SendLine(aBuf);

	return;
}

void IRC::Leave()
{
#if defined(CONF_FAMILY_WINDOWS)
	//clean up winsockets
	WSACleanup();
#endif
}

