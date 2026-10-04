#include "bootkitstudio/simulator_registry.hpp"

#include <stdexcept>

namespace bootkitstudio {

extern "C" const char* bks_mbr_stage_marker();
extern "C" const char* bks_vbr_stage_marker();
extern "C" const char* bks_uefi_stage_marker();
extern "C" const char* bks_acpi_annotation_marker();

std::string ResolveStageMarker(const std::string& module_id) {
  if (module_id == "sim.mbr.marker") {
    return std::string(bks_mbr_stage_marker());
  }
  if (module_id == "sim.vbr.marker") {
    return std::string(bks_vbr_stage_marker());
  }
  if (module_id == "sim.uefi.marker") {
    return std::string(bks_uefi_stage_marker());
  }
  if (module_id == "sim.acpi.annotation.marker") {
    return std::string(bks_acpi_annotation_marker());
  }

  throw std::runtime_error("Unknown stage simulator module: " + module_id);
}

}  // namespace bootkitstudio
