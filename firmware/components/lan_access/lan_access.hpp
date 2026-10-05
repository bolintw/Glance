#pragma once

#include <functional>
#include <string>
#include <string_view>

// Who may use the setup pages on the home network. Anyone on that network
// can reach the device, so the pages need a secret only someone in front of
// the panel has: a token in the URL behind the QR code. The first device to
// open it is bound (by address, plus a session cookie) and becomes the only
// one let in; anyone else showing up with the token or that session means
// the secret got out. Pure C++, tested on the host.
class LanAccess {
public:
    enum class Verdict { allow, deny, intruder };
    struct Result {
        Verdict verdict;
        std::string newSession;  // non-empty: send it as the session cookie
        bool firstVisit = false;  // this request bound the device
    };

    explicit LanAccess(std::string token) : token_(std::move(token)) {}

    // One request: `client` is the requester's address, `token` the URL's
    // (empty if none), `session` the cookie's (empty if none).
    // `makeSession` makes a fresh random session id.
    Result check(std::string_view client, std::string_view token, std::string_view session,
                 const std::function<std::string()>& makeSession);

private:
    std::string token_;
    std::string boundClient_;
    std::string session_;
    bool closed_ = false;
};

// The value of `name` in the URI's query string ("/?t=abc&x=1"), or "".
std::string queryParam(std::string_view uri, std::string_view name);
// The value of cookie `name` in a Cookie header ("a=1; glance_session=xyz"), or "".
std::string cookieValue(std::string_view header, std::string_view name);
