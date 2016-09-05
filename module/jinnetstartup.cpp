
#include "jinnetstartup.h"
#include "jinlogger.h"

JinNetStartup::JinNetStartup()
    : weStart_(false)
{
#ifdef JWIN
    WSADATA winsockInfo;
    if ( WSAStartup( MAKEWORD( 2, 2 ), &winsockInfo ) != 0 )
    {
#if   defined(JDEBUG)
        DWORD dwIOError = GetLastError();
        LPVOID messageBuffer;
        FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, dwIOError, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),  // Default language
            ( LPTSTR ) & messageBuffer, 0, NULL );
        // something has gone wrong here...
        JDLog( "WSAStartup failed:Error code - %d\n%s", dwIOError, messageBuffer );
        //Free the buffer.
        LocalFree( messageBuffer );
#endif
    }
    else
    {
        weStart_ = true;
    }
    JDLog("WSAStartup result=%d",(int)weStart_);
#else
    weStart_ = true;
#endif
}

JinNetStartup::~JinNetStartup()
{
#ifdef JWIN
    if(weStart_)
    {
        WSACleanup();
        JDLog("WSACleanup");
    }
#endif
}
