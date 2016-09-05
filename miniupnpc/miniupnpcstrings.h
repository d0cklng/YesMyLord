#ifndef MINIUPNPCSTRINGS_H_INCLUDED
#define MINIUPNPCSTRINGS_H_INCLUDED

//#define OS_STRING "Windows"
#define MINIUPNPC_VERSION_STRING "1.9"

#ifdef JWIN
#define OS_STRING "Windows"
#endif
#ifdef JLINUX
#define OS_STRING "Linux"
#endif
#ifdef JMAC
#define OS_STRING "Mac"
#endif

#if 0
/* according to "UPnP Device Architecture 1.0" */
#define UPNP_VERSION_STRING "UPnP/1.0"
#else
/* according to "UPnP Device Architecture 1.1" */
#define UPNP_VERSION_STRING "UPnP/1.1"
#endif

#endif
