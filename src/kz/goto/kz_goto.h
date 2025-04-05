#pragma once
#include "../kz.h"

class KZGotoService : public KZBaseService
{
	using KZBaseService::KZBaseService;

private:

public:
	virtual void Reset() override;
	static void Init();

	bool GotoPlayer(const char *playerNamePart);
};
