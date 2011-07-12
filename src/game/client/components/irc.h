#ifndef GAME_CLIENT_COMPONENTS_IRC_H
#define GAME_CLIENT_COMPONENTS_IRC_H

#include <game/client/component.h>
#include <game/client/lineinput.h>

class CIrcFrontend : public CComponent
{
	CLineInput m_Input;

	enum
	{
		IRCCHAT_CLOSED,
		IRCCHAT_OPENING,
		IRCCHAT_OPEN,
		IRCCHAT_CLOSING
	};

	vec4 ms_ColorTabbarInactiveIngame;
	vec4 ms_ColorTabbarActiveIngame;

	bool m_InputLineUpdate;
	int m_IrcChatState;
	int m_ChatlogActPage;
	float m_Progress;
	float m_FontSize;

	struct CChatlogEntry
	{
		float m_YOffset;
		char m_aText[1];
	};
	TStaticRingBuffer<CChatlogEntry, 64*1024, CRingBufferBase::FLAG_RECYCLE> m_Chatlog;
	TStaticRingBuffer<char, 64*1024, CRingBufferBase::FLAG_RECYCLE> m_History;
	char *m_pHistoryEntry;

	void RenderChat(CUIRect ChatView);
	void RenderUser(CUIRect UserView);
	void RenderLine(CUIRect LineView);
	void SetHighlightColor(char *pText);
	void Toggle();
	static void ConToggleIrcFrontend(IConsole::IResult *pResult, void *pUserData);

public:
	virtual void OnInit();
	virtual void OnRender();
	virtual void OnConsoleInit();
	virtual bool OnInput(IInput::CEvent Event);

	void PrintLine(const char *pLine);
};


#endif
