//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
// Health.cpp
//
// implementation of CHudHealth class
//
#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "view.h"

#include "iclientmode.h"

#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>

#include <vgui/ILocalize.h>

using namespace vgui;

#include "hudelement.h"
#include "hud_numericdisplay.h"

#include "convar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define INIT_HEALTH -1

//-----------------------------------------------------------------------------
// Purpose: Health panel
//-----------------------------------------------------------------------------
class CHudHealth : public CHudElement, public CHudNumericDisplay
{
	DECLARE_CLASS_SIMPLE( CHudHealth, CHudNumericDisplay );

public:
	CHudHealth( const char *pElementName );
	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );
	virtual void OnThink();
			void MsgFunc_Damage( bf_read &msg );

private:
	// old variables
	int		m_iHealth;
	
	int		m_bitsDamage;
};	

DECLARE_HUDELEMENT( CHudHealth );
DECLARE_HUD_MESSAGE( CHudHealth, Damage );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudHealth::CHudHealth( const char *pElementName ) : CHudElement( pElementName ), CHudNumericDisplay(NULL, "HudHealth")
{
	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHealth::Init()
{
	HOOK_HUD_MESSAGE( CHudHealth, Damage );
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHealth::Reset()
{
	m_iHealth		= INIT_HEALTH;
	m_bitsDamage	= 0;

	wchar_t *tempString = g_pVGuiLocalize->Find("#Valve_Hud_HEALTH");

	if (tempString)
	{
		SetLabelText(tempString);
	}
	else
	{
		SetLabelText(L"HEALTH");
	}
	SetDisplayValue(m_iHealth);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHealth::VidInit()
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHealth::OnThink()
{
	int newHealth = 0;
	C_BasePlayer *local = C_BasePlayer::GetLocalPlayer();
	if ( local )
	{
		// Never below zero
		newHealth = MAX( local->GetHealth(), 0 );
	}

	// Only update the fade if we've changed health
	if ( newHealth == m_iHealth )
	{
		return;
	}

	m_iHealth = newHealth;

	if ( m_iHealth >= 20 )
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("HealthIncreasedAbove20");
	}
	else if ( m_iHealth > 0 )
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("HealthIncreasedBelow20");
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("HealthLow");
	}

	SetDisplayValue(m_iHealth);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHealth::MsgFunc_Damage( bf_read &msg )
{

	int armor = msg.ReadByte();	// armor
	int damageTaken = msg.ReadByte();	// health
	long bitsDamage = msg.ReadLong(); // damage bits
	bitsDamage; // variable still sent but not used

	Vector vecFrom;

	vecFrom.x = msg.ReadBitCoord();
	vecFrom.y = msg.ReadBitCoord();
	vecFrom.z = msg.ReadBitCoord();

	// Actually took damage?
	if ( damageTaken > 0 || armor > 0 )
	{
		if ( damageTaken > 0 )
		{
			// start the animation
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("HealthDamageTaken");
		}
	}
}

//=============================================================================
// Project 1: target ID, like the one in Counter-Strike: Source.
//
// The server looks at what the player is aiming at (CP1TargetInfo in
// ai_basenpc.cpp) and sends the name, a detail line (health, level) and a colour
// code about four times a second. This element only draws them, centred under
// the crosshair. It lives in this file so no new file has to be added.
//=============================================================================

// Where the text starts, as a fraction of the screen height (0 = top, 1 = bottom)
static ConVar p1_target_y( "p1_target_y", "0.55", FCVAR_ARCHIVE, "Vertical position of the target ID text as a fraction of the screen height" );

class CHudP1Target : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudP1Target, vgui::Panel );

public:
	CHudP1Target( const char *pElementName );

	virtual void	Init( void );
	virtual void	VidInit( void );
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	Paint( void );

	// Called when the server sends a "P1Target" message
	void			MsgFunc_P1Target( bf_read &msg );

private:
	// Draws one line of text with a black drop shadow
	void			DrawLine( const wchar_t *pwszText, int x, int y, Color color );

	vgui::HFont		m_hFont;
	char			m_szName[64];		// line 1, as sent by the server
	char			m_szDetail[64];		// line 2, as sent by the server
	int				m_iRelation;		// 0 = neutral, 1 = friendly, 2 = hostile
	float			m_flLastUpdate;		// gpGlobals->curtime of the last message, 0 = nothing to show
};

DECLARE_HUDELEMENT( CHudP1Target );
DECLARE_HUD_MESSAGE( CHudP1Target, P1Target );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudP1Target::CHudP1Target( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "P1TargetID" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	// The HUD always has this font. ApplySchemeSettings() replaces it with the scheme's.
	m_hFont = g_hFontTrebuchet24;

	m_szName[0] = 0;
	m_szDetail[0] = 0;
	m_iRelation = 0;
	m_flLastUpdate = 0;

	// Hidden while the player is dead. The panel covers the whole screen, the text is
	// placed by hand in Paint().
	SetHiddenBits( HIDEHUD_PLAYERDEAD );
	SetSize( ScreenWidth(), ScreenHeight() );
}

//-----------------------------------------------------------------------------
// Purpose: Listen for the server's messages
//-----------------------------------------------------------------------------
void CHudP1Target::Init( void )
{
	HOOK_HUD_MESSAGE( CHudP1Target, P1Target );
}

//-----------------------------------------------------------------------------
// Purpose: A new map: forget the last target
//-----------------------------------------------------------------------------
void CHudP1Target::VidInit( void )
{
	CHudElement::VidInit();
	m_flLastUpdate = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Pick the font. Define "P1TargetID" in ClientScheme.res to change the
//			text size, otherwise it falls back to "TargetID" and then to "Default".
//-----------------------------------------------------------------------------
void CHudP1Target::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_hFont = pScheme->GetFont( "P1TargetID", IsProportional() );
	if ( m_hFont == vgui::INVALID_FONT )
	{
		m_hFont = pScheme->GetFont( "TargetID", IsProportional() );
	}
	if ( m_hFont == vgui::INVALID_FONT )
	{
		m_hFont = pScheme->GetFont( "Default", IsProportional() );
	}

	SetPaintBackgroundEnabled( false );
	SetSize( ScreenWidth(), ScreenHeight() );
}

//-----------------------------------------------------------------------------
// Purpose: Store what the server sent and restart the display timer
//-----------------------------------------------------------------------------
void CHudP1Target::MsgFunc_P1Target( bf_read &msg )
{
	msg.ReadString( m_szName, sizeof( m_szName ) );
	msg.ReadString( m_szDetail, sizeof( m_szDetail ) );
	m_iRelation = msg.ReadByte();
	m_flLastUpdate = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: One line of text, drawn twice: first in black one pixel (or more on a big
//			screen) to the lower right, then in its own colour on top.
//-----------------------------------------------------------------------------
void CHudP1Target::DrawLine( const wchar_t *pwszText, int x, int y, Color color )
{
	int iShadow = MAX( 1, ScreenHeight() / 480 );

	vgui::surface()->DrawSetTextFont( m_hFont );

	vgui::surface()->DrawSetTextColor( 0, 0, 0, color.a() );
	vgui::surface()->DrawSetTextPos( x + iShadow, y + iShadow );
	vgui::surface()->DrawPrintText( pwszText, wcslen( pwszText ) );

	vgui::surface()->DrawSetTextColor( color.r(), color.g(), color.b(), color.a() );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawPrintText( pwszText, wcslen( pwszText ) );
}

//-----------------------------------------------------------------------------
// Purpose: Draw the name and the detail line, centred under the crosshair
//-----------------------------------------------------------------------------
void CHudP1Target::Paint( void )
{
	// The server repeats its message every 0.25 s while the player aims at something.
	// Keep showing the text for 0.6 s after the last one (so it doesn't flicker),
	// and fade it out during the last 0.25 s of that.
	const float flShowTime = 0.6f;
	const float flFadeTime = 0.25f;

	float flAge = gpGlobals->curtime - m_flLastUpdate;
	if ( m_flLastUpdate <= 0 || flAge > flShowTime || m_szName[0] == 0 )
		return;

	int iAlpha = 255;
	if ( flAge > flShowTime - flFadeTime )
	{
		iAlpha = clamp( (int)( 255.0f * ( flShowTime - flAge ) / flFadeTime ), 0, 255 );
	}

	// The name is coloured by how the target feels about the player, like team colours
	Color colName( 235, 235, 235, iAlpha );			// neutral
	if ( m_iRelation == 1 )
	{
		colName = Color( 130, 190, 255, iAlpha );	// friendly
	}
	else if ( m_iRelation == 2 )
	{
		colName = Color( 255, 110, 90, iAlpha );	// hostile
	}

	wchar_t wszName[64];
	wchar_t wszDetail[64];
	g_pVGuiLocalize->ConvertANSIToUnicode( m_szName, wszName, sizeof( wszName ) );
	g_pVGuiLocalize->ConvertANSIToUnicode( m_szDetail, wszDetail, sizeof( wszDetail ) );

	int iYPos = (int)( ScreenHeight() * p1_target_y.GetFloat() );

	// Line 1: the name
	int iWide, iTall;
	vgui::surface()->GetTextSize( m_hFont, wszName, iWide, iTall );
	DrawLine( wszName, ( ScreenWidth() - iWide ) / 2, iYPos, colName );

	// Line 2: health and level, one text line further down
	if ( m_szDetail[0] )
	{
		int iDetailWide, iDetailTall;
		vgui::surface()->GetTextSize( m_hFont, wszDetail, iDetailWide, iDetailTall );
		DrawLine( wszDetail, ( ScreenWidth() - iDetailWide ) / 2, iYPos + iTall, Color( 255, 255, 255, iAlpha ) );
	}
}
