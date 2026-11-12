// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// AzaharFrame crashreport: Send Report through sentry-native (frame_crash_send.h).

#include "core/frame_crash_send.h"

#include <array>
#include <filesystem>
#include <mutex>

#include <fmt/format.h>

#include "common/scm_rev.h"
#include "core/frame_crash_config.h"

#ifdef AZAHARFRAME_SENTRY
#include <httplib.h>
#include <sentry.h>
#endif

namespace Core::CrashReport {

std::string BuiltInDsn() {
    return AZAHARFRAME_SENTRY_DSN_VALUE;
}

bool CanSend() {
#ifdef AZAHARFRAME_SENTRY
    return !BuiltInDsn().empty();
#else
    return false;
#endif
}

std::string ReleaseName() {
    const std::string version = Common::g_build_version;
    if (!version.empty() && version != "0") {
        return "azaharframe@" + version;
    }
    return "azaharframe@" + std::string(Common::g_scm_rev).substr(0, 9);
}

std::string Environment() {
    return AZAHARFRAME_SENTRY_ENVIRONMENT;
}

#ifdef AZAHARFRAME_SENTRY

namespace {

struct Dsn {
    std::string origin;  ///< "https://o1.ingest.sentry.io" (scheme, host, port)
    std::string path;    ///< "/api/<project>/envelope/"
    std::string key;
};

// scheme://key[:secret]@host[:port]/[prefix/]project
bool ParseDsn(const std::string& dsn, Dsn& out) {
    const std::size_t scheme_end = dsn.find("://");
    const std::size_t at = dsn.find('@');
    if (scheme_end == std::string::npos || at == std::string::npos || at < scheme_end) {
        return false;
    }
    const std::string scheme = dsn.substr(0, scheme_end);
    std::string key = dsn.substr(scheme_end + 3, at - scheme_end - 3);
    if (const std::size_t colon = key.find(':'); colon != std::string::npos) {
        key.resize(colon);
    }
    const std::size_t slash = dsn.find('/', at + 1);
    if (slash == std::string::npos || key.empty()) {
        return false;
    }
    const std::string hostport = dsn.substr(at + 1, slash - at - 1);
    std::string rest = dsn.substr(slash + 1);
    while (!rest.empty() && rest.back() == '/') {
        rest.pop_back();
    }
    const std::size_t last = rest.rfind('/');
    const std::string prefix = last == std::string::npos ? "" : "/" + rest.substr(0, last);
    const std::string project = last == std::string::npos ? rest : rest.substr(last + 1);
    if (hostport.empty() || project.empty() || (scheme != "https" && scheme != "http")) {
        return false;
    }
    out.origin = scheme + "://" + hostport;
    out.path = prefix + "/api/" + project + "/envelope/";
    out.key = key;
    return true;
}

const char* CaBundle() {
    static constexpr std::array<const char*, 4> candidates{
        "/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt",
        "/etc/ssl/cert.pem", "/etc/ssl/ca-bundle.pem"};
    for (const char* path : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) {
            return path;
        }
    }
    return nullptr;
}

struct Transport {
    Dsn dsn;
    SendResult* result = nullptr;
};

// sentry-native calls this for each envelope, on the thread that captured (no worker thread:
// the transport has no startup function). It owns the envelope.
void SendEnvelope(sentry_envelope_t* envelope, void* state) {
    auto* t = static_cast<Transport*>(state);
    std::size_t size = 0;
    char* body = sentry_envelope_serialize(envelope, &size);
    sentry_envelope_free(envelope);
    if (body == nullptr) {
        t->result->error = "Couldn't Make the Report";
        return;
    }
    const std::string payload(body, size);
    sentry_free(body);
    try {
        httplib::Client client(t->dsn.origin);
        client.set_connection_timeout(10);
        client.set_read_timeout(20);
        client.set_write_timeout(20);
        client.set_follow_location(false);
        if (t->dsn.origin.rfind("https://", 0) == 0) {
            const char* ca = CaBundle();
            if (ca == nullptr) {
                t->result->error = "No Certificate Store Found";
                return;
            }
            client.set_ca_cert_path(ca);
            client.enable_server_certificate_verification(true);
        }
        const httplib::Headers headers{
            {"X-Sentry-Auth",
             fmt::format("Sentry sentry_version=7, sentry_client=azaharframe/1, sentry_key={}",
                         t->dsn.key)},
        };
        const auto res =
            client.Post(t->dsn.path, headers, payload, "application/x-sentry-envelope");
        if (!res) {
            t->result->error = "Couldn't Connect: " + httplib::to_string(res.error());
            return;
        }
        t->result->status = res->status;
        if (res->status >= 200 && res->status < 300) {
            t->result->ok = true;
        } else if (res->status == 429) {
            t->result->error = "Too Many Reports: Try Again Later";
        } else {
            t->result->error = fmt::format("The Server Said {}", res->status);
        }
    } catch (const std::exception& e) {
        t->result->error = std::string("Couldn't Send: ") + e.what();
    }
}

sentry_value_t Str(const std::string& s) {
    return sentry_value_new_string(s.c_str());
}

std::string Hex(u64 v) {
    return fmt::format("0x{:x}", v);
}

// After Sentry has merged its scope into the event: only what the report shows stays. The
// modules of the crashed run replace the ones of this process.
sentry_value_t BeforeSend(sentry_value_t event, sentry_hint_t*, void* data) {
    const auto* r = static_cast<const Report*>(data);
    for (const char* key : {"contexts", "user", "server_name", "breadcrumbs", "modules",
                            "debug_meta", "threads", "request"}) {
        sentry_value_remove_by_key(event, key);
    }
    // No place: an empty user.geo, or Sentry derives a city from the request's address (tested
    // 2026-11-12: infer_ip alone does not stop that)
    sentry_value_t user = sentry_value_new_object();
    sentry_value_set_by_key(user, "geo", sentry_value_new_object());
    sentry_value_set_by_key(event, "user", user);
    // No IP: Sentry must not take the sender's address for the event
    // (a new sdk object: sentry-native's own is frozen)
    const sentry_value_t old_sdk = sentry_value_get_by_key(event, "sdk");
    sentry_value_t sdk = sentry_value_new_object();
    for (const char* key : {"name", "version"}) {
        const char* text = sentry_value_as_string(sentry_value_get_by_key(old_sdk, key));
        if (text != nullptr && *text != '\0') {
            sentry_value_set_by_key(sdk, key, Str(text));
        }
    }
    sentry_value_t sdk_settings = sentry_value_new_object();
    sentry_value_set_by_key(sdk_settings, "infer_ip", Str("never"));
    sentry_value_set_by_key(sdk, "settings", sdk_settings);
    sentry_value_set_by_key(event, "sdk", sdk);
    if (!r->modules.empty()) {
        sentry_value_t images = sentry_value_new_list();
        for (const Module& m : r->modules) {
            sentry_value_t image = sentry_value_new_object();
            sentry_value_set_by_key(image, "type", Str("elf"));
            sentry_value_set_by_key(image, "code_file", Str(m.path));
            sentry_value_set_by_key(image, "image_addr", Str(Hex(m.base)));
            sentry_value_set_by_key(image, "image_size",
                                    sentry_value_new_int64(static_cast<int64_t>(m.end - m.base)));
            if (!m.build_id.empty()) {
                sentry_value_set_by_key(image, "code_id", Str(m.build_id));
                sentry_value_set_by_key(image, "debug_id", Str(DebugId(m.build_id)));
            }
            sentry_value_append(images, image);
        }
        sentry_value_t meta = sentry_value_new_object();
        sentry_value_set_by_key(meta, "images", images);
        sentry_value_set_by_key(event, "debug_meta", meta);
    }
    return event;
}

std::mutex g_send_mutex;

} // namespace

SendResult Send(const Report& r, const std::string& dsn_text, const std::string& work_dir) {
    std::scoped_lock lock{g_send_mutex};
    SendResult result;
    Transport transport_state;
    transport_state.result = &result;
    if (!ParseDsn(dsn_text, transport_state.dsn)) {
        result.error = "No Place to Send To";
        return result;
    }
    const std::string db = work_dir + "/sentry-db";
    std::error_code ec;
    std::filesystem::create_directories(db, ec);

    sentry_options_t* options = sentry_options_new();
    sentry_options_set_dsn(options, dsn_text.c_str());
    const std::string release = r.release.empty() ? ReleaseName() : r.release;
    sentry_options_set_release(options, release.c_str());
    sentry_options_set_environment(options, Environment().c_str());
    sentry_options_set_database_path(options, db.c_str());
    // send-default-PII is off: sentry-native's default (its setter exists only on consoles)
    sentry_options_set_auto_session_tracking(options, 0);
    sentry_options_set_send_client_reports(options, 0);
    sentry_options_set_symbolize_stacktraces(options, 0);
    sentry_options_set_max_breadcrumbs(options, 0);
    sentry_options_set_shutdown_timeout(options, 2000);
    sentry_options_set_before_send(options, &BeforeSend, const_cast<Report*>(&r));
    sentry_transport_t* transport = sentry_transport_new(&SendEnvelope);
    sentry_transport_set_state(transport, &transport_state);
    sentry_options_set_transport(options, transport);
    if (sentry_init(options) != 0) {
        result.error = "Couldn't Start Sending";
        std::filesystem::remove_all(db, ec);
        return result;
    }

    // The report's two files, as Save Report writes them
    for (const auto& [name, text] : Files(r)) {
        sentry_attach_bytes(text.data(), text.size(), name.c_str());
    }

    sentry_value_t event = sentry_value_new_event();
    sentry_value_set_by_key(event, "level", Str("fatal"));
    sentry_value_t message = sentry_value_new_object();
    sentry_value_set_by_key(message, "formatted", Str(r.headline + ": " + r.reason));
    sentry_value_set_by_key(event, "message", message);
    sentry_value_t tags = sentry_value_new_object();
    sentry_value_set_by_key(tags, "reason", Str(std::string(KindKey(r.kind))));
    sentry_value_set_by_key(tags, "mode", Str(r.vr ? "vr" : "flat"));
    if (r.title_id != 0) {
        sentry_value_set_by_key(tags, "title_id", Str(fmt::format("{:016X}", r.title_id)));
    }
    if (!r.gpu.empty()) {
        sentry_value_set_by_key(tags, "gpu", Str(r.gpu.substr(0, 200)));
    }
    if (!r.driver.empty()) {
        sentry_value_set_by_key(tags, "driver", Str(r.driver.substr(0, 200)));
    }
    sentry_value_set_by_key(event, "tags", tags);
    sentry_value_t extra = sentry_value_new_object();
    if (!r.title.empty()) {
        sentry_value_set_by_key(extra, "game", Str(r.title));
    }
    if (!r.detail.empty()) {
        sentry_value_set_by_key(extra, "detail", Str(r.detail));
    }
    if (!r.note.empty()) {
        sentry_value_set_by_key(extra, "note", Str(r.note));
    }
    sentry_value_set_by_key(event, "extra", extra);
    if (!r.crash_line.empty()) {
        // the crash's type is the signal; its value the exception text, else the reason
        const std::size_t space = r.crash_line.find(' ');
        const std::string type = r.crash_line.substr(0, space);
        const std::string value = !r.exception.empty() ? r.exception : r.reason;
        sentry_value_t exc = sentry_value_new_exception(type.c_str(), value.c_str());
        sentry_value_t mechanism = sentry_value_new_object();
        sentry_value_set_by_key(mechanism, "type", Str("signalhandler"));
        sentry_value_set_by_key(mechanism, "handled", sentry_value_new_bool(0));
        sentry_value_set_by_key(exc, "mechanism", mechanism);
        if (!r.frames.empty()) {
            // Sentry wants the oldest call first; the record has the newest first. Each frame
            // names its module and, where the crashed run's backtrace had one, its exported
            // function (Sentry replaces it when it has the module's symbols).
            sentry_value_t frames = sentry_value_new_list();
            for (auto f = r.frames.rbegin(); f != r.frames.rend(); ++f) {
                sentry_value_t frame = sentry_value_new_object();
                sentry_value_set_by_key(frame, "instruction_addr", Str(Hex(f->addr)));
                if (f->module >= 0 && static_cast<std::size_t>(f->module) < r.modules.size()) {
                    const Module& m = r.modules[f->module];
                    sentry_value_set_by_key(frame, "package", Str(m.path));
                    sentry_value_set_by_key(frame, "image_addr", Str(Hex(m.base)));
                }
                if (!f->symbol.empty()) {
                    const std::size_t plus = f->symbol.rfind('+');
                    sentry_value_set_by_key(frame, "function",
                                            Str(f->symbol.substr(0, plus)));
                }
                sentry_value_append(frames, frame);
            }
            sentry_value_t stacktrace = sentry_value_new_object();
            sentry_value_set_by_key(stacktrace, "frames", frames);
            sentry_value_set_by_key(exc, "stacktrace", stacktrace);
        }
        sentry_event_add_exception(event, exc);
    }
    const sentry_uuid_t id = sentry_capture_event(event);
    sentry_close();
    char id_text[37] = {};
    sentry_uuid_as_string(&id, id_text);
    if (result.ok) {
        result.event_id = id_text;
    } else if (result.error.empty()) {
        result.error = "Nothing Was Sent";
    }
    std::filesystem::remove_all(db, ec);
    return result;
}

#else

SendResult Send(const Report&, const std::string&, const std::string&) {
    SendResult result;
    result.error = "Sending Isn't Built In";
    return result;
}

#endif

} // namespace Core::CrashReport
