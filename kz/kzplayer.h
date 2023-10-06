#include "movement/movement.h"

class KZPlayer : public MovementPlayer
{

};

namespace KZ
{
	KZPlayer *ToKZPlayer(MovementPlayer *player) { return static_cast<KZPlayer *>(player); };
}