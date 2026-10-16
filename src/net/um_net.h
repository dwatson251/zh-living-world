// Living World transport: server-paced lockstep over TCP.
//   * The server runs the authoritative simulation (headless engine), owns the command order, and broadcasts per-frame command batches.
//   * Clients are replicas: they send commands to the server and execute exactly the batches the server publishes, never running ahead.
//   * A client that joins at any time gets the world description plus the command log and fast-forwards (replay), then follows live.
//   * Clients report world CRCs; the server compares them with its own (system of record) and flags divergence.
#pragma once
#include "PreRTS.h"
#include <string>
#include <vector>
#include <map>
#include "GameNetwork/NetworkInterface.h"

struct UmSlotDesc { int state; int tmpl; int color; int startPos; int team; std::string name; int profT; unsigned rgb; std::string nick; };   // profT: 0 = none, else the occupant's faction template index + 1
struct UmWorldDesc
{
	std::string map; int seed; int synthStarts; int speed; std::vector<UmSlotDesc> slots;   // synthStarts: extra start waypoints (stress worlds)
	UmWorldDesc() : seed(0), synthStarts(0), speed(30) {}
};

enum UmRole { UM_ROLE_NONE, UM_ROLE_SERVER, UM_ROLE_CLIENT };

// Build the skirmish game info from a description and post MSG_NEW_GAME (same path for server and replicas).
void UmStartWorld(const UmWorldDesc &w);

class UmNet;
extern UmNet *TheUmNet;

// Creates the network object and installs it as TheNetwork. host/port are used by clients; world by the server.
void UmNetStartServer(int port, const UmWorldDesc &world);
void UmNetStartClient(const char *host, int port);
// Called by the headless engine after each GameEngine::update (CRC bookkeeping, frame advance broadcast, status).
void UmNetPostUpdate();
bool UmNetCatchUp(unsigned *frame, unsigned *live);   // client: true while still replaying towards the live frame (more than ~5 s behind)
struct UmRentInfo { bool valid; unsigned serverFrame, nextRent, interval, immuneUntil, paid, graceEnd; int missed, amount; bool expiring; UmRentInfo() : valid(false), serverFrame(0), nextRent(0), interval(0), immuneUntil(0), paid(0), graceEnd(0), missed(0), amount(0), expiring(false) {} };
bool UmNetRentInfo(UmRentInfo *out);
unsigned UmNetSpeed();   // logic frames per second of this world
void UmLoadStage(const char *text); void UmLoadTick(const char *what, int pct); void UmLoadDone(void); void UmDrawLoadStatus(void);   // the client's loading overlay (um_hud.cpp)
void UmDrawRentHud(void);   // client: the rent/protection panel (src/client/um_hud.cpp), called from InGameUI::postDraw   // client: the server's latest word on our rent and spawn protection (false until the first message)
int UmNetMySlot();   // client: the seat the server gave us (-1: watching only)
