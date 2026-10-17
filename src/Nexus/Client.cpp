#include "UTool/Nexus/Client.hpp"

#include "UTool/Lua/Host.hpp"

#include <cctype>
#include <chrono>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

#include <nlohmann/json.hpp>

namespace UTool::Nexus {
namespace {

#ifdef _WIN32

std::wstring widen(std::string_view s) {
  if (s.empty())
    return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  std::wstring out(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
  return out;
}

std::string narrow(std::wstring_view s) {
  if (s.empty())
    return {};
  const int n =
      WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
  std::string out(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr, nullptr);
  return out;
}

struct UrlParts {
  bool https = true;
  std::wstring host;
  INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
  std::wstring path;
};

UrlParts parseUrl(const std::string& url) {
  URL_COMPONENTSW uc{};
  uc.dwStructSize = sizeof(uc);
  wchar_t host[256]{};
  wchar_t path[2048]{};
  uc.lpszHostName = host;
  uc.dwHostNameLength = 256;
  uc.lpszUrlPath = path;
  uc.dwUrlPathLength = 2048;
  const auto wurl = widen(url);
  if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc))
    throw std::runtime_error("Invalid URL: " + url);
  UrlParts p;
  p.https = uc.nScheme == INTERNET_SCHEME_HTTPS;
  p.host.assign(host, uc.dwHostNameLength);
  p.port = uc.nPort;
  p.path.assign(path, uc.dwUrlPathLength);
  return p;
}

struct HttpResponse {
  DWORD status = 0;
  std::string body;
  std::string etag;
};

HttpResponse httpRequest(
    const std::string& url,
    const std::wstring& method,
    const std::string& body,
    const std::vector<std::pair<std::wstring, std::wstring>>& headers,
    bool binaryBody = false) {
  const auto parts = parseUrl(url);
  HINTERNET session = WinHttpOpen(L"UTool/0.2", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session)
    throw std::runtime_error("WinHttpOpen failed");

  HINTERNET connect = WinHttpConnect(session, parts.host.c_str(), parts.port, 0);
  if (!connect) {
    WinHttpCloseHandle(session);
    throw std::runtime_error("WinHttpConnect failed");
  }

  DWORD flags = parts.https ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET request =
      WinHttpOpenRequest(connect, method.c_str(), parts.path.c_str(), nullptr, WINHTTP_NO_REFERER,
                         WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if (!request) {
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    throw std::runtime_error("WinHttpOpenRequest failed");
  }

  for (const auto& [name, value] : headers) {
    const std::wstring line = name + L": " + value;
    WinHttpAddRequestHeaders(request, line.c_str(), static_cast<DWORD>(-1),
                             WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
  }

  const void* bodyPtr = body.empty() ? WINHTTP_NO_REQUEST_DATA : body.data();
  const DWORD bodyLen = static_cast<DWORD>(body.size());
  if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, const_cast<void*>(bodyPtr),
                          bodyLen, bodyLen, 0)) {
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    throw std::runtime_error("WinHttpSendRequest failed for " + url);
  }

  if (!WinHttpReceiveResponse(request, nullptr)) {
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    throw std::runtime_error("WinHttpReceiveResponse failed for " + url);
  }

  HttpResponse resp;
  DWORD statusSize = sizeof(resp.status);
  WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &resp.status, &statusSize,
                      WINHTTP_NO_HEADER_INDEX);

  wchar_t etagBuf[256]{};
  DWORD etagSize = sizeof(etagBuf);
  if (WinHttpQueryHeaders(request, WINHTTP_QUERY_ETAG, WINHTTP_HEADER_NAME_BY_INDEX, etagBuf,
                          &etagSize, WINHTTP_NO_HEADER_INDEX)) {
    resp.etag = narrow(etagBuf);
    if (!resp.etag.empty() && resp.etag.front() == '"')
      resp.etag.erase(resp.etag.begin());
    if (!resp.etag.empty() && resp.etag.back() == '"')
      resp.etag.pop_back();
  }

  (void)binaryBody;
  std::string collected;
  for (;;) {
    DWORD avail = 0;
    if (!WinHttpQueryDataAvailable(request, &avail))
      break;
    if (avail == 0)
      break;
    std::string chunk(avail, '\0');
    DWORD read = 0;
    if (!WinHttpReadData(request, chunk.data(), avail, &read))
      break;
    chunk.resize(read);
    collected += chunk;
  }
  resp.body = std::move(collected);

  WinHttpCloseHandle(request);
  WinHttpCloseHandle(connect);
  WinHttpCloseHandle(session);
  return resp;
}

HttpResponse apiJson(
    const std::string& apiBase,
    const std::string& path,
    const std::wstring& method,
    const std::string& apiKey,
    const nlohmann::json& body = nullptr) {
  std::string url = apiBase;
  if (!url.empty() && url.back() == '/')
    url.pop_back();
  url += path;
  std::vector<std::pair<std::wstring, std::wstring>> headers = {
      {L"apikey", widen(apiKey)},
      {L"User-Agent", L"UTool/0.2"},
      {L"Accept", L"application/json"},
  };
  std::string bodyStr;
  if (!body.is_null()) {
    bodyStr = body.dump();
    headers.emplace_back(L"Content-Type", L"application/json");
  }
  return httpRequest(url, method, bodyStr, headers);
}

std::string buildCompleteXml(const std::vector<std::pair<int, std::string>>& parts) {
  std::ostringstream oss;
  oss << "<CompleteMultipartUpload>\n";
  for (const auto& [num, etag] : parts) {
    oss << "  <Part>\n    <PartNumber>" << num << "</PartNumber>\n    <ETag>" << etag
        << "</ETag>\n  </Part>\n";
  }
  oss << "</CompleteMultipartUpload>";
  return oss.str();
}

UploadResult uploadWindows(const UploadRequest& request, const std::string& apiKey,
                           const std::string& apiBase) {
  UploadResult result;
  try {
    if (!std::filesystem::is_regular_file(request.filePath))
      throw std::runtime_error("File not found: " + request.filePath.string());
    const auto fileSize = std::filesystem::file_size(request.filePath);
    const std::string filename = request.filePath.filename().string();

    // 1) Create multipart upload
    nlohmann::json createBody = {
        {"filename", filename},
        {"size_bytes", std::to_string(fileSize)},
    };
    auto created = apiJson(apiBase, "/uploads/multipart", L"POST", apiKey, createBody);
    if (created.status < 200 || created.status >= 300)
      throw std::runtime_error("create multipart failed: " + std::to_string(created.status) + " " +
                               created.body);
    auto createJson = nlohmann::json::parse(created.body);
    const auto& data = createJson.at("data");
    const std::string uploadId = data.at("id").get<std::string>();
    const auto partSize = data.at("part_size_bytes").get<std::uint64_t>();
    const auto completeUrl = data.at("complete_presigned_url").get<std::string>();
    std::vector<std::string> partUrls;
    for (const auto& u : data.at("part_presigned_urls"))
      partUrls.push_back(u.get<std::string>());

    // 2) Upload parts
    std::ifstream file(request.filePath, std::ios::binary);
    if (!file)
      throw std::runtime_error("Cannot open " + request.filePath.string());
    std::vector<std::pair<int, std::string>> etags;
    for (std::size_t i = 0; i < partUrls.size(); ++i) {
      std::vector<char> buf(static_cast<size_t>(partSize));
      file.read(buf.data(), static_cast<std::streamsize>(partSize));
      const auto got = static_cast<size_t>(file.gcount());
      buf.resize(got);
      std::string body(buf.begin(), buf.end());
      auto partResp = httpRequest(partUrls[i], L"PUT", body,
                                  {{L"Content-Type", L"application/octet-stream"}}, true);
      if (partResp.status < 200 || partResp.status >= 300)
        throw std::runtime_error("part upload " + std::to_string(i + 1) +
                                 " failed: " + std::to_string(partResp.status));
      if (partResp.etag.empty())
        throw std::runtime_error("part upload " + std::to_string(i + 1) + " missing ETag");
      etags.emplace_back(static_cast<int>(i + 1), partResp.etag);
    }

    // 3) Complete multipart
    const auto xml = buildCompleteXml(etags);
    auto completeResp =
        httpRequest(completeUrl, L"POST", xml, {{L"Content-Type", L"application/xml"}});
    if (completeResp.status < 200 || completeResp.status >= 300)
      throw std::runtime_error("complete multipart failed: " + std::to_string(completeResp.status) +
                               " " + completeResp.body);

    // 4) Finalise
    auto finalise =
        apiJson(apiBase, "/uploads/" + uploadId + "/finalise", L"POST", apiKey, nlohmann::json::object());
    if (finalise.status < 200 || finalise.status >= 300)
      throw std::runtime_error("finalise failed: " + std::to_string(finalise.status) + " " +
                               finalise.body);

    // 5) Poll until available
    for (int attempt = 0; attempt < 60; ++attempt) {
      auto state = apiJson(apiBase, "/uploads/" + uploadId, L"GET", apiKey);
      if (state.status < 200 || state.status >= 300)
        throw std::runtime_error("poll upload failed: " + std::to_string(state.status));
      auto sj = nlohmann::json::parse(state.body);
      const std::string st = sj.at("data").at("state").get<std::string>();
      if (st == "available")
        break;
      if (attempt == 59)
        throw std::runtime_error("upload not available after polling");
      const auto delayMs = static_cast<int>(std::min(30000.0, 2000.0 * std::pow(1.5, attempt)));
      std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }

    // 6) Create mod file version
    nlohmann::json versionBody = {
        {"upload_id", uploadId},
        {"name", request.displayName},
        {"version", request.version},
        {"file_category", request.category},
        {"archive_existing_file", request.archiveExisting},
        {"update_mod_version", request.updateModVersion},
    };
    if (request.description)
      versionBody["description"] = *request.description;

    auto versionResp = apiJson(apiBase, "/mod-files/" + std::to_string(request.fileId) + "/versions",
                               L"POST", apiKey, versionBody);
    if (versionResp.status < 200 || versionResp.status >= 300)
      throw std::runtime_error("createModFileVersion failed: " + std::to_string(versionResp.status) +
                               " " + versionResp.body);

    auto vj = nlohmann::json::parse(versionResp.body);
    if (vj.contains("data") && vj["data"].contains("version") && vj["data"]["version"].contains("id"))
      result.versionId = vj["data"]["version"]["id"].get<std::string>();

    if (request.changelog && request.modId) {
      nlohmann::json clog = {{"version", request.version}, {"changelog", *request.changelog}};
      auto clogResp =
          apiJson(apiBase, "/mods/" + std::to_string(*request.modId) + "/changelogs", L"POST",
                  apiKey, clog);
      if (clogResp.status < 200 || clogResp.status >= 300)
        throw std::runtime_error("changelog failed: " + std::to_string(clogResp.status) + " " +
                                 clogResp.body);
    }

    result.ok = true;
    result.message = "ok";
  } catch (const std::exception& ex) {
    result.ok = false;
    result.message = ex.what();
  }
  return result;
}

std::string toLowerCopy(std::string s) {
  for (char& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

/// game-scoped mod URL id (e.g. 213) -> v3 mods.id (e.g. 18627273162965)
std::string resolveV3ModId(const std::string& gameDomain, std::int64_t gameScopedModId,
                           const std::string& apiKey) {
  const std::string path =
      "/games/" + gameDomain + "/mods/" + std::to_string(gameScopedModId);
  auto resp = apiJson("https://api.nexusmods.com/v3", path, L"GET", apiKey);
  if (resp.status < 200 || resp.status >= 300)
    throw std::runtime_error("resolve v3 mod id failed: " + std::to_string(resp.status) + " " +
                             resp.body);
  auto j = nlohmann::json::parse(resp.body);
  if (!j.contains("data") || !j["data"].contains("id"))
    throw std::runtime_error("v3 mod response missing data.id");
  if (j["data"]["id"].is_string())
    return j["data"]["id"].get<std::string>();
  if (j["data"]["id"].is_number_integer())
    return std::to_string(j["data"]["id"].get<std::int64_t>());
  throw std::runtime_error("v3 mod id has unexpected type");
}

std::int64_t parseIdField(const nlohmann::json& v) {
  if (v.is_string()) {
    const auto s = v.get<std::string>();
    return std::stoll(s);
  }
  if (v.is_number_integer())
    return v.get<std::int64_t>();
  return 0;
}

// v3 file id (upload API), not v1 game-scoped file_id.
ListFilesResult listModFilesWindows(const std::string& gameDomain, std::int64_t modId,
                                    const std::string& apiKey) {
  ListFilesResult result;
  try {
    const std::string v3ModId = resolveV3ModId(gameDomain, modId, apiKey);
    auto listResp =
        apiJson("https://api.nexusmods.com/v3", "/mods/" + v3ModId + "/files", L"GET", apiKey);
    if (listResp.status < 200 || listResp.status >= 300)
      throw std::runtime_error("list v3 files failed: " + std::to_string(listResp.status) + " " +
                               listResp.body);
    auto j = nlohmann::json::parse(listResp.body);
    if (!j.contains("data") || !j["data"].contains("mod_files") ||
        !j["data"]["mod_files"].is_array())
      throw std::runtime_error("unexpected v3 files shape");

    for (const auto& f : j["data"]["mod_files"]) {
      const bool active = f.value("is_active", false);
      if (!active)
        continue;
      const auto fileId = parseIdField(f.at("id"));
      if (fileId <= 0)
        continue;

      std::string matchName = f.value("name", "");
      std::string category = "main";
      std::string version;

      auto verResp = apiJson("https://api.nexusmods.com/v3",
                             "/mod-files/" + std::to_string(fileId) + "/versions", L"GET", apiKey);
      if (verResp.status >= 200 && verResp.status < 300) {
        auto vj = nlohmann::json::parse(verResp.body);
        if (vj.contains("data") && vj["data"].contains("versions") &&
            vj["data"]["versions"].is_array()) {
          const nlohmann::json* best = nullptr;
          for (const auto& ver : vj["data"]["versions"]) {
            const auto cat = toLowerCopy(ver.value("category", ""));
            if (ver.value("is_primary", false) || cat == "main") {
              best = &ver;
              break;
            }
            if (!best)
              best = &ver;
          }
          if (best) {
            matchName = best->value("name", matchName);
            category = best->value("category", category);
            version = best->value("version", "");
          }
        }
      }

      RemoteModFile rf;
      rf.fileId = fileId;
      rf.name = std::move(matchName);
      rf.category = std::move(category);
      rf.version = std::move(version);
      if (!rf.name.empty())
        result.files.push_back(std::move(rf));
    }
    result.ok = true;
    result.message = "ok";
  } catch (const std::exception& ex) {
    result.ok = false;
    result.message = ex.what();
  }
  return result;
}

std::vector<std::int64_t> findModsWindows(const std::string& gameDomain, const std::string& author,
                                          const std::string& apiKey) {
  std::vector<std::int64_t> out;
  const std::string authorKey = toLowerCopy(author);
  std::set<std::int64_t> seen;
  for (const char* period : {"1d", "1w", "1m"}) {
    const std::string path =
        std::string("/v1/games/") + gameDomain + "/mods/updated.json?period=" + period;
    auto resp = apiJson("https://api.nexusmods.com", path, L"GET", apiKey);
    if (resp.status < 200 || resp.status >= 300)
      continue;
    auto arr = nlohmann::json::parse(resp.body);
    if (!arr.is_array())
      continue;
    for (const auto& m : arr) {
      if (!m.contains("mod_id") || !m["mod_id"].is_number_integer())
        continue;
      const auto modId = m["mod_id"].get<std::int64_t>();
      if (!seen.insert(modId).second)
        continue;
      auto infoResp = apiJson("https://api.nexusmods.com",
                              "/v1/games/" + gameDomain + "/mods/" + std::to_string(modId) + ".json",
                              L"GET", apiKey);
      if (infoResp.status < 200 || infoResp.status >= 300)
        continue;
      auto info = nlohmann::json::parse(infoResp.body);
      const std::string uploaded = toLowerCopy(info.value("uploaded_by", ""));
      const std::string auth = toLowerCopy(info.value("author", ""));
      if (uploaded != authorKey && auth != authorKey)
        continue;
      out.push_back(modId);
    }
  }
  return out;
}

std::optional<std::int64_t> findModIdWindows(const std::string& gameDomain,
                                             const std::string& author,
                                             const std::string& apiKey,
                                             const std::string& nameHint) {
  auto normalize = [](std::string_view name) {
    std::string s = toLowerCopy(std::string(name));
    std::string out;
    for (char c : s) {
      if (std::isalnum(static_cast<unsigned char>(c)))
        out.push_back(c);
    }
    return out;
  };
  const std::string authorKey = toLowerCopy(author);
  const std::string hintKey = normalize(nameHint);
  std::optional<std::int64_t> bestId;
  int bestScore = 0;
  for (const auto modId : findModsWindows(gameDomain, author, apiKey)) {
    auto infoResp = apiJson("https://api.nexusmods.com",
                            "/v1/games/" + gameDomain + "/mods/" + std::to_string(modId) + ".json",
                            L"GET", apiKey);
    if (infoResp.status < 200 || infoResp.status >= 300)
      continue;
    auto info = nlohmann::json::parse(infoResp.body);
    int score = 10;
    const std::string name = info.value("name", "");
    const auto nk = normalize(name);
    if (!hintKey.empty() &&
        (nk.find(hintKey) != std::string::npos || hintKey.find(nk) != std::string::npos))
      score = 50;
    else if (nk.find("levelcap") != std::string::npos || nk.find("level250") != std::string::npos)
      score = 40;
    if (score > bestScore) {
      bestScore = score;
      bestId = modId;
    }
  }
  (void)authorKey;
  return bestId;
}

ValidateResult validateApiKeyWindows(const std::string& apiKey) {
  ValidateResult result;
  try {
    auto resp = apiJson("https://api.nexusmods.com", "/v1/users/validate.json", L"GET", apiKey);
    if (resp.status < 200 || resp.status >= 300)
      throw std::runtime_error("validate failed: " + std::to_string(resp.status) + " " + resp.body);
    auto j = nlohmann::json::parse(resp.body);
    result.name = j.value("name", "");
    if (j.contains("user_id") && j["user_id"].is_number_integer())
      result.userId = j["user_id"].get<std::int64_t>();
    result.ok = !result.name.empty();
    result.message = result.ok ? "ok" : "validate returned empty name";
  } catch (const std::exception& ex) {
    result.ok = false;
    result.message = ex.what();
  }
  return result;
}

#endif  // _WIN32

}  // namespace

UploadResult uploadModFileVersion(
    const UploadRequest& request,
    const std::string& apiKey,
    const std::string& apiBase) {
#ifdef _WIN32
  return uploadWindows(request, apiKey, apiBase);
#else
  (void)request;
  (void)apiKey;
  (void)apiBase;
  return {false, "Nexus upload is Windows-only in this build", std::nullopt};
#endif
}

UploadResult uploadFromManifestFile(
    const Core::NexusFileEntry& entry,
    const std::filesystem::path& zipPath,
    const std::string& updateVersion,
    const std::string& apiKey,
    const Core::NexusSettings& settings) {
  UploadRequest req;
  req.filePath = zipPath;
  req.fileId = entry.fileId;
  req.version = updateVersion;
  req.category = entry.category.empty() ? "main" : entry.category;
  req.archiveExisting = entry.archiveExisting;
  req.updateModVersion = entry.updateModVersion;
  req.modId = entry.modId ? entry.modId : settings.modId;
  std::string display = entry.displayName.value_or(zipPath.filename().string());
  display = Lua::expandOutputTemplate(display, std::optional<std::string>{updateVersion},
                                      std::nullopt);
  // Nexus name pattern: ^[a-zA-Z0-9 _'().-]+$
  {
    std::string cleaned;
    cleaned.reserve(display.size());
    for (unsigned char c : display) {
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
          c == ' ' || c == '_' || c == '\'' || c == '(' || c == ')' || c == '.' || c == '-')
        cleaned.push_back(static_cast<char>(c));
      else if (c == '%')
        cleaned.append("pct");
      else
        cleaned.push_back('-');
    }
    display = std::move(cleaned);
  }
  req.displayName = display;
  const std::string base =
      settings.apiBase.value_or("https://api.nexusmods.com/v3");
  return uploadModFileVersion(req, apiKey, base);
}

namespace {

std::string toLowerCopyPublic(std::string s) {
  for (char& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string normalizeFileKey(std::string_view name) {
  std::string s = toLowerCopyPublic(std::string(name));
  if (s.size() > 4 && s.substr(s.size() - 4) == ".zip")
    s.resize(s.size() - 4);

  // Strip "week" + optional separators + up to 4 digits BEFORE dropping
  // non-alnum, so "week 247 1.5x" does not become "week24715" and eat the tier.
  for (;;) {
    const auto w = s.find("week");
    if (w == std::string::npos)
      break;
    std::size_t i = w + 4;
    while (i < s.size() &&
           (s[i] == '-' || s[i] == '_' || s[i] == ' ' || s[i] == '.'))
      ++i;
    const std::size_t digitStart = i;
    while (i < s.size() && (i - digitStart) < 4 &&
           std::isdigit(static_cast<unsigned char>(s[i])))
      ++i;
    s.erase(w, i - w);
  }

  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (std::isalnum(static_cast<unsigned char>(c)))
      out.push_back(c);
  }
  return out;
}

bool isMainCategory(std::string_view cat) {
  const auto c = toLowerCopyPublic(std::string(cat));
  return c == "main" || c.empty();
}

std::string wantKeyFromEntry(const Core::NexusFileEntry& entry) {
  std::string src = entry.displayName.value_or(entry.match);
  auto eraseAll = [&](std::string_view tok) {
    for (;;) {
      const auto p = src.find(tok);
      if (p == std::string::npos)
        break;
      src.erase(p, tok.size());
    }
  };
  eraseAll("%updateversion%");
  eraseAll("%d");
  // Drop Week token so tier numbers (1.5x, 10%, ...) are not eaten as week ids.
  eraseAll("-Week-");
  eraseAll("_Week_");
  eraseAll("Week-");
  eraseAll("Week ");
  eraseAll("week-");
  eraseAll("week ");
  for (char& c : src) {
    if (c == '*')
      c = ' ';
  }
  return normalizeFileKey(src);
}

int matchScore(const std::string& wantKey, const RemoteModFile& remote) {
  const auto got = normalizeFileKey(remote.name);
  if (wantKey.empty() || got.empty())
    return 0;

  const bool wantMounts = wantKey.find("mount") != std::string::npos;
  const bool gotMounts = got.find("mount") != std::string::npos;
  if (wantMounts != gotMounts)
    return 0;

  const bool want500 = wantKey.find("500") != std::string::npos;
  const bool got500 = got.find("500") != std::string::npos;
  if (want500 != got500)
    return 0;

  const bool want250 = wantKey.find("250") != std::string::npos;
  const bool got250 = got.find("250") != std::string::npos;
  if (want250 != got250)
    return 0;

  if (wantKey == got)
    return isMainCategory(remote.category) ? 100 : 80;

  // Tight containment only (avoid 5% matching 15%, or shared prefix noise).
  if (wantKey.size() >= 8 && got.size() >= 8) {
    const auto lo = std::min(wantKey.size(), got.size());
    const auto hi = std::max(wantKey.size(), got.size());
    if (hi <= lo + 1 &&
        (got.find(wantKey) != std::string::npos || wantKey.find(got) != std::string::npos))
      return isMainCategory(remote.category) ? 70 : 50;
  }
  return 0;
}

}  // namespace

ListFilesResult listModFiles(const std::string& gameDomain, std::int64_t modId,
                             const std::string& apiKey) {
#ifdef _WIN32
  return listModFilesWindows(gameDomain, modId, apiKey);
#else
  (void)gameDomain;
  (void)modId;
  (void)apiKey;
  return {false, "listModFiles is Windows-only in this build", {}};
#endif
}

ValidateResult validateApiKey(const std::string& apiKey) {
#ifdef _WIN32
  return validateApiKeyWindows(apiKey);
#else
  (void)apiKey;
  return {false, "validateApiKey is Windows-only in this build", {}, 0};
#endif
}

std::optional<std::int64_t> findModIdForAuthor(const std::string& gameDomain,
                                               const std::string& author,
                                               const std::string& apiKey,
                                               const std::string& nameHint) {
#ifdef _WIN32
  return findModIdWindows(gameDomain, author, apiKey, nameHint);
#else
  (void)gameDomain;
  (void)author;
  (void)apiKey;
  (void)nameHint;
  return std::nullopt;
#endif
}

std::vector<std::int64_t> findModsForAuthor(const std::string& gameDomain, const std::string& author,
                                            const std::string& apiKey) {
#ifdef _WIN32
  return findModsWindows(gameDomain, author, apiKey);
#else
  (void)gameDomain;
  (void)author;
  (void)apiKey;
  return {};
#endif
}

ResolveIdsResult resolveManifestFileIds(Core::NexusSettings& settings, const std::string& apiKey,
                                        const std::string& authorHint) {
  ResolveIdsResult result;
  try {
    const std::string domain = settings.gameDomain.value_or("icarus");

    struct IndexedRemote {
      std::int64_t modId = 0;
      RemoteModFile file;
    };
    std::vector<IndexedRemote> remotes;
    std::set<std::int64_t> modsToScan;

    for (const auto& entry : settings.files) {
      if (entry.modId && *entry.modId > 0)
        modsToScan.insert(*entry.modId);
    }
    if (settings.modId && *settings.modId > 0)
      modsToScan.insert(*settings.modId);

    if (modsToScan.empty()) {
      if (authorHint.empty())
        throw std::runtime_error(
            "No nexus modId set - set files[].modId or ensure API key validate returns a username");
      for (const auto id : findModsForAuthor(domain, authorHint, apiKey))
        modsToScan.insert(id);
      if (modsToScan.empty())
        throw std::runtime_error("No Nexus mods found for author '" + authorHint + "'");
    }

    for (const auto modId : modsToScan) {
      auto listed = listModFiles(domain, modId, apiKey);
      if (!listed.ok)
        throw std::runtime_error("mod " + std::to_string(modId) + ": " + listed.message);
      for (auto& f : listed.files)
        remotes.push_back(IndexedRemote{modId, std::move(f)});
    }

    for (auto& entry : settings.files) {
      const auto want = wantKeyFromEntry(entry);
      const std::optional<std::int64_t> scope =
          entry.modId ? entry.modId : settings.modId;

      int best = 0;
      std::int64_t bestFileId = 0;
      std::int64_t bestModId = 0;
      std::vector<const IndexedRemote*> scoped;
      for (const auto& remote : remotes) {
        if (scope && *scope > 0 && remote.modId != *scope)
          continue;
        scoped.push_back(&remote);
        const int score = matchScore(want, remote.file);
        if (score > best) {
          best = score;
          bestFileId = remote.file.fileId;
          bestModId = remote.modId;
        }
      }

      // One file on a scoped mod: accept even if names differ.
      if (bestFileId <= 0 && scope && *scope > 0 && scoped.size() == 1) {
        bestFileId = scoped[0]->file.fileId;
        bestModId = scoped[0]->modId;
        best = 1;
      }

      if (bestFileId <= 0)
        continue;
      if (!entry.modId || *entry.modId <= 0)
        entry.modId = bestModId;
      entry.fileId = bestFileId;
    }

    result.resolved = 0;
    for (const auto& entry : settings.files) {
      if (entry.fileId > 0)
        ++result.resolved;
    }

    result.ok = true;
    result.modId = settings.modId.value_or(0);
    result.message = "files with ids=" + std::to_string(result.resolved) + "/" +
                     std::to_string(settings.files.size()) + " across " +
                     std::to_string(modsToScan.size()) + " mod(s)";
  } catch (const std::exception& ex) {
    result.ok = false;
    result.message = ex.what();
  }
  return result;
}

void writeConfigNexusIds(Core::Config& config, const Core::NexusSettings& settings) {
  if (config.configFilePath.empty())
    throw std::runtime_error("No utool.json to write nexus ids");
  std::ifstream in(config.configFilePath);
  if (!in)
    throw std::runtime_error("Cannot read " + config.configFilePath.string());
  nlohmann::json j;
  in >> j;
  in.close();

  if (!j.contains("nexus") || !j["nexus"].is_object())
    j["nexus"] = nlohmann::json::object();
  auto& n = j["nexus"];
  if (settings.gameDomain)
    n["gameDomain"] = *settings.gameDomain;
  if (settings.modId)
    n["modId"] = *settings.modId;
  else
    n.erase("modId");
  if (!n.contains("apiKeyEnv"))
    n["apiKeyEnv"] = settings.apiKeyEnv;
  n.erase("mods");
  nlohmann::json files = nlohmann::json::array();
  for (const auto& f : settings.files) {
    nlohmann::json e;
    e["id"] = f.id;
    if (f.modId)
      e["modId"] = *f.modId;
    e["fileId"] = f.fileId;
    e["match"] = f.match;
    if (f.displayName)
      e["displayName"] = *f.displayName;
    e["category"] = f.category;
    e["archiveExisting"] = f.archiveExisting;
    if (f.updateModVersion)
      e["updateModVersion"] = true;
    files.push_back(std::move(e));
  }
  n["files"] = std::move(files);

  std::ofstream out(config.configFilePath);
  if (!out)
    throw std::runtime_error("Cannot write " + config.configFilePath.string());
  out << j.dump(2) << '\n';
  config.nexus = settings;
}

}  // namespace UTool::Nexus
