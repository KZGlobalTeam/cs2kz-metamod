#include "kz_replaysystem.h"
#include "ledger.h"
#include "kz/option/kz_option.h"

namespace KZ::replaysystem
{
	void OnAPIUploadFinished(const UUID_t &uuid, bool uploaded)
	{
		ledger::UpdateEntry(uuid,
							[uploaded](ledger::Entry &entry)
							{
								if (!entry.uploadToAPI)
								{
									return;
								}
								entry.uploadToAPI = false;
								if (uploaded)
								{
									entry.offloaded =
										entry.offloaded || KZOptionService::GetOptionInt("replays/disk/deleteGlobalRunsAfterUpload", false);
								}
							});
	}
} // namespace KZ::replaysystem
