/* Stub of the GameSpy GP SDK (not redistributable, online service is dead). Declarations only; implementations are in um_gamespy_stub.cpp. */
#pragma once
typedef int GPEnum; typedef int GPResult; typedef int GPProfile; typedef int GPErrorCode; typedef void* GPConnection;
#define GP_NICK_LEN 31
#define GP_EMAIL_LEN 51
#define GP_PASSWORD_LEN 31
#define GP_UNIQUENICK_LEN 21
#define GP_FIRSTNAME_LEN 31
#define GP_LASTNAME_LEN 51
#define GP_STATUS_STRING_LEN 256
#define GP_LOCATION_STRING_LEN 256
#define GP_REASON_LEN 1025
#define GP_COUNTRYCODE_LEN 3
#define GP_NO_ERROR 0
#define GP_ERROR 1
typedef struct { int dummy; } GPIPE;
typedef struct { GPErrorCode errorCode; int fatal; char* errorString; GPResult result; } GPErrorArg;
typedef struct { GPResult result; GPProfile profile; } GPConnectResponseArg;
typedef struct { GPProfile profile; char reason[GP_REASON_LEN]; } GPRecvBuddyRequestArg;
typedef struct { GPProfile profile; GPEnum status; char statusString[GP_STATUS_STRING_LEN]; char locationString[GP_LOCATION_STRING_LEN]; } GPRecvBuddyStatusArg;
typedef struct { GPProfile profile; unsigned date; char* message; } GPRecvBuddyMessageArg;
typedef void (*GPCallback)(GPConnection*, void*, void*);
typedef GPCallback GPErrorCallback; typedef GPCallback GPConnectCallback; typedef GPCallback GPRecvBuddyRequestCallback;
typedef GPCallback GPRecvBuddyStatusCallback; typedef GPCallback GPRecvBuddyMessageCallback;
enum { GP_OFFLINE=0, GP_ONLINE=1, GP_PLAYING=2, GP_STAGING=3, GP_CHATTING=4, GP_AWAY=5 };
