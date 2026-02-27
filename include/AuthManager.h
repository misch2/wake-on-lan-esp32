#pragma once

#include <WString.h>
#include <map>

/**
 * Single-user authentication for the admin interface.
 *
 * The password is stored as a SHA-256 hex digest in /auth.json on LittleFS
 * (which must already be mounted before calling begin()).
 * On first boot, the default password "admin" is written automatically.
 *
 * Sessions are kept entirely in RAM; they are invalidated on reboot.
 * Each session is a 128-bit random token sent as the HttpOnly cookie "wol_session".
 * Session lifetime: 24 h (renewed on each login, not on each request).
 */
class AuthManager {
 public:
  static constexpr const char* CONFIG_PATH      = "/auth.json";
  static constexpr const char* DEFAULT_PASSWORD = "admin";

  /**
   * Load the stored password hash from LittleFS.
   * Writes the default hash if /auth.json does not exist.
   * Safe to call after DeviceManager::begin() (which mounts LittleFS).
   */
  bool begin();

  /** Returns true when `password` matches the stored hash. */
  bool checkPassword(const String& password) const;

  /**
   * Hash and persist `newPassword`.
   * Returns false on I/O error; the in-memory hash is updated regardless.
   */
  bool setPassword(const String& newPassword);

  /**
   * Mint a new 32-char hex session token, store it, and return it.
   * Expired tokens are pruned automatically before issuing a new one.
   */
  String createSession();

  /** Returns true when `token` is known and has not expired. */
  bool validateSession(const String& token) const;

  /** Remove `token` from the active-session map (logout). */
  void destroySession(const String& token);

 private:
  static constexpr unsigned long SESSION_DURATION_MS = 24UL * 60UL * 60UL * 1000UL;
  static constexpr size_t        MAX_SESSIONS        = 8;

  String _hash;                             ///< SHA-256 hex of current password
  std::map<String, unsigned long> _sessions;  ///< token → millis() at creation

  static String sha256hex(const String& input);
  void pruneExpiredSessions();
};
