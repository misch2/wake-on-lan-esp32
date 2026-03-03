#pragma once

#include <WString.h>
#include <map>
#include <vector>

/**
 * Multi-user authentication for the admin interface.
 *
 * Credentials are stored in /auth.json on LittleFS as SHA-256(salt + password),
 * where each user's salt is a 128-bit (32 hex char) random value generated at
 * account-creation time.  This prevents rainbow-table attacks if the flash is
 * dumped.  Legacy unsalted entries (from prior firmware versions) are migrated
 * to salted hashes transparently on the user's next successful login.
 *
 * On first boot a default "admin"/"admin" account is created automatically.
 *
 * Login brute-force protection: after MAX_FAILED_LOGINS consecutive failures
 * the account is locked for LOCKOUT_DURATION_MS milliseconds.
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
   * Stored credentials.  `salt` is a 32-char lowercase hex string.
   * An empty `salt` signals a legacy unsalted SHA-256 entry that will be
   * upgraded to the salted format on next successful login.
   * Declared public so that the translation-unit helper in AuthManager.cpp can use it.
   */
  struct UserRecord {
    String hash;  ///< SHA-256 hex of (salt + password), or bare SHA-256 if salt is empty
    String salt;  ///< 128-bit random value (32 hex chars); empty = legacy unsalted
  };

  /** Generate a 128-bit (32 hex char) random salt using the ESP32 hardware RNG. */
  static String generateSalt();
  /** SHA-256(salt + password) → lowercase hex, or SHA-256(password) when salt is empty. */
  static String hashPassword(const String& salt, const String& password);

  /**
   * Load stored credentials from LittleFS.
   * Creates a default admin account if /auth.json does not exist.
   * Migrates legacy single-hash format automatically.
   * Safe to call after DeviceManager::begin() (which mounts LittleFS).
   */
  bool begin();

  /**
   * Returns true when `username` + `password` match a stored account.
   * Tracks failed attempts and locks out the account after too many failures.
   * Automatically upgrades legacy unsalted SHA-256 hashes to salted ones on
   * first successful login.
   * NOTE: not const because it may update the hash store on legacy upgrade.
   */
  bool checkCredentials(const String& username, const String& password);

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
  static constexpr unsigned long SESSION_DURATION_MS  = 24UL * 60UL * 60UL * 1000UL;
  static constexpr size_t        MAX_SESSIONS         = 16;

  // Brute-force lockout
  static constexpr int           MAX_FAILED_LOGINS    = 5;
  static constexpr unsigned long LOCKOUT_DURATION_MS  = 5UL * 60UL * 1000UL;  ///< 5 min
  static constexpr size_t        MAX_TRACKED_IPS      = 32;  ///< cap on _failedLogins entries (RAM bound)

  struct SessionInfo {
    unsigned long createdAt;
    String        username;
  };

  struct FailInfo {
    int           count;       ///< consecutive failed attempts
    unsigned long firstFailAt; ///< millis() when the first failure occurred
  };

  std::map<String, UserRecord>  _users;        ///< username → credentials
  std::map<String, SessionInfo> _sessions;     ///< token → session info
  std::map<String, FailInfo>    _failedLogins; ///< username → failure tracking

  bool   save() const;
  static String sha256hex(const String& input);
  void   trackFailure(const String& username);
  void   pruneExpiredLockouts();
  void   pruneExpiredSessions();
};
