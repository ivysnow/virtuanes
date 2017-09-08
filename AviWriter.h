//
// AVI File Writer
//
#ifndef	__AVIFILEWRITER_INCLUDED__
#define	__AVIFILEWRITER_INCLUDED__

#define	WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vfw.h>

class	CAVIWriter
{
public:
	CAVIWriter();
	virtual	~CAVIWriter();

	bool	Open( const char* fname );
	void	Close();

	bool	IsAviRecord() { return m_pAviFile?true:false; }

	bool	SelectVideoFormat( HWND hWnd );

	void	SetVideoFormat( BITMAPINFOHEADER* bih );
	void	SetAudioFormat( const DWORD rate, const WORD bits, const INT channels );
	void	SetFrameRate( const DWORD scale, const DWORD rate );

	bool	WriteVideo( const char* video );
	bool	WriteAudio( const char* audio, const int length );
	bool	WriteFrame( const char* video, const char* audio, const int length );

protected:
private:
	LONG		m_FrameNumber;
	LONG		m_SampleNumber;

	DWORD		m_FrameRate;
	PAVIFILE	m_pAviFile;
	PAVISTREAM	m_pVideoStream;
	PAVISTREAM	m_pVideoStreamCompress;
	PAVISTREAM	m_pAudioStream;

	AVISTREAMINFO	m_VideoStreamInfo;
	AVISTREAMINFO	m_AudioStreamInfo;

	AVICOMPRESSOPTIONS m_VideoOption;

	COMPVARS	m_Compvers;

	BITMAPINFOHEADER* m_pVideoFormat;
	WAVEFORMATEX	m_AudioFormat;

};

#endif	// !__AVIFILEWRITER_INCLUDED__
