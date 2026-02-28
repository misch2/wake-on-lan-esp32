#pragma once

#include <WString.h>
#include <map>
#include <vector>

/**
 * Multi-user authentication for the admin interface.
 *
 * Credentials are stored as SHA-256 hex digests in /auth.json on LittleFS
 * (which must already be mounted before calling begin()).
 * On first boot a default "admin"/"admin" account is created automatically.
 * Legacy single-user configs are migrated transparently.
 *
 * Sessions are kept entirely in RAM; they are invalidated on reboot.
 * Each session is a 128-bit random token sent as the HttpOnly cookie "wol_session".
 * Session lifetime: 24 h (renewed on login, not on each request).
 */
class AuthManager {
 public:
  static constexpr const char* CONFIG_PATH      = "/auth.json";
  static constexpr const char* DEFAULT_USERNAME = "admin";
  static constexpr const char* DEFAULT_PASSWORD = "admin";

  /**
   * Load stored credentials from LittleFS.
   * Creates a default admin account if /auth.json does not exist.
   * Migrates legacy single-hash format automatically.
   * Safe to call after DeviceManager::begin() (which mounts LittleFS).
   */
  bool begin();

  /** Returns true when `username` + `password` match a stored account. */
  bool checkCredentials(const String& username, const String& password) const;

  /**
   * Mint a new 32-char hex session token tied to `username`, store it, and
   * return it. Expired tokens are pruned automatically before issuing a new one.
   */
  String createSession(const String& username);

  /** Returns true when `token` is known and has not expired. */
  bool validateSession(const String& token) const;

  /** Returns the username associated with `token`, or "" if invalid/expired. */
  String getSessionUser(const String& token) const;

  /** Remove `token` from the active-session map (logout). */
  void destroySession(const String& token);

  /** Returns a sorted list of all stored usernames. */
  std::vector<String> listUsers() const;

  /**
   * Add a new admin account.
   * Returns false if `username` already exists, is empty, or on I/O error.
   */
  bool addUser(const String& username, const String& password);

  /**
   * Remove an admin account and invalidate its active sessions.
   * Returns false if it is the last account (prevents lockout) or on I/O error.
   */
  bool removeUser(const String& username);

  /**
   * Hash and persist a new password for `username`.
   * Does NOT verify the old password - callers must do that beforehand.
   * Returns false if the user is not found or on I/O error.
   */
  bool changePassword(const String& username, const String& newPassword);

 private:
  static constexpr unsigned long SESSION_DURATION_MS = 24UL * 60UL * 60UL * 1000UL;
  static constexpr size_t        MAX_SESSIONS        = 16;

  struct SessionInfo {
    unsigned long createdAt;
    String        username;
  };

  std::map<String, String>      _users;    ///< username → SHA-256 hex hash
  std::map<String, SessionInfo> _sessions; ///< token → session info

  bool save() const;
  static String sha256hex(const String& input);
  void pruneExpiredSessions();
};
