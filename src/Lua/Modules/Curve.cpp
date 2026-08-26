#include "Curve.hpp"

#include "../HostInternal.hpp"

#include "UTool/Mod/CurveFloat.hpp"

#include <stdexcept>
#include <string>

namespace UTool::Lua {

void registerCurve(sol::state& lua, ScriptRegistrations& regs) {
  lua.new_usertype<Mod::CurveKey>("CurveKey",
                                  "Time", &Mod::CurveKey::time,
                                  "Value", &Mod::CurveKey::value,
                                  "time", &Mod::CurveKey::time,
                                  "value", &Mod::CurveKey::value);

  lua.new_usertype<Mod::CurveEditor>(
      "CurveEditor",
      "AssetName",
      sol::property([](const Mod::CurveEditor& e) { return e.assetName(); }),
      "LastKey", &Mod::CurveEditor::lastKey,
      "AddKey", &Mod::CurveEditor::addKey,
      "SetKey", &Mod::CurveEditor::setKey);

  auto curveFnKeep = detail::makeFunctionKeep(lua, "__utool_curve_fns");
  sol::table utool = lua["utool"];

  utool.set_function(
      "patch_curve",
      [&regs, curveFnKeep](const std::string& assetName, sol::variadic_args va) {
        std::string relative;
        sol::protected_function fn;
        if (va.size() >= 2 && va[0].is<std::string>() && va[1].is<sol::function>()) {
          relative = va[0].as<std::string>();
          fn = va[1].as<sol::protected_function>();
        } else if (va.size() >= 1 && va[0].is<sol::function>()) {
          fn = va[0].as<sol::protected_function>();
        } else {
          throw std::runtime_error("utool.patch_curve(assetName [, relativeDir], function)");
        }

        curveFnKeep->push_back(fn);
        CurveRegistration reg;
        reg.assetName = assetName;
        reg.relativeDirectory = relative;
        auto kept = curveFnKeep->back();
        auto luaKeep = regs.lua;
        reg.apply = [kept, luaKeep](Mod::CurveEditor& editor) {
          sol::state& state = *luaKeep;
          state["utool"]["curve"] = std::ref(editor);
          auto result = kept();
          state["utool"]["curve"] = sol::lua_nil;
          if (!result.valid()) {
            sol::error err = result;
            throw std::runtime_error(std::string("Lua curve patch failed: ") + err.what());
          }
        };
        regs.curves.push_back(std::move(reg));
      });
}

}  // namespace UTool::Lua
