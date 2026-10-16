// Living World economy: money pools per faction currency that cap what generators pay out, plus optional GDP inflation.
// All state lives in the simulation (xfer + CRC) and changes only through MSG_UM_ECON_TICK, MSG_UM_CHARGE, MSG_UM_GRANT_CASH and the income calls below,
// so every replica agrees. Compiled only with UM_LW.
#pragma once
#ifdef UM_LW
#include "Common/GameType.h"
class Player; class Xfer; class UnicodeString;

enum UmCurrency { UM_CUR_USD = 0, UM_CUR_CNY = 1, UM_CUR_ILS = 2, UM_CUR_COUNT = 3 };
enum UmIncomeCategory { UM_CAT_DEPOT = 0, UM_CAT_DERRICK = 1, UM_CAT_OTHER = 2, UM_CAT_COUNT = 3 };   // supply depots / oil derricks / other generators (Black Market, hackers, supply drops)

Int UmEconCurrency( const Player *p );                                       // faction group of a player: UM_CUR_*, or -1 (observer, neutral)
Int UmEconDraw( const Player *p, Int category, Int amount, Int sourceId );   // how much of 'amount' the pool lets a generator pay now (the whole amount when pools are off)
Bool UmEconBudgetEmpty( const Player *p, Int category );                     // true when the player's currency category has nothing left this cycle
Real UmEconCostMult( const Player *p );                                      // inflation factor on prices and rent (1.0 when off)
void UmEconSymbol( const Player *p, UnicodeString *out );                    // "$", "¥" or "₪" for the player's currency
void UmEconSwapSymbol( const Player *p, UnicodeString *fmt );                // swap the '$' in a money format string for the player's currency symbol
void UmEconLocalizeCash( UnicodeString *fmt );                               // the same for the local player (floating +$ / -$ texts)
void UmEconXfer( Xfer *xfer );
struct UmEconInfo { Int pool, budget[UM_CAT_COUNT], depotCap, minted, multPermille; };
Bool UmEconGetInfo( Int currency, UmEconInfo *out );                         // false until the economy has been configured
Bool UmEconEnabled( void );
#endif
