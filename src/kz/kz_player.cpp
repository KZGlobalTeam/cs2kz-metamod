#include "kz.h"

void KZPlayer::OnProcessMovement()
{
	KZ::misc::EnableGodMode(this->index - 1);
}