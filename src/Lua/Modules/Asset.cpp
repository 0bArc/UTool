#include "Asset.hpp"

#include "../HostInternal.hpp"
#include "../SolJson.hpp"

#include "UTool/Mod/JsonEditor.hpp"

#include <stdexcept>
#include <string>

namespace UTool::Lua {

void registerAsset(sol::state& lua, ScriptRegistrations& regs) {
  lua.new_usertype<Mod::JsonAssetEditor>(
      "JsonAssetEditor",
      "Get",
      [](Mod::JsonAssetEditor& self, const std::string& pointer, sol::this_state s) {
        return jsonToSol(sol::state_view(s), self.get(pointer));
      },
      "Replace",
      [](Mod::JsonAssetEditor& self, const std::string& pointer, const sol::object& value) {
        self.replace(pointer, solToJson(value));
      },
      "Add",
      [](Mod::JsonAssetEditor& self, const std::string& pointer, const sol::object& value) {
        self.add(pointer, solToJson(value));
      },
      "Set",
      [](Mod::JsonAssetEditor& self, const std::string& pointer, const sol::object& value) {
        self.set(pointer, solToJson(value));
      },
      "Append",
      [](Mod::JsonAssetEditor& self, const std::string& pointer, const sol::object& value) {
        self.append(pointer, solToJson(value));
      },
      "MergeInto",
      [](Mod::JsonAssetEditor& self, const std::string& pointer, const sol::object& value) {
        self.mergeInto(pointer, solToJson(value));
      },
      "Remove", &Mod::JsonAssetEditor::remove,
      "ReplaceAll",
      sol::overload(
          [](Mod::JsonAssetEditor& self, const std::string& name, const sol::object& value) {
            self.replaceAll(name, solToJson(value));
          },
          [](Mod::JsonAssetEditor& self, const std::string& name, const sol::object& value,
             const std::string& under) {
            self.replaceAll(name, solToJson(value), under);
          }),
      "MapArray",
      [](Mod::JsonAssetEditor& self, const std::string& arrayPointer, sol::protected_function fn,
         sol::this_state s) {
        sol::state_view view(s);
        return self.mapArray(arrayPointer, [&](nlohmann::json item) {
          auto call = fn(jsonToSol(view, item));
          if (!call.valid()) {
            sol::error err = call;
            throw std::runtime_error(std::string("MapArray callback failed: ") + err.what());
          }
          return solToJson(call.get<sol::object>());
        });
      },
      "SetOnArrayElementsWhere",
      [](Mod::JsonAssetEditor& self, const std::string& arrayPointer, const std::string& matchProperty,
         const sol::object& matchValue, const std::string& propertyPointer,
         const sol::object& value) {
        return self.setOnArrayElementsWhere(arrayPointer, matchProperty, solToJson(matchValue),
                                            propertyPointer, solToJson(value));
      },
      "RemoveArrayElementsWhere",
      [](Mod::JsonAssetEditor& self, const std::string& arrayPointer, const std::string& matchProperty,
         const sol::object& matchValue) {
        return self.removeArrayElementsWhere(arrayPointer, matchProperty, solToJson(matchValue));
      });

  auto assetFnKeep = detail::makeFunctionKeep(lua, "__utool_asset_fns");
  sol::table utool = lua["utool"];

  utool.set_function(
      "patch_asset",
      [&regs, assetFnKeep](const std::string& assetFileName, sol::variadic_args va) {
        std::string relative;
        sol::protected_function fn;
        if (va.size() >= 2 && va[0].is<std::string>() && va[1].is<sol::function>()) {
          relative = va[0].as<std::string>();
          fn = va[1].as<sol::protected_function>();
        } else if (va.size() >= 1 && va[0].is<sol::function>()) {
          fn = va[0].as<sol::protected_function>();
        } else {
          throw std::runtime_error("utool.patch_asset(file [, relativeDir], function)");
        }

        assetFnKeep->push_back(fn);
        AssetRegistration reg;
        reg.assetFileName = assetFileName;
        reg.relativeDirectory = relative;
        auto kept = assetFnKeep->back();
        auto luaKeep = regs.lua;
        reg.apply = [kept, luaKeep](Mod::JsonAssetEditor& editor) {
          sol::state& state = *luaKeep;
          state["utool"]["editor"] = std::ref(editor);
          auto result = kept();
          state["utool"]["editor"] = sol::lua_nil;
          if (!result.valid()) {
            sol::error err = result;
            throw std::runtime_error(std::string("Lua asset patch failed: ") + err.what());
          }
        };
        regs.assets.push_back(std::move(reg));
      });
}

}  // namespace UTool::Lua
