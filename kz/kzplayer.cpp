#include "kzplayer.h"


KZPlayer *KZ::ToKZPlayer(MovementPlayer *player) 
{ 
	return static_cast<KZPlayer *>(player); 
};