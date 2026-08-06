#pragma once
#include "config/config_diagnostics.hpp"
#include "config/config_service.hpp"
#include "storage/device_config_store.hpp"
namespace config {
void initialize_from_storage(ConfigService &, const storage::LoadResult &,
                             const storage::FlashRegion &,
                             ConfigurationDiagnostics &);
}
