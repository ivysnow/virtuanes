//
// AVI File Writer
//

#include "aviwriter.h"
#include "macro.h"

#pragma	comment(lib, "vfw32.lib")

CAVIWriter::CAVIWriter() : 
m_pAviFile(NULL),
m_pVideoStream(NULL),
m_pAudioStream(NULL),
m_pVideoStreamCompress(NULL),
m_pVideoFormat(NULL),
m_FrameNumber(0),
m_SampleNumber(0)
{
	::AVIFileInit();

	ZEROMEMORY( &m_VideoStreamInfo, sizeof(m_VideoStreamInfo) );
	m_VideoStreamInfo.fccType    = streamtypeVIDEO;
	m_VideoStreamInfo.fccHandler = comptypeDIB;
	m_VideoStreamInfo.dwQuality  = (DWORD)-1;

	// default formats
	SetAudioFormat( 44100, 16, 1 );
	SetFrameRate( 100, 60*100 );
	
	ZEROMEMORY( &m_VideoOption, sizeof(m_VideoOption) );
	m_VideoOption.fccType    = streamtypeVIDEO;
	m_VideoOption.fccHandler = comptypeDIB;

	ZEROMEMORY( &m_Compvers, sizeof(m_Compvers) );
	m_Compvers.cbSize    = sizeof(COMPVARS);
	m_Compvers.dwFlags    = ICMF_COMPVARS_VALID;
	m_Compvers.fccHandler = comptypeDIB;
	m_Compvers.lQ         = ICQUALITY_DEFAULT;
	m_Compvers.lKey       = 15;
}

CAVIWriter::~CAVIWriter()
{
	Close();
	::AVIFileExit();
}

bool	CAVIWriter::Open( const char* fname )
{
	if( !m_pVideoFormat )
		return	false;

	::DeleteFile( fname );

	if( ::AVIFileOpen( &m_pAviFile, fname, OF_WRITE | OF_CREATE, NULL ) ) {
		return	false;
	}

	// Video Stream
	if( ::AVIFileCreateStream( m_pAviFile, &m_pVideoStream, &m_VideoStreamInfo ) ) {
		Close();
		return	false;
	}
	if( ::AVIMakeCompressedStream( &m_pVideoStreamCompress, m_pVideoStream, &m_VideoOption, NULL ) != AVIERR_OK ) {
		Close();
		return	false;
	}
	if( ::AVIStreamSetFormat( m_pVideoStreamCompress, 0, m_pVideoFormat, m_pVideoFormat->biSize + m_pVideoFormat->biClrUsed * sizeof(RGBQUAD) ) ) {
		Close();
		return	false;
	}

	// Audio Stream
	if( ::AVIFileCreateStream( m_pAviFile, &m_pAudioStream, &m_AudioStreamInfo ) ) {
		Close();
		return	false;
	}
	if( ::AVIStreamSetFormat( m_pAudioStream, 0, &m_AudioFormat, sizeof(WAVEFORMATEX) ) ) {
		Close();
		return	false;
	}

	m_FrameNumber = 0;
	m_SampleNumber = 0;

	return	true;
}

void	CAVIWriter::Close()
{
	if( m_pAudioStream ) {
		::AVIStreamClose( m_pAudioStream );
		m_pAudioStream = NULL;
	}
	if( m_pVideoStreamCompress ) {
		::AVIStreamClose( m_pVideoStreamCompress );
		m_pVideoStreamCompress = NULL;
	}
	if( m_pVideoStream ) {
		::AVIStreamClose( m_pVideoStream );
		m_pVideoStream = NULL;
	}
	if( m_pAviFile ) {
		::AVIFileClose( m_pAviFile );
		m_pAviFile = NULL;
	}
}


bool	CAVIWriter::SelectVideoFormat( HWND hWnd )
{
	m_Compvers.dwFlags = ICMF_COMPVARS_VALID;
	m_Compvers.lpState = NULL;
	m_Compvers.cbState = 0;

	if( !::ICCompressorChoose( hWnd, ICMF_CHOOSE_DATARATE | ICMF_CHOOSE_KEYFRAME,
				m_pVideoFormat, NULL, &m_Compvers, NULL) )
		return	false;

	m_VideoStreamInfo.fccHandler = m_Compvers.fccHandler;

	ZEROMEMORY( &m_VideoOption, sizeof(m_VideoOption) );
	m_VideoOption.fccType          = streamtypeVIDEO;
	m_VideoOption.fccHandler       = m_Compvers.fccHandler;
	m_VideoOption.dwKeyFrameEvery  = m_Compvers.lKey;
	m_VideoOption.dwQuality        = m_Compvers.lQ;
	m_VideoOption.dwBytesPerSecond = m_Compvers.lDataRate;
	m_VideoOption.dwFlags          = ((m_Compvers.lDataRate > 0) ? AVICOMPRESSF_DATARATE : 0)
					|((m_Compvers.lKey > 0 ) ? AVICOMPRESSF_KEYFRAMES : 0);

	::ICCompressorFree( &m_Compvers );

	return	true;
}

void	CAVIWriter::SetVideoFormat( BITMAPINFOHEADER* bih )
{
	m_pVideoFormat = bih;
}

void	CAVIWriter::SetAudioFormat( const DWORD rate, const WORD bits, const INT channels )
{
	m_AudioFormat.wFormatTag      = WAVE_FORMAT_PCM;
	m_AudioFormat.nSamplesPerSec  = rate;
	m_AudioFormat.wBitsPerSample  = bits;
	m_AudioFormat.nChannels       = channels;
	m_AudioFormat.nBlockAlign     = m_AudioFormat.nChannels * m_AudioFormat.wBitsPerSample / 8;
	m_AudioFormat.nAvgBytesPerSec = m_AudioFormat.nSamplesPerSec * m_AudioFormat.nBlockAlign;
	m_AudioFormat.cbSize          = 0;

	ZEROMEMORY( &m_AudioStreamInfo, sizeof(m_AudioStreamInfo) );
	m_AudioStreamInfo.fccType         = streamtypeAUDIO;
//	m_AudioStreamInfo.fccHandler      = comptypeDIB;
	m_AudioStreamInfo.fccHandler      = m_AudioFormat.wFormatTag;
	m_AudioStreamInfo.dwScale         = m_AudioFormat.nBlockAlign;
	m_AudioStreamInfo.dwInitialFrames = 1;
	m_AudioStreamInfo.dwRate          = m_AudioFormat.nAvgBytesPerSec;
	m_AudioStreamInfo.dwQuality       = (DWORD)-1;
	m_AudioStreamInfo.dwSampleSize    = m_AudioFormat.nBlockAlign;
}

void	CAVIWriter::SetFrameRate( const DWORD scale, const DWORD rate )
{
	m_FrameRate = rate;

//	ZEROMEMORY( &m_VideoStreamInfo, sizeof(m_VideoStreamInfo) );
//	m_VideoStreamInfo.fccType    = streamtypeVIDEO;
//	m_VideoStreamInfo.fccHandler = comptypeDIB;
	m_VideoStreamInfo.dwScale    = scale;
	m_VideoStreamInfo.dwRate     = rate;
//	m_VideoStreamInfo.dwQuality  = (DWORD)-1;
}

bool	CAVIWriter::WriteVideo( const char* video )
{
	if( !m_pAviFile )
		return	false;

#if	0
	DWORD	dwFlags = 0;
	if( m_VideoOption.dwFlags & AVICOMPRESSF_KEYFRAMES ) {
		if( (m_FrameNumber % m_VideoOption.dwKeyFrameEvery) == 0 ) {
			dwFlags = AVIIF_KEYFRAME;
		}
	}
	if( ::AVIStreamWrite( m_pVideoStreamCompress, m_FrameNumber, 1, (void*)video,
				m_pVideoFormat->biSizeImage, dwFlags, NULL, NULL ) ) {
		Close();
		return	false;
	}
#else
	if( ::AVIStreamWrite( m_pVideoStreamCompress, m_FrameNumber, 1, (void*)video,
				m_pVideoFormat->biSizeImage, AVIIF_KEYFRAME, NULL, NULL ) ) {
		Close();
		return	false;
	}
#endif
	m_FrameNumber++;

	return	true;
}

bool	CAVIWriter::WriteAudio( const char* audio, const int length )
{
	if( !m_pAviFile )
		return	false;

	int	samples = length / m_AudioFormat.nBlockAlign;
	if( ::AVIStreamWrite( m_pAudioStream, m_SampleNumber, samples, (void*)audio,
				length, 0, NULL, NULL ) ) {
		Close();
		return	false;
	}

	m_SampleNumber += samples;

	return	true;
}

bool	CAVIWriter::WriteFrame( const char* video, const char* audio, const int length )
{
	if( !WriteVideo( video ) )
		return	false;
	if( !WriteAudio( audio, length ) )
		return	false;

	return	true;
}

