#include "UTool/Mod/CurveFloat.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace UTool::Mod {
namespace {

bool iequals(std::string_view a, std::string_view b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  }
  return true;
}

float parseFloatNode(const nlohmann::json& node) {
  if (node.is_number_float() || node.is_number_integer())
    return node.get<float>();
  if (node.is_string()) {
    auto s = node.get<std::string>();
    if (!s.empty() && s.front() == '+')
      s.erase(s.begin());
    return std::stof(s);
  }
  return 0.f;
}

void collectSimpleKeyArrays(nlohmann::json& node, std::vector<nlohmann::json*>& out) {
  if (node.is_object()) {
    for (auto it = node.begin(); it != node.end(); ++it) {
      if (iequals(it.key(), "Keys") && it.value().is_array() && !it.value().empty() &&
          it.value()[0].is_object() &&
          (it.value()[0].contains("Time") || it.value()[0].contains("time"))) {
        out.push_back(&it.value());
      }
      collectSimpleKeyArrays(it.value(), out);
    }
  } else if (node.is_array()) {
    for (auto& item : node)
      collectSimpleKeyArrays(item, out);
  }
}

struct UAssetApiKeyRef {
  nlohmann::json* parentArray = nullptr;
  nlohmann::json* entry = nullptr;
};

void collectUAssetApiKeys(nlohmann::json& node, std::vector<UAssetApiKeyRef>& out) {
  if (node.is_object()) {
    const bool isKeys =
        node.contains("Name") && node["Name"].is_string() &&
        iequals(node["Name"].get<std::string>(), "Keys") && node.contains("$type") &&
        node["$type"].is_string() &&
        node["$type"].get<std::string>().find("ArrayPropertyData") != std::string::npos;

    if (isKeys && node.contains("Value") && node["Value"].is_array() && !node["Value"].empty() &&
        node["Value"][0].is_object()) {
      const auto& first = node["Value"][0];
      const bool rich =
          first.contains("$type") && first["$type"].is_string() &&
          first["$type"].get<std::string>().find("StructPropertyData") != std::string::npos &&
          first.contains("StructType") && first["StructType"].is_string() &&
          iequals(first["StructType"].get<std::string>(), "RichCurveKey");
      if (rich) {
        auto& arr = node["Value"];
        for (auto& entry : arr) {
          if (entry.is_object())
            out.push_back(UAssetApiKeyRef{&arr, &entry});
        }
        return;
      }
    }
    for (auto& [_, child] : node.items())
      collectUAssetApiKeys(child, out);
  } else if (node.is_array()) {
    for (auto& item : node)
      collectUAssetApiKeys(item, out);
  }
}

float readUAssetApiKeyTime(const nlohmann::json& entry) {
  if (entry.contains("Value") && entry["Value"].is_array() && !entry["Value"].empty() &&
      entry["Value"][0].is_object() && entry["Value"][0].contains("Value") &&
      entry["Value"][0]["Value"].is_object())
    return parseFloatNode(entry["Value"][0]["Value"].value("Time", 0.f));
  if (entry.contains("Value") && entry["Value"].is_object())
    return parseFloatNode(entry["Value"].value("Time", 0.f));
  return 0.f;
}

float readUAssetApiKeyValue(const nlohmann::json& entry) {
  if (entry.contains("Value") && entry["Value"].is_array() && !entry["Value"].empty() &&
      entry["Value"][0].is_object() && entry["Value"][0].contains("Value") &&
      entry["Value"][0]["Value"].is_object())
    return parseFloatNode(entry["Value"][0]["Value"].value("Value", 0.f));
  if (entry.contains("Value") && entry["Value"].is_object())
    return parseFloatNode(entry["Value"].value("Value", 0.f));
  return 0.f;
}

nlohmann::json createUAssetApiKeyEntry(const nlohmann::json& templateEntry, float time, float value) {
  nlohmann::json clone = templateEntry;
  const std::string t = nlohmann::json(time).dump();
  const std::string v = nlohmann::json(value).dump();
  // Prefer numeric write
  if (clone.contains("Value") && clone["Value"].is_array() && !clone["Value"].empty() &&
      clone["Value"][0].is_object() && clone["Value"][0].contains("Value") &&
      clone["Value"][0]["Value"].is_object()) {
    clone["Value"][0]["Value"]["Time"] = time;
    clone["Value"][0]["Value"]["Value"] = value;
  } else if (clone.contains("Value") && clone["Value"].is_object()) {
    clone["Value"]["Time"] = time;
    clone["Value"]["Value"] = value;
  }
  (void)t;
  (void)v;
  return clone;
}

bool tryPatchUAssetApiJson(nlohmann::json& root, const CurveFloatPatchSpec& spec) {
  std::vector<UAssetApiKeyRef> keyEntries;
  collectUAssetApiKeys(root, keyEntries);
  if (keyEntries.empty())
    return false;

  auto* parentArray = keyEntries[0].parentArray;
  const nlohmann::json templateEntry = *keyEntries[0].entry;
  const float minTime = spec.minPatchTime
                            ? *spec.minPatchTime
                            : std::min_element(spec.keys.begin(), spec.keys.end(),
                                               [](const CurveKey& a, const CurveKey& b) {
                                                 return a.time < b.time;
                                               })
                                  ->time;

  nlohmann::json rebuilt = nlohmann::json::array();
  if (spec.extendFromVanilla) {
    for (const auto& entry : keyEntries) {
      const float time = readUAssetApiKeyTime(*entry.entry);
      if (time < minTime)
        rebuilt.push_back(*entry.entry);
    }
  }

  auto keys = spec.keys;
  std::sort(keys.begin(), keys.end(),
            [](const CurveKey& a, const CurveKey& b) { return a.time < b.time; });
  for (const auto& key : keys)
    rebuilt.push_back(createUAssetApiKeyEntry(templateEntry, key.time, key.value));

  *parentArray = std::move(rebuilt);
  return true;
}

void patchSimpleKeyArrays(std::vector<nlohmann::json*>& keysArrays, const CurveFloatPatchSpec& spec) {
  const float minTime = spec.minPatchTime
                            ? *spec.minPatchTime
                            : std::min_element(spec.keys.begin(), spec.keys.end(),
                                               [](const CurveKey& a, const CurveKey& b) {
                                                 return a.time < b.time;
                                               })
                                  ->time;

  nlohmann::json newKeys = nlohmann::json::array();
  if (spec.extendFromVanilla && !keysArrays.empty()) {
    for (const auto& item : *keysArrays[0]) {
      if (!item.is_object())
        continue;
      float time = 0.f;
      if (item.contains("Time"))
        time = parseFloatNode(item["Time"]);
      else if (item.contains("time"))
        time = parseFloatNode(item["time"]);
      if (time < minTime)
        newKeys.push_back(item);
    }
  }

  auto keys = spec.keys;
  std::sort(keys.begin(), keys.end(),
            [](const CurveKey& a, const CurveKey& b) { return a.time < b.time; });
  for (const auto& key : keys) {
    newKeys.push_back({
        {"Time", key.time},
        {"Value", key.value},
        {"InterpMode", "RCIM_Linear"},
        {"TangentMode", "RCTM_Auto"},
    });
  }

  for (auto* array : keysArrays)
    *array = newKeys;
}

std::string readTextFile(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    throw std::runtime_error("Cannot read " + path.string());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

void writeTextFile(const std::filesystem::path& path, const std::string& text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  if (!out)
    throw std::runtime_error("Cannot write " + path.string());
  out << text;
}

bool looksLikeJson(const std::string& text) {
  for (char c : text) {
    if (std::isspace(static_cast<unsigned char>(c)))
      continue;
    return c == '{' || c == '[';
  }
  return false;
}

#pragma pack(push, 1)
// Cooked Icarus/UE4 CurveFloat Keys: 27-byte RichCurveKey with weights before mode bytes.
// (Editor FRichCurveKey is often 28 with a pad; these packages omit the pad.)
struct RichCurveKeyBin {
  float time = 0.f;
  float value = 0.f;
  float arriveTangent = 0.f;
  float leaveTangent = 0.f;
  float arriveTangentWeight = 0.f;
  float leaveTangentWeight = 0.f;
  std::uint8_t interpMode = 0;
  std::uint8_t tangentMode = 0;
  std::uint8_t tangentWeightMode = 0;
};
#pragma pack(pop)

static_assert(sizeof(RichCurveKeyBin) == 27, "cooked RichCurveKey is 27 bytes");

bool isPlausibleKey(const RichCurveKeyBin& key, float prevTime, bool hasPrev) {
  if (!std::isfinite(key.time) || !std::isfinite(key.value) || !std::isfinite(key.arriveTangent) ||
      !std::isfinite(key.leaveTangent) || !std::isfinite(key.arriveTangentWeight) ||
      !std::isfinite(key.leaveTangentWeight))
    return false;
  // Cooked curves often start with a dummy (0,0) key before level keys.
  if (!hasPrev && key.time == 0.f && key.value == 0.f)
    return true;
  // Do not require mode bytes ≤4: the last vanilla key can overlap the next
  // property tag (e.g. 0x0A / 0x0E) while Time/Value are still real.
  if (hasPrev && key.time <= prevTime)
    return false;
  if (key.time < 1.f || key.time > 100000.f || key.value <= 0.f)
    return false;
  if (std::fabs(key.time - std::round(key.time)) > 1e-3f)
    return false;
  return true;
}

bool findKeyRun(
    const std::vector<std::uint8_t>& data,
    size_t& bestOff,
    size_t& bestCount) {
  bestOff = 0;
  bestCount = 0;
  constexpr size_t kSize = sizeof(RichCurveKeyBin);
  for (size_t off = 0; off + kSize <= data.size(); ++off) {
    size_t count = 0;
    float prev = 0.f;
    while (off + (count + 1) * kSize <= data.size()) {
      RichCurveKeyBin key{};
      std::memcpy(&key, data.data() + off + count * kSize, kSize);
      if (!isPlausibleKey(key, prev, count > 0))
        break;
      prev = key.time;
      ++count;
      if (count > 5000)
        break;
    }
    if (count > bestCount && count >= 2) {
      bestCount = count;
      bestOff = off;
    }
  }
  return bestCount >= 2;
}

// Icarus cooked CurveFloat uexp layout (verified against UAssetAPI):
//   int64 FloatCurve size @16
//   int64 nested size     @65  (== keys bytes + 53)
//   int32 Keys count      @82
//   int64 Keys byte size  @102 (== count * 27)
//   RichCurveKey[]        starts at first key of the run (often @138)
constexpr size_t kFloatCurveSizeOff = 16;
constexpr size_t kKeysMidSizeOff = 65;
constexpr size_t kKeysCountOff = 82;
constexpr size_t kKeysByteSizeOff = 102;
constexpr std::int64_t kKeysMidSizePadding = 53;

bool readI32(const std::vector<std::uint8_t>& data, size_t off, std::int32_t& out) {
  if (off + 4 > data.size())
    return false;
  std::memcpy(&out, data.data() + off, 4);
  return true;
}

bool readI64(const std::vector<std::uint8_t>& data, size_t off, std::int64_t& out) {
  if (off + 8 > data.size())
    return false;
  std::memcpy(&out, data.data() + off, 8);
  return true;
}

void writeI32(std::vector<std::uint8_t>& data, size_t off, std::int32_t value) {
  std::memcpy(data.data() + off, &value, 4);
}

void writeI64(std::vector<std::uint8_t>& data, size_t off, std::int64_t value) {
  std::memcpy(data.data() + off, &value, 8);
}

bool updateUassetExportSizes(const std::filesystem::path& uassetPath, std::int64_t oldSerialSize,
                             std::int64_t addBytes) {
  std::ifstream in(uassetPath, std::ios::binary);
  if (!in)
    return false;
  std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
  if (data.size() < 16)
    return false;

  const std::int64_t uassetLen = static_cast<std::int64_t>(data.size());
  const std::int64_t newSerialSize = oldSerialSize + addBytes;
  const std::int64_t oldTotal = uassetLen + oldSerialSize;
  const std::int64_t newTotal = uassetLen + newSerialSize;
  bool foundSerial = false;

  for (size_t i = 0; i + 16 <= data.size(); ++i) {
    std::int64_t serial = 0;
    std::int64_t offset = 0;
    std::memcpy(&serial, data.data() + i, 8);
    std::memcpy(&offset, data.data() + i + 8, 8);
    if (serial == oldSerialSize && offset == uassetLen) {
      writeI64(data, i, newSerialSize);
      foundSerial = true;
      break;
    }
  }
  if (!foundSerial) {
    for (size_t i = 0; i + 8 <= data.size(); ++i) {
      std::int64_t serial = 0;
      std::memcpy(&serial, data.data() + i, 8);
      if (serial == oldSerialSize) {
        writeI64(data, i, newSerialSize);
        foundSerial = true;
        break;
      }
    }
  }
  if (!foundSerial)
    return false;

  // Bulk/export end marker: uasset length + SerialSize.
  for (size_t i = 0; i + 8 <= data.size(); ++i) {
    std::int64_t total = 0;
    std::memcpy(&total, data.data() + i, 8);
    if (total == oldTotal) {
      writeI64(data, i, newTotal);
      break;
    }
  }

  std::ofstream out(uassetPath, std::ios::binary | std::ios::trunc);
  if (!out)
    return false;
  out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  return true;
}

bool tryPatchBinaryCurve(
    const std::filesystem::path& uassetPath,
    const CurveFloatPatchSpec& spec) {
  const auto uexpPath = std::filesystem::path(uassetPath).replace_extension(".uexp");
  if (!std::filesystem::is_regular_file(uexpPath))
    return false;

  std::ifstream in(uexpPath, std::ios::binary);
  if (!in)
    return false;
  std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
  if (data.size() < 4)
    return false;

  size_t bestOff = 0;
  size_t bestCount = 0;
  if (!findKeyRun(data, bestOff, bestCount))
    return false;

  std::int32_t declaredCount = 0;
  std::int64_t declaredKeyBytes = 0;
  const bool hasArrayMeta =
      readI32(data, kKeysCountOff, declaredCount) && readI64(data, kKeysByteSizeOff, declaredKeyBytes) &&
      declaredCount > 0 &&
      declaredKeyBytes == static_cast<std::int64_t>(declaredCount) * static_cast<std::int64_t>(sizeof(RichCurveKeyBin)) &&
      bestOff + static_cast<size_t>(declaredCount) * sizeof(RichCurveKeyBin) <= data.size() &&
      static_cast<size_t>(declaredCount) >= bestCount;

  // Prefer UAssetAPI-declared count when the heuristic run is a suffix of it
  // (heuristic may skip the leading dummy 0/0 key if detection starts late).
  size_t keyCount = bestCount;
  size_t keyOff = bestOff;
  if (hasArrayMeta) {
    const size_t metaEnd = bestOff + bestCount * sizeof(RichCurveKeyBin);
    const size_t declaredBytes = static_cast<size_t>(declaredCount) * sizeof(RichCurveKeyBin);
    if (metaEnd >= declaredBytes) {
      keyOff = metaEnd - declaredBytes;
      keyCount = static_cast<size_t>(declaredCount);
    }
  }

  RichCurveKeyBin lastVanilla{};
  std::memcpy(&lastVanilla, data.data() + keyOff + (keyCount - 1) * sizeof(RichCurveKeyBin),
              sizeof(lastVanilla));
  // minTime should be the last progression key (>=1), not a trailing dummy.
  float lastProgressionTime = lastVanilla.time;
  for (size_t i = keyCount; i > 0; --i) {
    RichCurveKeyBin k{};
    std::memcpy(&k, data.data() + keyOff + (i - 1) * sizeof(RichCurveKeyBin), sizeof(k));
    if (k.time >= 1.f && k.value > 0.f) {
      lastProgressionTime = k.time;
      break;
    }
  }

  const float minTime = spec.minPatchTime ? *spec.minPatchTime : lastProgressionTime;

  RichCurveKeyBin templ{};
  templ.interpMode = 0;   // RCIM_Linear
  templ.tangentMode = 0;  // RCTM_Auto
  templ.tangentWeightMode = 0;

  std::vector<RichCurveKeyBin> toAppend;
  auto keys = spec.keys;
  std::sort(keys.begin(), keys.end(),
            [](const CurveKey& a, const CurveKey& b) { return a.time < b.time; });
  for (const auto& k : keys) {
    if (spec.extendFromVanilla && k.time <= minTime + 1e-4f)
      continue;
    RichCurveKeyBin key = templ;
    key.time = k.time;
    key.value = k.value;
    toAppend.push_back(key);
  }
  if (toAppend.empty())
    return true;

  const size_t insertAt = keyOff + keyCount * sizeof(RichCurveKeyBin);
  const size_t addBytes = toAppend.size() * sizeof(RichCurveKeyBin);
  const std::int64_t oldSerialSize = static_cast<std::int64_t>(data.size()) - 4;

  // The last 3 bytes of the final key overlap the next property tag (name index).
  // Keep them on the new last key; clear them on the old last key now that it is
  // in the middle of the array (matches UAssetAPI).
  RichCurveKeyBin oldLast{};
  std::memcpy(&oldLast, data.data() + keyOff + (keyCount - 1) * sizeof(RichCurveKeyBin),
              sizeof(oldLast));
  toAppend.back().interpMode = oldLast.interpMode;
  toAppend.back().tangentMode = oldLast.tangentMode;
  toAppend.back().tangentWeightMode = oldLast.tangentWeightMode;
  oldLast.interpMode = 0;
  oldLast.tangentMode = 0;
  oldLast.tangentWeightMode = 0;
  std::memcpy(data.data() + keyOff + (keyCount - 1) * sizeof(RichCurveKeyBin), &oldLast,
              sizeof(oldLast));

  std::vector<std::uint8_t> out;
  out.reserve(data.size() + addBytes);
  out.insert(out.end(), data.begin(), data.begin() + static_cast<std::ptrdiff_t>(insertAt));
  for (const auto& key : toAppend) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(&key);
    out.insert(out.end(), p, p + sizeof(key));
  }
  out.insert(out.end(), data.begin() + static_cast<std::ptrdiff_t>(insertAt), data.end());

  std::int64_t floatCurveSize = 0;
  if (readI64(out, kFloatCurveSizeOff, floatCurveSize))
    writeI64(out, kFloatCurveSizeOff, floatCurveSize + static_cast<std::int64_t>(addBytes));

  if (hasArrayMeta) {
    writeI32(out, kKeysCountOff, declaredCount + static_cast<std::int32_t>(toAppend.size()));
    writeI64(out, kKeysByteSizeOff, declaredKeyBytes + static_cast<std::int64_t>(addBytes));
    // Nested StructProperty size between FloatCurve and Keys (keys bytes + 53).
    std::int64_t midSize = 0;
    if (readI64(out, kKeysMidSizeOff, midSize) &&
        midSize == declaredKeyBytes + kKeysMidSizePadding)
      writeI64(out, kKeysMidSizeOff, midSize + static_cast<std::int64_t>(addBytes));
  }

  std::ofstream uexpOut(uexpPath, std::ios::binary | std::ios::trunc);
  if (!uexpOut)
    return false;
  uexpOut.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
  uexpOut.close();

  if (!updateUassetExportSizes(uassetPath, oldSerialSize, static_cast<std::int64_t>(addBytes)))
    return false;
  return true;
}

}  // namespace

CurveEditor::CurveEditor(std::string assetName, std::vector<CurveKey> vanillaKeys)
    : assetName_(std::move(assetName)), keys_(std::move(vanillaKeys)) {
  std::sort(keys_.begin(), keys_.end(),
            [](const CurveKey& a, const CurveKey& b) { return a.time < b.time; });
}

CurveKey CurveEditor::lastKey() const {
  if (keys_.empty())
    throw std::runtime_error("Curve '" + assetName_ + "' has no keys.");
  return *std::max_element(keys_.begin(), keys_.end(),
                           [](const CurveKey& a, const CurveKey& b) { return a.time < b.time; });
}

void CurveEditor::addKey(float time, float value) {
  for (auto& key : keys_) {
    if (std::fabs(key.time - time) < 1e-4f) {
      key = CurveKey{time, value};
      return;
    }
  }
  keys_.push_back(CurveKey{time, value});
}

void CurveEditor::scaleValues(float factor) {
  for (auto& key : keys_)
    key.value *= factor;
}

std::vector<CurveKey> readCurveKeysFromJson(const nlohmann::json& root) {
  std::vector<UAssetApiKeyRef> api;
  // const_cast for collector that mutates structure identically for walk — use const walk
  nlohmann::json copy = root;
  collectUAssetApiKeys(copy, api);
  if (!api.empty()) {
    std::vector<CurveKey> keys;
    keys.reserve(api.size());
    for (const auto& e : api)
      keys.push_back(CurveKey{readUAssetApiKeyTime(*e.entry), readUAssetApiKeyValue(*e.entry)});
    std::sort(keys.begin(), keys.end(),
              [](const CurveKey& a, const CurveKey& b) { return a.time < b.time; });
    return keys;
  }

  std::vector<nlohmann::json*> arrays;
  collectSimpleKeyArrays(copy, arrays);
  if (arrays.empty())
    return {};

  std::vector<CurveKey> keys;
  for (const auto& item : *arrays[0]) {
    if (!item.is_object())
      continue;
    float time = item.contains("Time") ? parseFloatNode(item["Time"])
                 : item.contains("time") ? parseFloatNode(item["time"])
                                         : 0.f;
    float value = item.contains("Value") ? parseFloatNode(item["Value"])
                  : item.contains("value") ? parseFloatNode(item["value"])
                                           : 0.f;
    keys.push_back(CurveKey{time, value});
  }
  std::sort(keys.begin(), keys.end(),
            [](const CurveKey& a, const CurveKey& b) { return a.time < b.time; });
  return keys;
}

nlohmann::json patchRichCurveKeysJson(nlohmann::json root, const CurveFloatPatchSpec& spec) {
  if (spec.keys.empty())
    throw std::runtime_error("Curve patch has no keys");
  if (tryPatchUAssetApiJson(root, spec))
    return root;
  std::vector<nlohmann::json*> arrays;
  collectSimpleKeyArrays(root, arrays);
  if (arrays.empty())
    throw std::runtime_error("No RichCurve Keys array found in UAsset JSON.");
  patchSimpleKeyArrays(arrays, spec);
  return root;
}

std::vector<CurveKey> readCurveKeys(const std::filesystem::path& assetOrJsonPath) {
  const auto jsonSidecar = assetOrJsonPath.string() + ".json";
  std::filesystem::path path = assetOrJsonPath;
  if (!std::filesystem::is_regular_file(path) && std::filesystem::is_regular_file(jsonSidecar))
    path = jsonSidecar;
  else if (std::filesystem::is_regular_file(jsonSidecar) &&
           !looksLikeJson(readTextFile(assetOrJsonPath)))
    path = jsonSidecar;

  if (!std::filesystem::is_regular_file(path))
    throw std::runtime_error("Curve asset not found: " + assetOrJsonPath.string());

  const auto text = readTextFile(path);
  if (looksLikeJson(text)) {
    auto root = nlohmann::json::parse(text);
    // .curve.json spec?
    if (root.contains("keys") && root["keys"].is_array()) {
      std::vector<CurveKey> keys;
      for (const auto& k : root["keys"]) {
        keys.push_back(CurveKey{
            k.value("time", k.value("Time", 0.f)),
            k.value("value", k.value("Value", 0.f)),
        });
      }
      return keys;
    }
    return readCurveKeysFromJson(root);
  }

  // Binary: recover times/values from best RichCurveKey run in .uexp
  const auto uexpPath = std::filesystem::path(assetOrJsonPath).replace_extension(".uexp");
  if (!std::filesystem::is_regular_file(uexpPath))
    return {};

  std::ifstream in(uexpPath, std::ios::binary);
  std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
  size_t bestOff = 0;
  size_t bestCount = 0;
  if (!findKeyRun(data, bestOff, bestCount))
    return {};

  std::vector<CurveKey> keys;
  keys.reserve(bestCount);
  for (size_t i = 0; i < bestCount; ++i) {
    RichCurveKeyBin key{};
    std::memcpy(&key, data.data() + bestOff + i * sizeof(RichCurveKeyBin), sizeof(key));
    keys.push_back(CurveKey{key.time, key.value});
  }
  return keys;
}

void applyCurveKeys(const std::filesystem::path& uassetPath, const CurveFloatPatchSpec& spec) {
  if (!std::filesystem::is_regular_file(uassetPath))
    throw std::runtime_error("Curve uasset not found: " + uassetPath.string());

  const auto text = readTextFile(uassetPath);
  if (looksLikeJson(text)) {
    auto patched = patchRichCurveKeysJson(nlohmann::json::parse(text), spec);
    writeTextFile(uassetPath, patched.dump(-1));
    return;
  }

  const auto sidecar = uassetPath.string() + ".json";
  if (std::filesystem::is_regular_file(sidecar)) {
    auto patched = patchRichCurveKeysJson(nlohmann::json::parse(readTextFile(sidecar)), spec);
    writeTextFile(sidecar, patched.dump(2));
  }

  if (!tryPatchBinaryCurve(uassetPath, spec)) {
    throw std::runtime_error(
        "Failed to patch CurveFloat binary keys for " + uassetPath.string() +
        ". Provide a UAssetAPI JSON sidecar (" + sidecar + ") or valid .uexp keys.");
  }
}

std::vector<CurveFloatPatchSpec> readCurveJsonDirectory(const std::filesystem::path& curvesDir) {
  std::vector<CurveFloatPatchSpec> specs;
  if (!std::filesystem::is_directory(curvesDir))
    return specs;

  for (const auto& entry : std::filesystem::directory_iterator(curvesDir)) {
    if (!entry.is_regular_file())
      continue;
    const auto name = entry.path().filename().string();
    if (name.size() < 11 || name.substr(name.size() - 11) != ".curve.json")
      continue;

    auto root = nlohmann::json::parse(readTextFile(entry.path()));
    CurveFloatPatchSpec spec;
    spec.assetName = root.value("assetName", root.value("AssetName", ""));
    spec.relativeDirectory =
        root.value("relativeDirectory", root.value("RelativeDirectory", "Data/Character"));
    spec.extendFromVanilla = root.value("extendFromVanilla", root.value("ExtendFromVanilla", true));
    if (root.contains("minPatchTime"))
      spec.minPatchTime = root["minPatchTime"].get<float>();
    else if (root.contains("MinPatchTime"))
      spec.minPatchTime = root["MinPatchTime"].get<float>();

    const auto& keysNode = root.contains("keys") ? root["keys"] : root["Keys"];
    if (!keysNode.is_array())
      throw std::runtime_error("Curve patch has no keys: " + entry.path().string());
    for (const auto& k : keysNode) {
      spec.keys.push_back(CurveKey{
          k.value("time", k.value("Time", 0.f)),
          k.value("value", k.value("Value", 0.f)),
      });
    }
    if (spec.assetName.empty() || spec.keys.empty())
      throw std::runtime_error("Invalid curve patch: " + entry.path().string());
    specs.push_back(std::move(spec));
  }
  return specs;
}

}  // namespace UTool::Mod
