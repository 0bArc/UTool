#include "Registry.hpp"

#include "Asset.hpp"
#include "Curve.hpp"

namespace UTool::Lua {

void registerModules(sol::state& lua, ScriptRegistrations& regs) {
  registerCurve(lua, regs);
  registerAsset(lua, regs);
}

}  // namespace UTool::Lua
