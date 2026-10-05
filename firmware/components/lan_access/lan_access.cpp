#include "lan_access.hpp"

LanAccess::Result LanAccess::check(std::string_view client, std::string_view token, std::string_view session,
                                   const std::function<std::string()>& makeSession) {
    if (closed_) {
        return {Verdict::deny, {}};
    }
    bool rightToken = !token.empty() && token == token_;
    if (boundClient_.empty()) {
        if (!rightToken) {
            return {Verdict::deny, {}};
        }
        boundClient_ = std::string(client);
        session_ = makeSession();
        return {Verdict::allow, session_, true};
    }
    bool rightSession = !session.empty() && session == session_;
    if (client == boundClient_) {
        if (rightSession) {
            return {Verdict::allow, {}};
        }
        // The same phone again without the cookie -- e.g. moving from the
        // camera's preview to Safari -- may rescan the QR code.
        return rightToken ? Result{Verdict::allow, session_} : Result{Verdict::deny, {}};
    }
    if (rightToken || rightSession) {
        closed_ = true;
        return {Verdict::intruder, {}};
    }
    return {Verdict::deny, {}};
}

std::string queryParam(std::string_view uri, std::string_view name) {
    size_t query = uri.find('?');
    if (query == std::string_view::npos) {
        return {};
    }
    std::string_view rest = uri.substr(query + 1);
    while (!rest.empty()) {
        size_t amp = rest.find('&');
        std::string_view pair = rest.substr(0, amp);
        rest = amp == std::string_view::npos ? std::string_view() : rest.substr(amp + 1);
        size_t eq = pair.find('=');
        if (eq != std::string_view::npos && pair.substr(0, eq) == name) {
            return std::string(pair.substr(eq + 1));
        }
    }
    return {};
}

std::string cookieValue(std::string_view header, std::string_view name) {
    while (!header.empty()) {
        size_t semi = header.find(';');
        std::string_view pair = header.substr(0, semi);
        header = semi == std::string_view::npos ? std::string_view() : header.substr(semi + 1);
        size_t start = pair.find_first_not_of(' ');
        if (start == std::string_view::npos) {
            continue;
        }
        pair = pair.substr(start);
        size_t eq = pair.find('=');
        if (eq != std::string_view::npos && pair.substr(0, eq) == name) {
            return std::string(pair.substr(eq + 1));
        }
    }
    return {};
}
