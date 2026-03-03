#pragma once

#include <WString.h>
#include <map>
#include <vector>

/**
 * Multi-user authentication for the admin interface.
 *
 * Passwords are stored as SHA-256(salt + password) in /auth.json on LittleFS.
 * Legacy unsalted entries are upgraded transparently on next successful login.
 * Default account on first boot: "admin"/"admin".
 *
 * Brute-force lockout after MAX_FAILED_LOGINS consecutive failures.
 * Sessions are RAM-only (lost on reboot), 24 h lifetime.
 */
class AuthManager {
 public:
  static constexpr const char* CONFIG_PATH      = "/auth.json";
  static constexpr const char* DEFAULT_USERNAME = "admin";
  static constexpr const char* DEFAULT_PASSWORD = "admin";

  struct UserRecord {
    String hash;  // SHA-256 hex; bare SHA-256 if salt is empty (legacy)
    String salt;  // empty = legacy unsalted entry
  };

  static String generateSalt();
  static String hashPassword(const String& salt, const String& password);

  bool begin();

  // Not const - may upgrade legacy unsalted hash on successful login.
  bool checkCredentials(const String& username, const String& password);

  String createSession(const String& username);
  bool validateSession(const String& token) const;
  String getSessionUser(const String& token) const;
  void destroySession(const String& token);
  std::vector<String> listUsers() const;
  bool addUser(const String& username, const String& password);
  bool removeUser(const String& username);

  // Does NOT verify the old password - callers must do that beforehand.
  bool changePassword(const String& username, const String& newPassword);

 private:
  static constexpr unsigned long SESSION_DURATION_MS  = 24UL * 60UL * 60UL * 1000UL;
  static constexpr size_t        MAX_SESSIONS         = 16;

  // Brute-force lockout
  static constexpr int           MAX_FAILED_LOGINS    = 5;
  static constexpr unsigned long LOCKOUT_DURATION_MS  = 5UL * 60UL * 1000UL;
  static constexpr size_t        MAX_TRACKED_IPS      = 32;  // cap to prevent RAM exhaustion

  struct SessionInfo {
    unsigned long createdAt;
    String        username;
  };

  struct FailInfo {
    int           count;
    unsigned long firstFailAt;
  };

  std::map<String, UserRecord>  _users;
  std::map<String, SessionInfo> _sessions;
  std::map<String, FailInfo>    _failedLogins;

  bool   save() const;
  static String sha256hex(const String& input);
  void   trackFailure(const String& username);
  void   pruneExpiredLockouts();
  void   pruneExpiredSessions();
};
