
#include <base/system.h>

#include <game/generated/client_data.h>

#include <engine/shared/ringbuffer.h>
#include <engine/shared/config.h>

#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/textrender.h>

#include <game/client/components/menus.h>

#include "irc.h"
#include <string.h>

// same function as in the console
static float FlyInScaleFunc(float t)
{
	return sinf(acosf(1.0f-t));
}

void CIrcFrontend::OnInit()
{
	m_IrcChatState = IRCCHAT_CLOSED;
	m_Progress = 0.0f;
	m_FontSize = 16.0f;
	m_pHistoryEntry = 0x0;
	m_ChatlogActPage = 0;
	m_IrcCommands[0] = "/connect";
	m_IrcCommands[1] = "/topic";
	m_IrcCommands[2] = "/quit";
	m_IrcCommands[3] = "/names";
}

void CIrcFrontend::OnConsoleInit()
{
	Console()->Register("toggle_irc", "", CFGFLAG_CLIENT, ConToggleIrcFrontend, this, "Toggle the irc chat");
}

bool CIrcFrontend::OnInput(IInput::CEvent Event)
{
	if (m_IrcChatState == IRCCHAT_CLOSED || m_IrcChatState == IRCCHAT_CLOSING)
		return false;

	bool InputHandled = false;

	if (Event.m_Flags&IInput::FLAG_PRESS)
	{
		if(Event.m_Key == KEY_ESCAPE)
		{
			Toggle();
			InputHandled = true;
		}
		else if(Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER)
		{
			if(m_Input.GetString()[0])
			{
				//dbg_msg("IRC", "SEND:%s", m_Input.GetString());
				Client()->IRCSend(m_Input.GetString());

				char *pEntry = m_History.Allocate(m_Input.GetLength()+1);
				mem_copy(pEntry, m_Input.GetString(), m_Input.GetLength()+1);

				m_Input.Clear();
				m_pHistoryEntry = 0x0;
			}
			InputHandled = true;
		}
		if(Event.m_Flags&IInput::FLAG_PRESS && Event.m_Key == KEY_TAB)
		{
			//if the input begins with '/' it has to be a command
			if (m_Input.GetString()[0] == '/')
			{
				//copy input to buffer on first TAB
				if (!m_CommandCompletion)
				{
					str_copy(m_aCompletionBuffer, m_Input.GetString(), m_Input.GetLength()+1);
					m_CommandCompletion = true;
				}

				//put the next cmd to the m_Input if it fits the buffer
				for (int i = 0; i < IRC_MAX_CMDS; i++)
				{
					int TempCommandIndex = (m_CommandIndex + i) % IRC_MAX_CMDS;
					if (!str_comp_nocase_num(m_aCompletionBuffer, m_IrcCommands[TempCommandIndex], str_length(m_aCompletionBuffer)) && str_comp_nocase(m_IrcCommands[TempCommandIndex], m_Input.GetString()))
					{
						m_CommandIndex = TempCommandIndex;
						m_Input.Set(m_IrcCommands[m_CommandIndex]);
						break;
					}
				}
				return true;
			}

			//There can't be users if you are not connected
			if (!Client()->IRCIsConnecnted())
				return true;

			/*
			 * The following user-autocompletion and history is nearly the same as in the chat.cpp
			 */

			// fill the completion buffer
			if(m_CompletionChosen < 0)
			{
				const char *pCursor = m_Input.GetString()+m_Input.GetCursorOffset();
				for(int Count = 0; Count < m_Input.GetCursorOffset() && *(pCursor-1) != ' '; --pCursor, ++Count);
				m_PlaceholderOffset = pCursor-m_Input.GetString();

				for(m_PlaceholderLength = 0; *pCursor && *pCursor != ' '; ++pCursor)
					++m_PlaceholderLength;

				str_copy(m_aCompletionBuffer, m_Input.GetString()+m_PlaceholderOffset, min(static_cast<int>(sizeof(m_aCompletionBuffer)), m_PlaceholderLength+1));
			}

			// find next possible name
			const char *pCompletionString = 0;
			m_CompletionChosen = (m_CompletionChosen+1)%(2*IRC_MAX_USERS);
			for(int i = 0; i < 2*IRC_MAX_USERS; ++i)
			{
				int SearchType = ((m_CompletionChosen+i)%(2*IRC_MAX_USERS))/IRC_MAX_USERS;
				int Index = (m_CompletionChosen+i)%IRC_MAX_USERS;
				if(!m_pUsers[Index])
					continue;

				bool Found = false;
				if(SearchType == 1)
				{
					if(str_comp_nocase_num(m_pUsers[Index], m_aCompletionBuffer, str_length(m_aCompletionBuffer)) &&
						str_find_nocase(m_pUsers[Index], m_aCompletionBuffer))
						Found = true;
				}
				else if(!str_comp_nocase_num(m_pUsers[Index], m_aCompletionBuffer, str_length(m_aCompletionBuffer)))
					Found = true;

				if(Found)
				{
					pCompletionString = m_pUsers[Index];
					m_CompletionChosen = Index+SearchType*IRC_MAX_USERS;
					break;
				}
			}

			// insert the name
			if(pCompletionString)
			{
				char aBuf[256];
				// add part before the name
				str_copy(aBuf, m_Input.GetString(), min(static_cast<int>(sizeof(aBuf)), m_PlaceholderOffset+1));

				// add the name
				str_append(aBuf, pCompletionString, sizeof(aBuf));

				// add seperator
				const char *pSeparator = "";
				if(*(m_Input.GetString()+m_PlaceholderOffset+m_PlaceholderLength) != ' ')
					pSeparator = m_PlaceholderOffset == 0 ? ": " : " ";
				else if(m_PlaceholderOffset == 0)
					pSeparator = ":";
				if(*pSeparator)
					str_append(aBuf, pSeparator, sizeof(aBuf));

				// add part after the name
				str_append(aBuf, m_Input.GetString()+m_PlaceholderOffset+m_PlaceholderLength, sizeof(aBuf));

				m_PlaceholderLength = str_length(pSeparator)+str_length(pCompletionString);
				m_OldChatStringLength = m_Input.GetLength();
				m_Input.Set(aBuf);
				m_Input.SetCursorOffset(m_PlaceholderOffset+m_PlaceholderLength);
				InputHandled = true;
			}
		}
		else
		{
			// reset name completion process
			if(Event.m_Flags&IInput::FLAG_PRESS && Event.m_Key != KEY_TAB)
			{
				m_CompletionChosen = -1;
				m_CommandCompletion = false;
			}

			m_OldChatStringLength = m_Input.GetLength();
			m_Input.ProcessInput(Event);
			InputHandled = true;
		}
		if (Event.m_Key == KEY_UP)
		{
			if (m_pHistoryEntry)
			{
				char *pTest = m_History.Prev(m_pHistoryEntry);

				if (pTest)
					m_pHistoryEntry = pTest;
			}
			else
				m_pHistoryEntry = m_History.Last();

			if (m_pHistoryEntry)
				m_Input.Set(m_pHistoryEntry);
			InputHandled = true;
		}
		else if (Event.m_Key == KEY_DOWN)
		{
			if (m_pHistoryEntry)
				m_pHistoryEntry = m_History.Next(m_pHistoryEntry);

			if (m_pHistoryEntry)
				m_Input.Set(m_pHistoryEntry);
			else
				m_Input.Clear();
			InputHandled = true;
		}
		else if(Event.m_Key == KEY_PAGEUP)
		{
			++m_ChatlogActPage;
		}
		else if(Event.m_Key == KEY_PAGEDOWN)
		{
			--m_ChatlogActPage;
			if(m_ChatlogActPage < 0)
				m_ChatlogActPage = 0;
		}
	}

	if (!InputHandled)
		m_Input.ProcessInput(Event);

	return true;
}

void CIrcFrontend::OnRender()
{
	if (m_IrcChatState == IRCCHAT_CLOSED)
		return;

	CUIRect MainView, ChatView, UserView, LineView;
	CUIRect Screen = *UI()->Screen();
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);

	//Set colors like in the menu
	//from menus.cpp
	vec3 Rgb = HslToRgb(vec3(g_Config.m_UiColorHue/255.0f, g_Config.m_UiColorSat/255.0f, g_Config.m_UiColorLht/255.0f));
	vec4 ms_GuiColor = vec4(Rgb.r, Rgb.g, Rgb.b, g_Config.m_UiColorAlpha/255.0f);

	float ColorIngameScaleI = 0.5f;
	float ColorIngameAcaleA = 0.2f;
	ms_ColorTabbarInactiveIngame = vec4(
		ms_GuiColor.r*ColorIngameScaleI,
		ms_GuiColor.g*ColorIngameScaleI,
		ms_GuiColor.b*ColorIngameScaleI,
		ms_GuiColor.a*0.8f);

	ms_ColorTabbarActiveIngame = vec4(
		ms_GuiColor.r*ColorIngameAcaleA,
		ms_GuiColor.g*ColorIngameAcaleA,
		ms_GuiColor.b*ColorIngameAcaleA,
		ms_GuiColor.a);

	//Preparing the Ui components
	Screen.Margin(10.0f, &MainView);

	MainView.VSplitLeft((4.0f/5.0f)*Screen.w, &ChatView, &UserView);
	UserView.x += UI()->Scale()*10.0f;
	UserView.w -= UI()->Scale()*10.0f;

	ChatView.HSplitBottom(70.0f, &ChatView, &LineView);
	LineView.y += UI()->Scale()*10.0f;
	LineView.h -= UI()->Scale()*10.0f;

	//Fade in/fade out
	if (m_IrcChatState == IRCCHAT_OPENING  || m_IrcChatState == IRCCHAT_CLOSING)
	{
		ChatView.y =  ChatView.h*FlyInScaleFunc(m_Progress)-ChatView.h+10.0f;
		LineView.y = Screen.h-LineView.h*FlyInScaleFunc(m_Progress)-10.0f;
		UserView.x = Screen.w-UserView.w*FlyInScaleFunc(m_Progress)-10.0f;

		if (m_IrcChatState == IRCCHAT_OPENING)
			m_Progress += 0.05f;
		else
			m_Progress -= 0.05f;

		if (m_IrcChatState == IRCCHAT_OPENING && m_Progress >= 1.0f)
			m_IrcChatState = IRCCHAT_OPEN;
		else if (m_IrcChatState == IRCCHAT_CLOSING && m_Progress <= 0.0f)
			m_IrcChatState = IRCCHAT_CLOSED;
	}

	//Render!
	RenderChat(ChatView);
	RenderUser(UserView);
	RenderLine(LineView);
}

void CIrcFrontend::RenderChat(CUIRect ChatView)
{
	CUIRect Screen = *UI()->Screen();
	RenderTools()->DrawUIRect(&ChatView, ms_ColorTabbarActiveIngame, CUI::CORNER_ALL, 10.0f);

	float ChatOffsetX = ChatView.x+5.0f;
	float ChatOffsetY = ChatView.y+ChatView.h-5.0f;

	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, ChatOffsetX, ChatOffsetY, m_FontSize-4.0f, TEXTFLAG_RENDER);
	Cursor.m_LineWidth = ChatView.w-10.0f;
	Cursor.m_MaxLines = -1;

	/* same code as in console.cpp */
	//	render log (actual page, wrap lines)
	CChatlogEntry *pEntry = m_Chatlog.Last();
	float OffsetY = 0.0f;
	float LineOffset = 1.0f;
	for(int Page = 0; Page <= m_ChatlogActPage; ++Page, OffsetY = 0.0f)
	{
		while(pEntry)
		{
			// get y offset (calculate it if we haven't yet)
			if(pEntry->m_YOffset < 0.0f)
			{
				TextRender()->SetCursor(&Cursor, 0.0f, 0.0f, m_FontSize-4.0f, 0);
				Cursor.m_LineWidth = ChatView.w-10;
				TextRender()->TextEx(&Cursor, pEntry->m_aText, -1);
				pEntry->m_YOffset = Cursor.m_Y+Cursor.m_FontSize+LineOffset;
			}
			OffsetY += pEntry->m_YOffset;

			//	next page when lines reach the top
			if(ChatOffsetY-OffsetY <= ChatView.y+5.0f)
				break;

			//	just render output from actual backlog page (render bottom up)
			if(Page == m_ChatlogActPage)
			{
				TextRender()->SetCursor(&Cursor, ChatOffsetX, ChatOffsetY-OffsetY, m_FontSize-4.0f, TEXTFLAG_RENDER);
				Cursor.m_LineWidth = ChatView.w-10.0f;
				SetHighlightColor(pEntry->m_aText);
				TextRender()->TextEx(&Cursor, pEntry->m_aText, -1);
			}
			pEntry = m_Chatlog.Prev(pEntry);
		}

		//	actual backlog page number is too high, render last available page (current checked one, render top down)
		if(!pEntry && Page < m_ChatlogActPage)
		{
			m_ChatlogActPage = Page;
			pEntry = m_Chatlog.First();
			while(OffsetY > 0.0f && pEntry)
			{
				TextRender()->SetCursor(&Cursor, ChatOffsetX, ChatOffsetY-OffsetY, m_FontSize-4.0f, TEXTFLAG_RENDER);
				Cursor.m_LineWidth = ChatView.w-10.0f;
				SetHighlightColor(pEntry->m_aText);
				TextRender()->TextEx(&Cursor, pEntry->m_aText, -1);
				OffsetY -= pEntry->m_YOffset;
				pEntry = m_Chatlog.Next(pEntry);
			}
			break;
		}
	}
	TextRender()->TextColor(1,1,1,1); //reset text color
}

void CIrcFrontend::RenderUser(CUIRect UserView)
{
	CUIRect Screen = *UI()->Screen();
	RenderTools()->DrawUIRect(&UserView, ms_ColorTabbarActiveIngame, CUI::CORNER_ALL, 5.0f);

	if(!Client()->IRCIsConnecnted())
	{
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor,UserView.x+5.0f,UserView.y+UserView.h/2.0f-13.0f,m_FontSize,TEXTFLAG_RENDER);
		TextRender()->TextEx(&Cursor, "Not Connected", -1);
		return;
	}

	static int s_UserList = 0;
	static int s_SelectedUser = 0;
	static sorted_array<char const *> s_Users; //TODO: XXLTomate: Sort the userlist
	static float s_ScrollValue = 0;

	if (Client()->IRCGetUsers(&m_pUsers))
		s_Users.clear();

	if(s_Users.size() == 0 && m_pUsers[0] != NULL)
	{
		for(int i = 0; i < IRC_MAX_USERS; i++)
		{
			if (m_pUsers[i] == NULL)
				break;

			s_Users.add(m_pUsers[i]);
			if (str_comp(m_pUsers[i], Client()->IRCGetNickName()) == 0)
				s_SelectedUser = i;
		}
	}

	m_pClient->m_pMenus->UiDoListboxStart(&s_UserList, &UserView, 24.0f, "Userlist", "", s_Users.size(), 1,	s_SelectedUser,	s_ScrollValue);

	for(sorted_array<char const *>::range r = s_Users.all(); !r.empty(); r.pop_front())
	{
		CMenus::CListboxItem Item = m_pClient->m_pMenus->UiDoListboxNextItem(&r.front());

		if(Item.m_Visible)
			UI()->DoLabelScaled(&Item.m_Rect, r.front(), m_FontSize, -1);
	}

	s_SelectedUser = m_pClient->m_pMenus->UiDoListboxEnd(&s_ScrollValue, 0);
}

void CIrcFrontend::RenderLine(CUIRect LineView)
{
	CUIRect Screen = *UI()->Screen();
	RenderTools()->DrawUIRect(&LineView, ms_ColorTabbarActiveIngame, CUI::CORNER_ALL, 10.0f);

	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, LineView.x+5.0f, LineView.y+LineView.h/2.0f-13.0f, m_FontSize, TEXTFLAG_RENDER);
	Cursor.m_LineWidth = LineView.w;
	Cursor.m_MaxLines = 1;

	char aInputString[245];
	str_copy(aInputString, m_Input.GetString(), sizeof(aInputString));
	TextRender()->TextEx(&Cursor, aInputString, m_Input.GetCursorOffset());
	static float MarkerOffset = TextRender()->TextWidth(0, m_FontSize, "|", -1)/3;
	CTextCursor Marker = Cursor;
	TextRender()->TextEx(&Marker, "|", -1);
	TextRender()->TextEx(&Cursor, aInputString+m_Input.GetCursorOffset(), -1);
}

void CIrcFrontend::Toggle()
{
	if (m_IrcChatState == IRCCHAT_CLOSED || m_IrcChatState == IRCCHAT_CLOSING)
	{
		m_IrcChatState = IRCCHAT_OPENING;
		if (m_Progress <= 0.0f)
			m_Progress = 0.0f;
	}
	else
	{
		m_IrcChatState = IRCCHAT_CLOSING;
		if (m_Progress >= 1.0f)
			m_Progress = 1.0f;
	}
}

void CIrcFrontend::PrintLine(const char *pLine)
{
	int Len = str_length(pLine);

	if (Len > 255)
		Len = 255;

	CChatlogEntry *pEntry = m_Chatlog.Allocate(sizeof(CChatlogEntry)+Len);
	pEntry->m_YOffset = -1.0f;
	mem_copy(pEntry->m_aText, pLine, Len);
	pEntry->m_aText[Len] = 0;
}

void CIrcFrontend::SetHighlightColor(char* pText)
{
	//IRC highlighting TODO: XXLTomate: play a sound (but not in render ;-) )
	char aBuf[255];
	str_copy(aBuf, pText, 255);
	char * SelfNick = strtok(aBuf, ":");

	if(str_find_nocase(pText, Client()->IRCGetNickName()) && str_comp(SelfNick, Client()->IRCGetNickName()))
		TextRender()->TextColor(1.0f, 0.3f, 0.3f, 1.0f);
	else
		TextRender()->TextColor(1,1,1,1);
}

void CIrcFrontend::ConToggleIrcFrontend(IConsole::IResult *pResult, void *pUserData)
{
	((CIrcFrontend *) pUserData)->Toggle();
}
