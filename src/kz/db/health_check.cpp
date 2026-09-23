#include "kz_db.h"
#include "utils/ctimer.h"

#include "vendor/sql_mm/src/public/sql_mm.h"

// Periodic `SELECT 1` against the MySQL connection. Keeps the connection from being dropped for idling, and makes
// libmysqlclient's auto-reconnect happen on the ping instead of on a real query.
static_global constexpr f64 healthCheckInterval = 60.0;
// Timer tick, also the ping interval while the connection is unhealthy.
static_global constexpr f64 healthRecheckInterval = 5.0;

static_global CTimerBase *healthCheckTimer;
static_global bool healthCheckInFlight;
static_global bool databaseHealthy = true;
static_global f64 lastHealthCheckTime;
static_global f64 unhealthySince;

static_function f64 GetRealTime()
{
	return g_pKZUtils->GetGlobals()->realtime;
}

static_function void OnHealthCheckSuccess(std::vector<ISQLQuery *> queries)
{
	healthCheckInFlight = false;
	if (!databaseHealthy)
	{
		databaseHealthy = true;
		KZ_LOG_INFO(LogChannel::DB, "Database connection is healthy again (unhealthy for %.0fs).\n", GetRealTime() - unhealthySince);
	}
}

static_function void OnHealthCheckFailure(std::string error, int failIndex)
{
	healthCheckInFlight = false;
	if (databaseHealthy)
	{
		databaseHealthy = false;
		unhealthySince = GetRealTime();
		// sql_mm errors end with a newline.
		while (!error.empty() && isspace((unsigned char)error.back()))
		{
			error.pop_back();
		}
		KZ_LOG_WARN(LogChannel::DB, "Database connection is unhealthy: %s\n", error.c_str());
	}
}

static_function f64 RunHealthCheck()
{
	ISQLConnection *connection = KZDatabaseService::GetDatabaseConnection();
	if (!connection)
	{
		// The connection was destroyed, the timer is deleted after returning.
		healthCheckTimer = nullptr;
		return 0.0;
	}

	// A ping can sit in the queue behind slow queries or wait for the read timeout; don't stack more behind it.
	if (healthCheckInFlight)
	{
		return healthRecheckInterval;
	}

	f64 now = GetRealTime();
	if (databaseHealthy && now - lastHealthCheckTime < healthCheckInterval)
	{
		return healthRecheckInterval;
	}

	lastHealthCheckTime = now;
	healthCheckInFlight = true;
	Transaction txn;
	txn.queries.push_back("SELECT 1");
	connection->ExecuteTransaction(txn, OnHealthCheckSuccess, OnHealthCheckFailure);
	return healthRecheckInterval;
}

void KZDatabaseService::StartHealthCheck()
{
	if (healthCheckTimer)
	{
		return;
	}
	databaseHealthy = true;
	healthCheckInFlight = false;
	lastHealthCheckTime = GetRealTime();
	healthCheckTimer = StartTimer(RunHealthCheck, healthRecheckInterval, true, true);
}
