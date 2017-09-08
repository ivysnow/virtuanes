//
// AVI変換ダイアログクラス
//
#ifndef	__CAVICONVDLG_INCLUDED__
#define	__CAVICONVDLG_INCLUDED__

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
using namespace std;

#include "Wnd.h"
#include "AviWriter.h"

class	NES;

class	CAviConvDlg : public CWnd
{
public:
	CAviConvDlg();
	virtual	~CAviConvDlg();

	// Override from CWnd
	INT	DoModal( HWND hWndParent );

protected:
	// Message map
	DLG_MESSAGE_MAP()
	DLGMSG		OnInitDialog( DLGMSGPARAM );
	DLGMSG		OnTimer( DLGMSGPARAM );
	DLGCMD		OnCancel( DLGCMDPARAM );
	DLGCMD		OnRomFileBrowse( DLGCMDPARAM );
	DLGCMD		OnMovieFileBrowse( DLGCMDPARAM );
	DLGCMD		OnAviFileBrowse( DLGCMDPARAM );
	DLGCMD		OnVideoCompressSelect( DLGCMDPARAM );
	DLGCMD		OnConvertStart( DLGCMDPARAM );

	DLGMSG		OnConvertDone( DLGMSGPARAM );
	DLGMSG		OnConvertError( DLGMSGPARAM );

	void	ControlEnable( BOOL bEnable );

	//
	CAVIWriter	m_AviWriter;
	NES*		m_pNes;

	INT	m_FrameDivider;

	INT	m_FrameNo;
	DWORD	m_StartTime;
	DWORD	m_FinishTime;
	DWORD	m_RecordFrames;

	BITMAPINFOHEADER m_BitmapHdr;
	RGBQUAD	m_Palette[256*8];

	LPBYTE	m_lpRender;
	LPBYTE	m_lpLinecolor;
	LPBYTE	m_lpScreen;
	LPBYTE	m_lpSound;

	CHAR	m_szTitle[256];
	CHAR	m_szRomFile[_MAX_PATH];
	CHAR	m_szMovieFile[_MAX_PATH];
	CHAR	m_szAviFile[_MAX_PATH];

	// Thread
	volatile BOOL	m_bThreadExit;

	HANDLE	m_hThread;
	DWORD	m_dwThreadID;

	static	CHAR	g_szErrorMessage[256];
	static	DWORD WINAPI	ThreadProc( LPVOID lpVoid );


private:
};

#endif	// !__CAVICONVDLG_INCLUDED__

