#include "kz_style.h"

class KZNormalStyleService : public KZStyleService
{
	using KZStyleService::KZStyleService;
public:
	virtual const char *GetStyleName() override { return "Normal"; }
	virtual const char *GetStyleShortName() override { return "NRM"; }
};
