//
// AVI変換ダイアログクラス
//
//
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <string>
using namespace std;

#include "VirtuaNESres.h"
#include "DebugOut.h"
#include "App.h"
#include "Config.h"
#include "Pathlib.h"

#include "Wnd.h"
#include "AviConvDlg.h"

#include "nes.h"
#include "mmu.h"
#include "ppu.h"
#include "apu.h"
#include "rom.h"

#include "DirectDraw.h"

CHAR	CAviConvDlg::g_szErrorMessage[256];

DLG_MESSAGE_BEGIN(CAviConvDlg)
DLG_ON_MESSAGE( WM_INITDIALOG,	OnInitDialog )
DLG_ON_MESSAGE( WM_TIMER,	OnTimer )
DLG_ON_MESSAGE( WM_APP+100,	OnConvertDone )
DLG_ON_MESSAGE( WM_APP+101,	OnConvertError )

DLG_COMMAND_BEGIN()
DLG_ON_COMMAND( IDCANCEL, OnCancel )

DLG_ON_COMMAND( IDC_MCV_ROMBROWSE, OnRomFileBrowse )
DLG_ON_COMMAND( IDC_MCV_MOVIEBROWSE, OnMovieFileBrowse )
DLG_ON_COMMAND( IDC_MCV_AVIBROWSE, OnAviFileBrowse )

DLG_ON_COMMAND( IDC_MCV_VIDEOSELECT, OnVideoCompressSelect )
DLG_ON_COMMAND( IDC_MCV_CONVERT, OnConvertStart )
DLG_COMMAND_END()
DLG_MESSAGE_END()

CAviConvDlg::CAviConvDlg() :
m_lpRender(NULL),
m_lpLinecolor(NULL),
m_lpScreen(NULL),
m_lpSound(NULL),
m_pNes(NULL),
m_hThread(NULL)
{
	if( !(m_lpRender = (LPBYTE)::malloc( CDirectDraw::RENDER_WIDTH*CDirectDraw::RENDER_HEIGHT )) ) {
		throw	CApp::GetErrorString( IDS_ERROR_OUTOFMEMORY );
	}
	if( !(m_lpScreen = (LPBYTE)::malloc( CDirectDraw::SCREEN_WIDTH*CDirectDraw::SCREEN_HEIGHT*3+16 )) ) {
		throw	CApp::GetErrorString( IDS_ERROR_OUTOFMEMORY );
	}
	if( !(m_lpLinecolor = (LPBYTE)::malloc( CDirectDraw::RENDER_HEIGHT )) ) {
		throw	CApp::GetErrorString( IDS_ERROR_OUTOFMEMORY );
	}
	if( !(m_lpSound = (LPBYTE)::malloc( 8192 )) ) {
		throw	CApp::GetErrorString( IDS_ERROR_OUTOFMEMORY );
	}
}

CAviConvDlg::~CAviConvDlg()
{
	FREE( m_lpSound );
	FREE( m_lpLinecolor );
	FREE( m_lpScreen );
	FREE( m_lpRender );
}

INT	CAviConvDlg::DoModal( HWND hWndParent )
{
	return	::DialogBoxParam( CApp::GetPlugin(), MAKEINTRESOURCE(IDD_MOVIECONV),
				hWndParent, g_DlgProc, (LPARAM)this );
}

DLGMSG	CAviConvDlg::OnInitDialog( DLGMSGPARAM )
{
//	DEBUGOUT( "CAviConvDlg::OnInitDialog\n" );

	m_szRomFile[0] = '\0';
	m_szMovieFile[0] = '\0';
	m_szAviFile[0] = '\0';

	::SendDlgItemMessage( m_hWnd, IDC_MCV_FRAMERATE_COMBO, CB_RESETCONTENT, 0, 0 );
	::SendDlgItemMessage( m_hWnd, IDC_MCV_FRAMERATE_COMBO, CB_INSERTSTRING, (WPARAM)0, (LPARAM)"Full FPS" );
	::SendDlgItemMessage( m_hWnd, IDC_MCV_FRAMERATE_COMBO, CB_INSERTSTRING, (WPARAM)1, (LPARAM)"1/2 FPS" );
	::SendDlgItemMessage( m_hWnd, IDC_MCV_FRAMERATE_COMBO, CB_INSERTSTRING, (WPARAM)2, (LPARAM)"1/3 FPS" );
	::SendDlgItemMessage( m_hWnd, IDC_MCV_FRAMERATE_COMBO, CB_INSERTSTRING, (WPARAM)3, (LPARAM)"1/4 FPS" );
	::SendDlgItemMessage( m_hWnd, IDC_MCV_FRAMERATE_COMBO, CB_INSERTSTRING, (WPARAM)4, (LPARAM)"1/5 FPS" );
	::SendDlgItemMessage( m_hWnd, IDC_MCV_FRAMERATE_COMBO, CB_INSERTSTRING, (WPARAM)5, (LPARAM)"1/6 FPS" );
	::SendDlgItemMessage( m_hWnd, IDC_MCV_FRAMERATE_COMBO, CB_SETCURSEL, 0, 0 );

	INT	i, select = 0;
	CHAR	szMode[64];
	::SendDlgItemMessage( m_hWnd, IDC_MCV_SAMPLERATE_COMBO, CB_RESETCONTENT, 0, 0 );
//	for( i = 0; i < 8; i++ ) {
	for( i = 0; CConfig::SamplingRateTable[i*2+0]; i++ ) {
		::wsprintf( szMode, "%d.%03dKHz %2dbits",
				CConfig::SamplingRateTable[i*2+0]/1000,
				CConfig::SamplingRateTable[i*2+0]%1000,
				CConfig::SamplingRateTable[i*2+1] );
		::SendDlgItemMessage( m_hWnd, IDC_MCV_SAMPLERATE_COMBO, CB_INSERTSTRING, (WPARAM)i, (LPARAM)szMode );

		if( Config.sound.nRate == CConfig::SamplingRateTable[i*2+0]
		 && Config.sound.nBits == CConfig::SamplingRateTable[i*2+1] ) {
			select = i;
		}
	}
	::SendDlgItemMessage( m_hWnd, IDC_MCV_FILTERTYPE_COMBO, CB_RESETCONTENT, 0, 0 );
	for( i = IDS_SND_FILTER_NONE; i <= IDS_SND_FILTER_TYPE4; i++ ) {
		CApp::LoadString( i, szMode, sizeof(szMode) );
		::SendDlgItemMessage( m_hWnd, IDC_MCV_FILTERTYPE_COMBO, CB_INSERTSTRING, (WPARAM)i-IDS_SND_FILTER_NONE, (LPARAM)szMode );
	}

	::SendDlgItemMessage( m_hWnd, IDC_MCV_SAMPLERATE_COMBO, CB_SETCURSEL, (WPARAM)select, 0 );
	::SendDlgItemMessage( m_hWnd, IDC_MCV_FILTERTYPE_COMBO, CB_SETCURSEL, (WPARAM)Config.sound.nFilterType, 0 );

	CTRLENABLE( IDC_MCV_CONVERT, FALSE );

	ZEROMEMORY( &m_BitmapHdr, sizeof(m_BitmapHdr) );

	m_BitmapHdr.biSize        = sizeof(BITMAPINFOHEADER);
	m_BitmapHdr.biWidth       = 256;
	m_BitmapHdr.biHeight      = 240;
	m_BitmapHdr.biPlanes      = 1;
	m_BitmapHdr.biBitCount    = 24;
	m_BitmapHdr.biCompression = BI_RGB;
	m_BitmapHdr.biSizeImage   = 256*240*3;

	::GetWindowText( m_hWnd, m_szTitle, sizeof(m_szTitle)-1 );

	m_AviWriter.SetVideoFormat( &m_BitmapHdr );

	m_hThread = NULL;

	return	TRUE;
}

DLGCMD	CAviConvDlg::OnCancel( DLGCMDPARAM )
{
//	DEBUGOUT( "CAviConvDlg::OnCancel\n" );

	::KillTimer( m_hWnd, 0x1234 );

	if( m_hThread ) {
		m_bThreadExit = TRUE;
		::WaitForSingleObject( m_hThread, INFINITE );
		CLOSEHANDLE( m_hThread );
		m_bThreadExit = FALSE;

		if( m_pNes ) {
			m_pNes->MovieStop();
			DELETEPTR( m_pNes );
		}
		m_AviWriter.Close();

		::SetWindowText( m_hWnd, m_szTitle );

		ControlEnable( TRUE );
	} else {
		::EndDialog( m_hWnd, IDCANCEL );
	}
}

DLGCMD	CAviConvDlg::OnRomFileBrowse( DLGCMDPARAM )
{
	string	pathstr;
	if( Config.path.bRomPath ) {
		pathstr = CPathlib::CreatePath( CApp::GetModulePath(), Config.path.szRomPath );
	} else {
		pathstr = CApp::GetModulePath();
	}

	::GetDlgItemText( m_hWnd, IDC_MCV_ROMFILE, m_szRomFile, sizeof(m_szRomFile)-1 );

	CHAR	szTitle[256];
	CApp::LoadString( IDS_UI_OPENROM, szTitle, sizeof(szTitle) );

	OPENFILENAME	ofn = { sizeof(OPENFILENAME) };
	ofn.hwndOwner       = m_hWnd;
	ofn.lpstrFile       = m_szRomFile;
	ofn.nMaxFile        = sizeof(m_szRomFile);
	ofn.lpstrFilter     = "All Support Types\0*.nes;*.fds;*.lzh;*.zip;*.rar;*.cab\0"
			      "NES ROM (*.nes)\0*.nes\0Disk Image (*.fds)\0*.fds\0"
			      "Archive File\0*.lzh;*.zip;*.rar;*.cab\0";
	ofn.nFilterIndex    = 1;
	ofn.Flags           = OFN_READONLY|OFN_HIDEREADONLY|OFN_EXPLORER|OFN_PATHMUSTEXIST;
	ofn.lpstrInitialDir = pathstr.c_str();
	ofn.lpstrTitle = szTitle;

	if( ::GetOpenFileName( &ofn ) ) {
		::SetDlgItemText( m_hWnd, IDC_MCV_ROMFILE, m_szRomFile );
	}
}

DLGCMD	CAviConvDlg::OnMovieFileBrowse( DLGCMDPARAM )
{
	string	pathstr;
	if( Config.path.bMoviePath ) {
		pathstr = CPathlib::CreatePath( CApp::GetModulePath(), Config.path.szMoviePath );
		::CreateDirectory( pathstr.c_str(), NULL );
	} else {
		pathstr = CApp::GetModulePath();
	}

	::GetDlgItemText( m_hWnd, IDC_MCV_MOVIEFILE, m_szMovieFile, sizeof(m_szMovieFile)-1 );

	OPENFILENAME	ofn = { sizeof(OPENFILENAME) };
	ofn.hwndOwner       = m_hWnd;
	ofn.lpstrFile       = m_szMovieFile;
	ofn.lpstrDefExt     = "vmv";
	ofn.nMaxFile        = sizeof(m_szMovieFile);
	ofn.lpstrFilter     = "Movie Files(*.vmv)\0*.vmv\0All Files(*.*)\0*.*\0";
	ofn.nFilterIndex    = 1;
	ofn.Flags           = OFN_EXPLORER|OFN_PATHMUSTEXIST;
	ofn.lpstrInitialDir = pathstr.c_str();

	CHAR	szTitle[256];
	CApp::LoadString( IDS_UI_PLAYMOVIE, szTitle, sizeof(szTitle) );
	ofn.lpstrTitle = szTitle;

	if( ::GetOpenFileName( &ofn ) ) {
		::SetDlgItemText( m_hWnd, IDC_MCV_MOVIEFILE, m_szMovieFile );
	}
}

DLGCMD	CAviConvDlg::OnAviFileBrowse( DLGCMDPARAM )
{
	string	pathstr, tempstr;
	if( Config.path.bMoviePath ) {
		pathstr = CPathlib::CreatePath( CApp::GetModulePath(), Config.path.szMoviePath );
		::CreateDirectory( pathstr.c_str(), NULL );
	} else {
		pathstr = CApp::GetModulePath();
	}
	::GetDlgItemText( m_hWnd, IDC_MCV_AVIFILE, m_szAviFile, sizeof(m_szAviFile)-1 );

	OPENFILENAME	ofn = { sizeof(OPENFILENAME) };
	ofn.hwndOwner       = m_hWnd;
	ofn.lpstrFile       = m_szAviFile;
	ofn.lpstrDefExt     = "avi";
	ofn.nMaxFile        = sizeof(m_szAviFile);
	ofn.lpstrFilter     = "Movie Files(*.avi)\0*.avi\0All Files(*.*)\0*.*\0";
	ofn.nFilterIndex    = 1;
	ofn.Flags           = OFN_EXPLORER|OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT;
	ofn.lpstrInitialDir = pathstr.c_str();

//	CHAR	szTitle[256];
//	CApp::LoadString( IDS_UI_AVICONV, szTitle, sizeof(szTitle) );
//	ofn.lpstrTitle = szTitle;

	if( ::GetSaveFileName( &ofn ) ) {
		::SetDlgItemText( m_hWnd, IDC_MCV_AVIFILE, m_szAviFile );
	}
}

DLGCMD	CAviConvDlg::OnVideoCompressSelect( DLGCMDPARAM )
{
//	DEBUGOUT( "CAviConvDlg::OnCompressSelect\n" );

	if( !m_AviWriter.SelectVideoFormat( m_hWnd ) ) {
		return;
	}

	CTRLENABLE( IDC_MCV_CONVERT, TRUE );
}

DLGMSG	CAviConvDlg::OnConvertDone( DLGMSGPARAM )
{
//	DEBUGOUT( "CAviConvDlg::OnConvertDone\n" );

	::KillTimer( m_hWnd, 0x1234 );

	::WaitForSingleObject( m_hThread, INFINITE );
	CLOSEHANDLE( m_hThread );

	::SetWindowText( m_hWnd, m_szTitle );

	m_AviWriter.Close();
	DELETEPTR( m_pNes );
	ControlEnable( TRUE );

	CHAR	szTemp[256];
	DWORD	totaltime = (m_FinishTime - m_StartTime) / 1000;
	INT	h, m, s;
	h = totaltime / 3600;
	totaltime %= 3600;
	m = totaltime / 60;
	s = totaltime % 60;

	::wsprintf( szTemp, "Convert Finished. Time:%02d:%02d:%02d", h, m, s );

	::MessageBox( m_hWnd, szTemp, "VirtuaNES", MB_OK );

	return	TRUE;
}

DLGMSG	CAviConvDlg::OnConvertError( DLGMSGPARAM )
{
//	DEBUGOUT( "CAviConvDlg::OnConvertError\n" );

	::KillTimer( m_hWnd, 0x1234 );

	::WaitForSingleObject( m_hThread, INFINITE );
	CLOSEHANDLE( m_hThread );

	::SetWindowText( m_hWnd, m_szTitle );

	m_AviWriter.Close();
	DELETEPTR( m_pNes );
	ControlEnable( TRUE );

	::MessageBox( m_hWnd, (LPSTR)lParam, "VirtuaNES", MB_OK );

	return	TRUE;
}

DLGCMD	CAviConvDlg::OnConvertStart( DLGCMDPARAM )
{
//	DEBUGOUT( "CAviConvDlg::OnConvertStart\n" );

	::GetDlgItemText( m_hWnd, IDC_MCV_ROMFILE, m_szRomFile, sizeof(m_szRomFile)-1 );
	::GetDlgItemText( m_hWnd, IDC_MCV_MOVIEFILE, m_szMovieFile, sizeof(m_szMovieFile)-1 );
	::GetDlgItemText( m_hWnd, IDC_MCV_AVIFILE, m_szAviFile, sizeof(m_szAviFile)-1 );

	INT select;

	select = ::SendDlgItemMessage( m_hWnd, IDC_MCV_FRAMERATE_COMBO, CB_GETCURSEL, 0, 0 );

#if	1
	m_FrameDivider = select + 1;
#else
	switch( select ) {
		default:
		case	0: m_FrameDivider = 1; break;
		case	1: m_FrameDivider = 2; break;
		case	2: m_FrameDivider = 3; break;
		case	3: m_FrameDivider = 4; break;
		case	4: m_FrameDivider = 5; break;
		case	5: m_FrameDivider = 6; break;
	}
#endif

	select = ::SendDlgItemMessage( m_hWnd, IDC_MCV_SAMPLERATE_COMBO, CB_GETCURSEL, 0, 0 );
	Config.sound.nRate = CConfig::SamplingRateTable[select*2+0];
	Config.sound.nBits = CConfig::SamplingRateTable[select*2+1];

	Config.sound.nFilterType = ::SendDlgItemMessage( m_hWnd, IDC_MCV_FILTERTYPE_COMBO, CB_GETCURSEL, 0, 0 );

	m_AviWriter.SetAudioFormat( Config.sound.nRate, Config.sound.nBits, 1 );

	m_hThread = NULL;

	try {
		m_pNes = new NES(m_szRomFile);
		m_pNes->ppu->SetScreenPtr( m_lpRender, m_lpLinecolor );
		m_pNes->Reset();

		if( !m_pNes->MoviePlay( m_szMovieFile ) )
			throw	"MoviePlay error.";

		WORD	wRecVersion, wVersion;
		DWORD	dwRecordTimes;
		m_pNes->GetMovieInfo( wRecVersion, wVersion, m_RecordFrames, dwRecordTimes );

		m_AviWriter.SetFrameRate( 10000, m_pNes->nescfg->FrameRate * 10000 / m_FrameDivider );

		if( !m_AviWriter.Open( m_szAviFile ) ) {
			CHAR	szTemp[256];
			::wsprintf( szTemp, CApp::GetErrorString(IDS_ERROR_OPEN), m_szAviFile );
			throw	szTemp;
		}

		if( !(m_hThread = ::CreateThread( NULL, 0, ThreadProc, this, CREATE_SUSPENDED, &m_dwThreadID )) ) {
			throw	"CreateThread failed.";
		}

		ControlEnable( FALSE );
		DirectDraw.GetPaletteData2( m_Palette );

		m_FrameNo = -1;

		// Time Remain
		::SetTimer( m_hWnd, 0x1234, 500, NULL );

		// start converter
		m_bThreadExit = FALSE;
//		m_StartTime = m_FinishTime = ::timeGetTime();
		::ResumeThread( m_hThread );

	} catch( CHAR* str ) {
		if( m_hThread ) {
			::TerminateThread( m_hThread, 0 );
			CLOSEHANDLE( m_hThread );
		}

		::KillTimer( m_hWnd, 0x1234 );

		m_AviWriter.Close();
		DELETEPTR( m_pNes );

		::MessageBox( m_hWnd, str, "VirtuaNES", MB_OK );
#ifndef	_DEBUG
	} catch(...) {
		if( m_hThread ) {
			::TerminateThread( m_hThread, 0 );
			CLOSEHANDLE( m_hThread );
		}

		::KillTimer( m_hWnd, 0x1234 );

		m_AviWriter.Close();
		DELETEPTR( m_pNes );

		::MessageBox( m_hWnd, CApp::GetErrorString( IDS_ERROR_UNKNOWN ), "VirtuaNES", MB_OK );
#endif
	}
}

DLGMSG	CAviConvDlg::OnTimer( DLGMSGPARAM )
{
	CHAR	szTemp[256];

	INT	nowframe = m_FrameNo;
	DWORD	elapsed = ::timeGetTime() - m_StartTime;

	if( m_FrameNo >= 0 && elapsed > 5000 ) {
		DWORD	elapsedave = elapsed / nowframe;
		DWORD	estimation = elapsedave * (m_RecordFrames-nowframe) / 1000;
		INT	h, m, s;
		h = estimation / 3600;
		estimation %= 3600;
		m = estimation / 60;
		s = estimation % 60;

		::wsprintf( szTemp, "%s [%d/%d] Time remain %02d:%02d:%02d", m_szTitle, m_FrameNo, m_RecordFrames, h, m, s );
	} else {
		::wsprintf( szTemp, "%s [%d/%d] Time remain --:--:--", m_szTitle, m_FrameNo, m_RecordFrames );
	}

	::SetWindowText( m_hWnd, szTemp );

	return	FALSE;
}

DWORD	WINAPI	CAviConvDlg::ThreadProc( LPVOID lpVoid )
{
CAviConvDlg*	pAviConvDlg = (CAviConvDlg*)lpVoid;
NES*		pNes = pAviConvDlg->m_pNes;
CAVIWriter&	AviWriter = pAviConvDlg->m_AviWriter;

LPBYTE	pRender = pAviConvDlg->m_lpRender;
LPBYTE	pScreen = pAviConvDlg->m_lpScreen;
LPBYTE	pLinecolor = pAviConvDlg->m_lpLinecolor;
RGBQUAD* pPalette = pAviConvDlg->m_Palette;
LPBYTE	pSound = pAviConvDlg->m_lpSound;

INT	FrameDivider = pAviConvDlg->m_FrameDivider;

BOOL	bFinish = FALSE;

double	Samples = 0.0;

	pAviConvDlg->m_FrameNo = 0;
	pAviConvDlg->m_StartTime = pAviConvDlg->m_FinishTime = ::timeGetTime();

	while( !pAviConvDlg->m_bThreadExit ) {
		if( !pNes->IsMoviePlay() ) {
			bFinish = TRUE;
			break;
		}

		try {
			pNes->Movie();

			if( (pAviConvDlg->m_FrameNo % FrameDivider) == 0 ) {
				pNes->EmulateFrame( TRUE );

				for( INT i = 0; i < CDirectDraw::SCREEN_HEIGHT; i++ ) {
					LPBYTE	pSrc = &pRender[CDirectDraw::RENDER_WIDTH*i+8];
					LPBYTE	pDst = &pScreen[CDirectDraw::SCREEN_WIDTH*3*(CDirectDraw::SCREEN_HEIGHT-i-1)];
					RGBQUAD* pPal = &pPalette[ (pLinecolor[i] & 7)*256+((pLinecolor[i] & 0x80)?0x40:0x00) ];
#if	0
					for( INT j = 0; j < CDirectDraw::SCREEN_WIDTH; j++ ) {
						BYTE	pixel = *pSrc++;
						pixel |= (pLinecolor[i] & 0x80)?0x40:0;
						*pDst++ = pPal[ pixel ].rgbBlue;
						*pDst++ = pPal[ pixel ].rgbGreen;
						*pDst++ = pPal[ pixel ].rgbRed;
					}
#else
					DWORD	width = CDirectDraw::SCREEN_WIDTH;

					__asm {
						mov		eax, pSrc
						mov		esi, pPal
						mov		edi, pDst
_render_loop:
						mov		edx, [eax]
						movzx		ebx, dl
						mov		ebx, [esi+ebx*4]
						shr		edx, 8
						mov		[edi+ 0], ebx
						movzx		ebx, dl
						mov		ebx, [esi+ebx*4]
						shr		edx, 8
						mov		[edi+ 3], ebx
						movzx		ebx, dl
						shr		edx, 8
						mov		ebx, [esi+ebx*4]
						mov		edx, [esi+edx*4]
						mov		[edi+ 6], ebx
						mov		[edi+ 9], dx
						shr		edx, 16
						mov		[edi+11], dl

						mov		edx, [eax+4]
						movzx		ebx, dl
						mov		ebx, [esi+ebx*4]
						shr		edx, 8
						mov		[edi+12], ebx
						movzx		ebx, dl
						mov		ebx, [esi+ebx*4]
						shr		edx, 8
						mov		[edi+15], ebx
						movzx		ebx, dl
						shr		edx, 8
						mov		ebx, [esi+ebx*4]
						mov		edx, [esi+edx*4]
						mov		[edi+18], ebx
						mov		[edi+21], dx
						shr		edx, 16
						mov		[edi+23], dl

						lea		eax, [eax+ 8]
						lea		edi, [edi+24]

						sub		width, 8
						jg		_render_loop
					}
#endif
				}

				if( !AviWriter.WriteVideo( (const char*)pScreen ) ) {
					throw	"Write error.(video)";
				}
			} else {
				pNes->EmulateFrame( FALSE );
			}

			INT	size;
			INT	blockalign = Config.sound.nBits / 8;

			Samples += ((double)Config.sound.nRate / (double)pNes->nescfg->FrameRate) * (double)blockalign;
			size = (INT)Samples - ((INT)Samples % blockalign);
			Samples -= size;

			pNes->apu->Process( pSound, size );
			if( !AviWriter.WriteAudio( (const char*)pSound, size ) ) {
				throw	"Write error.(audio)";
			}

			pAviConvDlg->m_FrameNo++;

		} catch( CHAR* str ) {
			pNes->MovieStop();
			AviWriter.Close();

			::strcpy( g_szErrorMessage, str );
			::PostMessage( pAviConvDlg->m_hWnd, WM_APP+101, 0, (LPARAM)g_szErrorMessage );

			pAviConvDlg->m_bThreadExit = TRUE;
#ifndef	_DEBUG
		} catch(...) {
			pNes->MovieStop();
			AviWriter.Close();

			::PostMessage( pAviConvDlg->m_hWnd, WM_APP+101, 0, (LPARAM)CApp::GetErrorString( IDS_ERROR_UNKNOWN ) );

			pAviConvDlg->m_bThreadExit = TRUE;
#endif
		}

		// For Windows9x
		::Sleep( 0 );
	}

	pNes->MovieStop();
	AviWriter.Close();

	if( bFinish ) {
		pAviConvDlg->m_FinishTime = ::timeGetTime();
		// Convert Done
		::PostMessage( pAviConvDlg->m_hWnd, WM_APP+100, 0, 0 );
	}

	return	0;
}

void	CAviConvDlg::ControlEnable( BOOL bEnable )
{
	CTRLENABLE( IDC_MCV_ROMBROWSE,		bEnable );
	CTRLENABLE( IDC_MCV_MOVIEBROWSE,	bEnable );
	CTRLENABLE( IDC_MCV_AVIBROWSE,		bEnable );
	CTRLENABLE( IDC_MCV_ROMFILE,		bEnable );
	CTRLENABLE( IDC_MCV_MOVIEFILE,		bEnable );
	CTRLENABLE( IDC_MCV_AVIFILE,		bEnable );

	CTRLENABLE( IDC_MCV_FRAMERATE_COMBO,	bEnable );
	CTRLENABLE( IDC_MCV_SAMPLERATE_COMBO,	bEnable );
	CTRLENABLE( IDC_MCV_FILTERTYPE_COMBO,	bEnable );
	CTRLENABLE( IDC_MCV_VIDEOSELECT,	bEnable );

	CTRLENABLE( IDC_MCV_CONVERT,		bEnable );
}

