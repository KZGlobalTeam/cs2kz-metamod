#pragma once
#include "../kz.h"
#include "detectors/strafe_optimizer.h"

class KZBaseService;

class KZAnticheatService : public KZBaseService
{
public:
	using KZBaseService::KZBaseService;

private:
	bool hasValidCvars = true;
	StrafeOptimizerDetector strafeOptDetector;

public:
	bool ShouldCheckClientCvars()
	{
		return hasValidCvars;
	}

	void MarkHasInvalidCvars()
	{
		hasValidCvars = false;
	}

	void OnPlayerFullyConnect();

	virtual void OnSetupMove(PlayerCommand *);

};
