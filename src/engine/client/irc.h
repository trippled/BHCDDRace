/* IRC handle script by XXLTomate */
#ifndef GAME_CLIENT_IRC_H
#define GAME_CLIENT_IRC_H

#define IRC_BUFFER_SIZE	512
#define IRC_MAX_ARGS	256
#define IRC_MAX_USERS	128 //TODO: XXLTomate: there is not a limit for users in a channel

class IRC
{
public:

	struct CIRCData
	{
		char m_Server[32];
		short m_Port;
		char m_Channel[32];
		char m_ChannelKey[32];
		char m_Nick[32];
		char m_RealName[32];
	} m_IRCData;

	bool m_Connected;
	bool m_NewUserList;
	int m_NewMessages;
	char* pUsers[IRC_MAX_USERS];

	void MainParser(char *pOut);
	void Init(char *pOut);
	void Leave();
	void Send(const char * pMsg);
	void Names();
	void Quit();
	void Topic();

private:
	int Connect(const char *remote_host_p, unsigned short remote_port);
	int SendLine(char *format_p);
	int RecvLine(char *line_p, unsigned int line_size);
	void OutFormat(char* pOut, char* pArg[IRC_MAX_ARGS], int argumentCount, int offset, char* prefix, bool skipFirstChar = true);
	void SetUsers(char* pArg[IRC_MAX_ARGS], int argumentCount, int offset);

	int	m_Sock;//the socket handle
	int m_ConnectAttempts;
	int m_ArgumentCount;
	char m_Buffer[512]; //the raw lines from IRC
	char m_From[56];
	char m_Nick[56];
	char *m_Sender;
	char *pArgument[IRC_MAX_ARGS]; //splited buffer by " "

	char m_ParseBuffer[512];
	char m_UserBuffer[512];
};
#endif
