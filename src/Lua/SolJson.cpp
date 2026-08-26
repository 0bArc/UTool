#include "SolJson.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace UTool::Lua {
namespace {

nlohmann::json solTableToJson(const sol::table& table) {
  bool arrayLike = true;
  int maxIndex = 0;
  int count = 0;
  table.for_each([&](const sol::object& key, const sol::object&) {
    ++count;
    if (!key.is<int>() && !key.is<double>()) {
      arrayLike = false;
      return;
    }
    const int idx = key.is<int>() ? key.as<int>() : static_cast<int>(key.as<double>());
    if (idx < 1)
      arrayLike = false;
    maxIndex = std::max(maxIndex, idx);
  });
  if (arrayLike && count > 0 && maxIndex == count) {
    nlohmann::json arr = nlohmann::json::array();
    for (int i = 1; i <= maxIndex; ++i)
      arr.push_back(solToJson(table[i]));
    return arr;
  }

  nlohmann::json obj = nlohmann::json::object();
  table.for_each([&](const sol::object& key, const sol::object& value) {
    std::string k;
    if (key.is<std::string>())
      k = key.as<std::string>();
    else if (key.is<int>())
      k = std::to_string(key.as<int>());
    else
      k = "key";
    obj[k] = solToJson(value);
  });
  return obj;
}

sol::object jsonToSolValue(sol::state_view lua, const nlohmann::json& value) {
  if (value.is_null())
    return sol::make_object(lua, sol::lua_nil);
  if (value.is_boolean())
    return sol::make_object(lua, value.get<bool>());
  if (value.is_number_integer())
    return sol::make_object(lua, value.get<std::int64_t>());
  if (value.is_number_unsigned())
    return sol::make_object(lua, value.get<std::uint64_t>());
  if (value.is_number_float())
    return sol::make_object(lua, value.get<double>());
  if (value.is_string())
    return sol::make_object(lua, value.get<std::string>());
  if (value.is_array()) {
    sol::table table = lua.create_table(static_cast<int>(value.size()), 0);
    int i = 1;
    for (const auto& item : value)
      table[i++] = jsonToSolValue(lua, item);
    return table;
  }
  if (value.is_object()) {
    sol::table table = lua.create_table(0, static_cast<int>(value.size()));
    for (auto it = value.begin(); it != value.end(); ++it)
      table[it.key()] = jsonToSolValue(lua, it.value());
    return table;
  }
  return sol::make_object(lua, sol::lua_nil);
}

}  // namespace

nlohmann::json solToJson(const sol::object& obj) {
  switch (obj.get_type()) {
    case sol::type::nil:
      return nullptr;
    case sol::type::boolean:
      return obj.as<bool>();
    case sol::type::number: {
      const double d = obj.as<double>();
      if (std::floor(d) == d && d >= static_cast<double>(std::numeric_limits<int>::min()) &&
          d <= static_cast<double>(std::numeric_limits<int>::max()))
        return static_cast<int>(d);
      return d;
    }
    case sol::type::string:
      return obj.as<std::string>();
    case sol::type::table:
      return solTableToJson(obj.as<sol::table>());
    default:
      return nullptr;
  }
}

sol::object jsonToSol(sol::state_view lua, const nlohmann::json& value) {
  return jsonToSolValue(lua, value);
}

}  // namespace UTool::Lua
