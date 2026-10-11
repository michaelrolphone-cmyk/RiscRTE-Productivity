/* Daily Paper: latest metadata -> dated SD EPUB -> existing Reader.
 * All I/O uses capabilities; the UI uses the existing NOVA components. */
#include "metadata.h"
#include "RiscNetHttpClientV1.h"
#include "RiscRuntimeV1.h"
#include "RiscSceneComponentsV1.h"
#include "RiscSceneResidentV1.h"
#include "RiscStorageVolumeFsV1.h"
#include "RiscRealtimeV1.h"
#include "T5FileOpenApi.h"
#include "PortableWifiSavedNetwork.h"
#include <stdio.h>
#include <string.h>

#define READER_COOKIE UINT64_C(0x4441494c59504150)
enum { CLOSE = 1, RETRY = 2 };
static struct {
    const risc_runtime_api_v1 *rt;
    const risc_scene_api_v1 *scene;
    const risc_scene_components_api_v1 *ui;
    const t5_file_open_api_v1 *files;
    const risc_storage_volume_api_v1_ext *volume;
    const risc_storage_volume_api_v1_fs *fs;
    const risc_realtime_api_v1 *clock;
    const wifi_api_v1 *wifi;
    const risc_key_value_v1 *credentials;
    const risc_http_client_v1 *http;
    risc_runtime_capability_v1 grants[7];
    unsigned acquired;
    risc_scene_resident_v1 resident;
    risc_components_document_v1 document;
    uint64_t session, sequence, transfer;
    risc_storage_file_t writer;
    uint32_t revision, painted, spinner, downloaded, total;
    bool terminal, done, retry, error, dirty, owns_wifi, temporary;
    char heading[40], message[72], date[11], url[DAILY_URL_MAX];
    char destination[96], partial[96], source[100];
    char metadata[DAILY_METADATA_MAX + 1];
    uint8_t chunk[RISC_HTTP_CHUNK_MAX];
} a;
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t
risc_resident_app_descriptor_v1 = {1, sizeof(risc_resident_app_descriptor_v1_t), RISC_RESIDENT_ROLE_FOREGROUND, 0};

static bool runtime_alive(void) {
    if (!a.terminal && !risc_runtime_get_api(1)) a.terminal = true;
    return !a.terminal;
}
static void retain(const char *reason) {
    if (!runtime_alive()) return;
    if (a.rt->diagnostic) a.rt->diagnostic(reason);
    a.terminal = true;
    a.rt->retain_invocation();
}
static bool alive(void) {
    if (!runtime_alive()) return false;
    if (a.fs && a.fs->state.observe(a.volume->base.context) == RISC_STORAGE_STATE_RETAINED) {
        retain("Daily Paper: SD cleanup failed; restart required"); return false;
    }
    return runtime_alive();
}
static uint32_t now(void) {
    uint32_t ms = 0;
    if (!runtime_alive()) return 0;
    if (a.rt->struct_size >= RISC_RUNTIME_MONOTONIC_V1_SIZE && a.rt->monotonic_ms && a.rt->monotonic_ms(&ms)) return ms;
    risc_runtime_health_v1 health = {.struct_size = sizeof(health)};
    return a.rt->health && a.rt->health(&health) ? health.uptime_ms : 0;
}
static void status(const char *heading, const char *message) {
    snprintf(a.heading, sizeof(a.heading), "%s", heading);
    snprintf(a.message, sizeof(a.message), "%s", message);
    a.dirty = true;
}
static bool failure(const char *heading, const char *message) {
    a.error = true;
    status(heading, message);
    if (runtime_alive() && a.rt->diagnostic) {
        char line[192]; snprintf(line, sizeof(line), "Daily Paper: %s: %s", heading, message);
        a.rt->diagnostic(line);
    }
    return false;
}
static bool storage_failure(const char *operation) {
    char detail[72] = "SD card could not be read or written.";
    if (alive() && a.volume && a.volume->base.last_error)
        a.volume->base.last_error(a.volume->base.context, detail, sizeof(detail));
    if (!detail[0]) snprintf(detail, sizeof(detail), "Check the SD card and free space.");
    return failure(operation, detail);
}
static bool scene_result(int32_t rc) {
    if (rc == RISC_SCENE_RETAINED) retain("Daily Paper: display cleanup failed; restart required");
    return runtime_alive() && rc == RISC_SCENE_OK;
}
static risc_scene_node_v1 *node(unsigned kind, const char *label, const char *text, unsigned action) {
    unsigned i = a.document.node_count++;
    risc_scene_node_v1 *n = &a.document.nodes[i];
    n->id = i + 1; n->route = 1; n->kind = kind; n->action = action;
    snprintf(n->label, sizeof(n->label), "%s", label);
    snprintf(n->text, sizeof(n->text), "%s", text);
    return n;
}
static void declare(void) {
    memset(&a.document, 0, sizeof(a.document));
    a.document.api_version = 1; a.document.struct_size = sizeof(a.document);
    a.document.revision = ++a.revision; a.document.root = 1;
    a.document.route_count = 1; a.document.routes[0].id = 1;
    a.document.routes[0].back_action = CLOSE; a.document.screen_key = a.error ? 2 : 1;
    snprintf(a.document.routes[0].title, sizeof(a.document.routes[0].title), "DAILY PAPER");
    if (a.error) {
        node(RISC_COMPONENT_EMPTY, a.heading, a.message, 0);
        node(RISC_SCENE_ACTION, "RETRY", "", RETRY)->flags = RISC_SCENE_PRIMARY;
    } else {
        risc_scene_node_v1 *n = node(RISC_COMPONENT_PROGRESS, a.heading, a.message, 0);
        n->maximum = a.total ? 100 : 0;
        n->value = a.total ? (int32_t)((a.downloaded * 100u) / a.total) : (int32_t)(a.spinner % 3u);
        if (n->value > 100 && n->maximum) n->value = 100;
    }
    if (a.date[0]) node(RISC_SCENE_TEXT_NODE, a.date, "Saved separately from other days", 0);
    node(RISC_SCENE_ACTION, a.error ? "CLOSE" : "CANCEL", "", CLOSE);
}
static bool open_scene(void) {
    declare();
    if (!scene_result(a.ui->open(a.scene->context, &a.document, NULL, &a.session))) return false;
    a.sequence = 0; a.dirty = false; a.painted = now();
    return scene_result(a.ui->lifecycle.configure(a.scene->context, a.session, 0));
}
static bool close_scene(void) {
    if (!a.session) return true;
    for (unsigned i = 0; i < 1000 && runtime_alive(); ++i) {
        int32_t rc = a.scene->close(a.scene->context, a.session);
        if (rc == RISC_SCENE_OK) { a.session = 0; return runtime_alive(); }
        if (rc != RISC_SCENE_AGAIN) { retain("Daily Paper: unable to close display; restart required"); return false; }
        a.rt->yield_ms(5);
    }
    retain("Daily Paper: display close timed out; restart required"); return false;
}
static bool pump(void) {
    if (!alive() || a.done || a.retry) return false;
    risc_scene_event_v1 e = {.struct_size = sizeof(e)};
    int32_t rc = a.scene->next(a.scene->context, a.session, &e);
    if (rc != RISC_SCENE_IDLE && !scene_result(rc)) return false;
    if (rc == RISC_SCENE_OK && e.document_revision == a.document.revision && e.sequence > a.sequence) {
        a.sequence = e.sequence;
        if (e.kind == RISC_SCENE_SUSPEND_EVENT || e.action == CLOSE) a.done = true;
        else if (a.error && e.kind == RISC_SCENE_ACTION_EVENT && e.action == RETRY &&
                 e.node && e.node <= a.document.node_count && a.document.nodes[e.node-1].action == RETRY) a.retry = true;
    }
    if (a.done || a.retry) return false;
    uint32_t ms = now();
    if (a.dirty || (!a.error && (uint32_t)(ms - a.painted) >= 600)) {
        ++a.spinner; declare();
        if (!scene_result(a.ui->update(a.scene->context, a.session, &a.document))) return false;
        a.dirty = false; a.painted = ms;
    }
    a.rt->yield_ms(5);
    return alive();
}
static bool acquire(unsigned index, const char *capability, uint64_t instance) {
    a.grants[index] = (risc_runtime_capability_v1){.struct_size = sizeof(a.grants[index])};
    if (!a.rt->acquire(capability, 1, instance, &a.grants[index]))
        return failure("SERVICE UNAVAILABLE", capability);
    a.acquired = index + 1;
    return runtime_alive();
}
static bool release_to(unsigned count) {
    while (a.acquired > count && runtime_alive()) {
        unsigned index = --a.acquired;
        if (index == 6) a.http = NULL;
        if (index == 5) a.credentials = NULL;
        if (index == 4) a.wifi = NULL;
        if (index == 3) a.clock = NULL;
        if (index == 2) { a.fs = NULL; a.volume = NULL; }
        if (!a.rt->release(&a.grants[index])) { retain("Daily Paper: service release failed; restart required"); return false; }
    }
    return runtime_alive();
}
static bool close_http(void) {
    if (!a.transfer) return alive();
    int32_t rc = a.http->close(a.http->context, a.transfer);
    if (!alive()) return false;
    if (rc != RISC_HTTP_OK) { retain("Daily Paper: HTTP cleanup failed; restart required"); return false; }
    a.transfer = 0; return true;
}
static bool cleanup_work(void) {
    if (!alive() || !close_http()) return false;
    if (a.wifi && a.owns_wifi) {
        bool ok = a.wifi->disconnect_checked(a.wifi->context);
        if (!runtime_alive()) return false;
        if (!ok) { retain("Daily Paper: Wi-Fi cleanup failed; restart required"); return false; }
        a.owns_wifi = false;
    }
    if (!alive()) return false;
    if (a.writer) {
        bool ok = a.volume->base.file_close(a.volume->base.context, a.writer, false);
        if (!alive()) return false;
        if (!ok) { retain("Daily Paper: SD file close failed; restart required"); return false; }
        a.writer = 0; a.temporary = false;
    }
    if (a.temporary && a.fs) {
        risc_storage_metadata_v1 m = {.struct_size = sizeof(m)};
        int32_t rc = a.fs->metadata(a.volume->base.context, a.partial, &m);
        if (!alive()) return false;
        if (rc == RISC_STORAGE_FS_OK && !a.volume->base.remove(a.volume->base.context, a.partial))
            storage_failure("TEMPORARY FILE CLEANUP FAILED");
        else if (rc != RISC_STORAGE_FS_OK && rc != RISC_STORAGE_FS_NOT_FOUND)
            storage_failure("TEMPORARY FILE CLEANUP FAILED");
        a.temporary = false;
    }
    return alive() && release_to(2);
}
static bool directory(const char *path) {
    risc_storage_metadata_v1 m = {.struct_size = sizeof(m)};
    int32_t rc = a.fs->metadata(a.volume->base.context, path, &m);
    if (!alive()) return false;
    if (rc == RISC_STORAGE_FS_OK) return (m.flags & RISC_STORAGE_DIRECTORY) || failure("SD FOLDER UNAVAILABLE", path);
    if (rc != RISC_STORAGE_FS_NOT_FOUND || !a.volume->mkdir(a.volume->base.context, path)) return storage_failure("CANNOT CREATE SD FOLDER");
    return alive();
}
static bool dependencies(void) {
    if (!acquire(2, "storage.volume", 0)) return false;
    const risc_storage_volume_api_v1 *volume = a.grants[2].api;
    a.volume = risc_storage_volume_extension(volume); a.fs = risc_storage_volume_fs(volume);
    if (!a.volume || !a.fs || !volume->refresh || !volume->ready || !volume->file_open_write ||
        !volume->file_write || !volume->file_close || !volume->remove || !a.volume->file_sync || !a.volume->mkdir || !a.volume->handle_error)
        return failure("SD SERVICE UNAVAILABLE", "Install the current X4 storage provider.");
    if (!volume->refresh(volume->context) || !alive() || !volume->ready(volume->context)) return storage_failure("SD CARD UNAVAILABLE");
    if (!directory("/Books") || !directory(DAILY_DIRECTORY)) return false;
    if (!acquire(3, "runtime.realtime", 0)) return false;
    a.clock = a.grants[3].api;
    if (!a.clock || a.clock->api_version != 1 || a.clock->struct_size < sizeof(*a.clock) || !a.clock->read)
        return failure("CLOCK UNAVAILABLE", "Set the date and time in Settings.");
    if (!acquire(4, "net.wifi", 0)) return false;
    a.wifi = a.grants[4].api;
    if (!portable_wifi_saved_network_api_valid(a.wifi)) return failure("WI-FI UNAVAILABLE", "Install the current Wi-Fi provider.");
    if (!acquire(5, "storage.key-value", PORTABLE_WIFI_SAVED_NETWORK_STORAGE_INSTANCE)) return false;
    a.credentials = a.grants[5].api;
    if (!acquire(6, RISC_NET_HTTP_CLIENT_CAPABILITY, 0)) return false;
    a.http = a.grants[6].api;
    if (!a.http || a.http->api_version != 1 || a.http->struct_size < sizeof(*a.http) ||
        !a.http->open || !a.http->read || !a.http->info || !a.http->close)
        return failure("DOWNLOAD SERVICE UNAVAILABLE", "Install the net-http-client provider.");
    return alive();
}
static bool connect_wifi(void) {
    status("CONNECTING WI-FI", "Using your saved network.");
    if (!pump()) return false;
    wifi_link_t state = a.wifi->status(a.wifi->context);
    if (!alive()) return false;
    if (state == WIFI_LINK_DOWN) {
        int rc = portable_wifi_saved_network_connect(a.credentials, a.wifi);
        a.owns_wifi = rc == PORTABLE_WIFI_SAVED_NETWORK_STARTED || rc == PORTABLE_WIFI_SAVED_NETWORK_CONNECT_FAILED;
        if (!alive()) return false;
        if (rc != PORTABLE_WIFI_SAVED_NETWORK_STARTED)
            return failure("WI-FI CONNECTION FAILED", "Save a network in Wi-Fi Settings, then retry.");
    }
    uint32_t start = now();
    do {
        state = a.wifi->status(a.wifi->context);
        if (!alive()) return false;
        if (state == WIFI_LINK_UP) {
            wifi_ipv4_v1 ip = {{0}, {0}, {0}}, ap = {{0}, {0}, {0}};
            bool ok = a.wifi->addresses(a.wifi->context, &ip, &ap);
            if (!alive()) return false;
            if (ok && (ip.address[0] || ip.address[1] || ip.address[2] || ip.address[3])) return true;
        } else if (state != WIFI_LINK_JOINING)
            return failure("WI-FI CONNECTION FAILED", "Check the saved network and signal, then retry.");
        if ((uint32_t)(now() - start) >= 30000)
            return failure("WI-FI TIMED OUT", "No network address after 30 seconds.");
    } while (pump());
    return false;
}
static bool http_failure(int32_t rc) {
    if (rc == RISC_HTTP_RETAINED) { retain("Daily Paper: retained HTTP failure; restart required"); return false; }
    int code = 0;
    if (a.transfer && alive()) {
        risc_http_response_v1 response = {.struct_size = sizeof(response)};
        int32_t info = a.http->info(a.http->context, a.transfer, &response);
        if (info == RISC_HTTP_RETAINED) { retain("Daily Paper: retained HTTP failure; restart required"); return false; }
        code = response.status_code;
    }
    if (!alive()) return false;
    char message[72];
    if (code >= 400) snprintf(message, sizeof(message), "Server returned HTTP %d. Retry later.", code);
    else if (rc == RISC_HTTP_TIMEOUT) snprintf(message, sizeof(message), "The download timed out. Check Wi-Fi and retry.");
    else if (rc == RISC_HTTP_MEMORY) snprintf(message, sizeof(message), "Not enough free memory for HTTPS. Restart and retry.");
    else if (rc == RISC_HTTP_NETWORK) snprintf(message, sizeof(message), "Wi-Fi disconnected. Check the network and retry.");
    else if (rc == RISC_HTTP_SIZE) snprintf(message, sizeof(message), "The response is too large or incomplete.");
    else snprintf(message, sizeof(message), "HTTPS error %d. Check network and device date/time.", (int)rc);
    return failure("DOWNLOAD FAILED", message);
}
static bool get(const char *url, bool book) {
    risc_realtime_snapshot_v1 clock = {.struct_size = sizeof(clock)};
    int32_t rc = a.clock->read(a.clock->context, &clock);
    if (!alive()) return false;
    if (rc || clock.validity != RISC_REALTIME_VALID || clock.epoch_seconds < INT64_C(1704067200) || clock.epoch_seconds > INT64_C(4102444799))
        return failure("SET DATE AND TIME", "HTTPS needs a valid clock. Open Settings, then retry.");
    a.downloaded = a.total = 0;
    status(book ? "DOWNLOADING PAPER" : "CHECKING LATEST EDITION", book ? "Saving to the SD card." : "Getting the newest edition date.");
    if (!pump()) return false;
    risc_http_request_v1 request = {sizeof(request), url, book ? RISC_HTTP_MAX_BYTES : DAILY_METADATA_MAX,
        book ? 120000u : 30000u, (uint64_t)clock.epoch_seconds};
    rc = a.http->open(a.http->context, &request, &a.transfer);
    if (!alive()) return false;
    if (rc != RISC_HTTP_OK) return http_failure(rc);
    uint32_t used = 0; uint8_t signature[4] = {0};
    for (;;) {
        if (!pump()) return false;
        uint32_t count = 0;
        rc = a.http->read(a.http->context, a.transfer, a.chunk, sizeof(a.chunk), &count);
        if (!alive()) return false;
        if (rc < 0) return http_failure(rc);
        if (count > sizeof(a.chunk) || count > request.max_bytes - used) return http_failure(RISC_HTTP_SIZE);
        if (rc != RISC_HTTP_OK && rc != RISC_HTTP_AGAIN && rc != RISC_HTTP_EOF) return http_failure(RISC_HTTP_TRANSPORT);
        if (count) {
            if (book) {
                for (uint32_t i = 0; i < count && used + i < 4; ++i) signature[used+i] = a.chunk[i];
                uint32_t written = 0;
                while (written < count) {
                    size_t n = a.volume->base.file_write(a.volume->base.context, a.writer, a.chunk + written, count - written);
                    if (!alive()) return false;
                    if (!n || n > count - written || a.volume->handle_error(a.volume->base.context, a.writer, false))
                        return storage_failure("SD WRITE FAILED");
                    written += (uint32_t)n;
                }
            } else memcpy(a.metadata + used, a.chunk, count);
            used += count; a.downloaded = used;
        }
        if (rc == RISC_HTTP_EOF) break;
        risc_http_response_v1 response = {.struct_size = sizeof(response)};
        int32_t info = a.http->info(a.http->context, a.transfer, &response);
        if (!alive()) return false;
        if (info < 0) return http_failure(info);
        if (response.content_length > 0 && response.content_length <= request.max_bytes)
            a.total = (uint32_t)response.content_length;
    }
    if (!close_http()) return false;
    if (!used || (a.total && used != a.total)) return failure("INCOMPLETE DOWNLOAD", "The response ended before the whole paper arrived.");
    if (!book) {
        a.metadata[used] = 0;
        if (!daily_metadata(a.metadata, used, a.date, a.url))
            return failure("INVALID EDITION DETAILS", "The latest edition date or download address is invalid.");
    } else if (used < 4 || memcmp(signature, "PK\003\004", 4))
        return failure("NOT AN EPUB", "The downloaded response is not an EPUB file.");
    return true;
}
static bool download(void) {
    a.error = a.retry = false; a.date[0] = 0;
    status("STARTING", "Preparing the SD card.");
    if (!pump() || !dependencies() || !connect_wifi() || !get(DAILY_LATEST, false)) return false;
    snprintf(a.destination, sizeof(a.destination), DAILY_DIRECTORY "/%s.epub", a.date);
    snprintf(a.partial, sizeof(a.partial), DAILY_DIRECTORY "/.%s.part", a.date);
    snprintf(a.source, sizeof(a.source), "/sd%s", a.destination);
    if (!a.fs->recover_replace(a.volume->base.context, a.destination)) return storage_failure("SD RECOVERY FAILED");
    if (!alive()) return false;
    risc_storage_metadata_v1 m = {.struct_size = sizeof(m)};
    int32_t rc = a.fs->metadata(a.volume->base.context, a.partial, &m);
    if (!alive()) return false;
    if (rc == RISC_STORAGE_FS_OK) {
        if ((m.flags & RISC_STORAGE_DIRECTORY) || !a.volume->base.remove(a.volume->base.context, a.partial))
            return storage_failure("CANNOT REMOVE INCOMPLETE DOWNLOAD");
    } else if (rc != RISC_STORAGE_FS_NOT_FOUND) return storage_failure("SD CARD UNAVAILABLE");
    if (!alive()) return false;
    a.writer = a.volume->base.file_open_write(a.volume->base.context, a.partial);
    if (!alive()) return false;
    if (!a.writer) return storage_failure("CANNOT SAVE PAPER");
    a.temporary = true;
    if (!get(a.url, true)) return false;
    status("SAVING PAPER", "Finishing the SD file.");
    if (!pump()) return false;
    if (!a.volume->file_sync(a.volume->base.context, a.writer)) return storage_failure("SD SYNC FAILED");
    if (!alive()) return false;
    bool closed = a.volume->base.file_close(a.volume->base.context, a.writer, true);
    if (!alive()) return false;
    if (!closed) { retain("Daily Paper: SD close failed; restart required"); return false; }
    a.writer = 0;
    if (!a.fs->replace_file(a.volume->base.context, a.partial, a.destination)) return storage_failure("CANNOT FINISH SAVING PAPER");
    if (!alive()) return false;
    a.temporary = false;
    return true;
}
__attribute__((visibility("default"))) void app_main(void) {
    memset(&a, 0, sizeof(a)); a.rt = risc_runtime_get_api(1);
    if (!a.rt || a.rt->api_version != 1 || a.rt->struct_size < RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE ||
        !a.rt->retain_invocation || !a.rt->acquire || !a.rt->release || !a.rt->yield_ms) return;
    if (!acquire(0, RISC_SCENE_CAPABILITY, 0)) return;
    a.scene = a.grants[0].api; a.ui = risc_scene_components_get_v1(a.scene);
    if (!a.ui || !a.ui->lifecycle.configure || !a.scene->next || !a.scene->close || !a.scene->snapshot) {
        failure("UI UNAVAILABLE", "Current NOVA scene components are required."); release_to(0); return;
    }
    if (!acquire(1, "file.open", 0)) { release_to(0); return; }
    a.files = a.grants[1].api;
    if (!a.files || a.files->api_version != 1 || a.files->struct_size < sizeof(*a.files) ||
        !a.files->open_request || !a.files->open_take_result) { release_to(0); return; }
    int32_t reader_error = 0; uint64_t cookie = 0;
    bool returned = a.files->open_take_result(&reader_error, &cookie);
    if (!runtime_alive()) return;
    /* The loader relaunches a file-open caller after Reader returns. Consume
     * that result once; never download/reopen in a loop on normal Reader exit. */
    if (returned && cookie == READER_COOKIE && !reader_error) { release_to(0); return; }
    if (!risc_scene_resident_bind_v1(a.rt, &a.resident)) { retain("Daily Paper: resident binding failed"); return; }
    status("STARTING", "Preparing the latest newspaper.");
    if (!open_scene()) { if (close_scene()) release_to(0); return; }
    bool first = true;
    while (alive() && !a.done) {
        bool success;
        if (first && returned && cookie == READER_COOKIE && reader_error) {
            char text[72]; snprintf(text, sizeof(text), "Reader could not open the paper (error %d).", (int)reader_error);
            success = failure("READER OPEN FAILED", text);
        } else success = download();
        first = false;
        if (!cleanup_work()) return;
        if (success && !a.done && !a.error) {
            status("OPENING READER", a.date);
            if (pump() && close_scene()) {
                bool opened = a.files->open_request(a.source, "ebook-reader", READER_COOKIE);
                if (!runtime_alive()) return;
                if (opened) { release_to(0); return; }
                failure("READER UNAVAILABLE", "Paper saved. Install or enable Reader, then retry.");
                if (!open_scene()) break;
            }
        }
        if (a.done) break;
        if (!a.error) failure("DOWNLOAD INTERRUPTED", "The operation stopped. Retry to download again.");
        a.retry = false;
        while (pump()) {
            /* Work is now closed. Only the error screen participates in idle
             * policy; live downloads never sleep or yield focus to the shell. */
            if (a.resident.enabled) {
                risc_scene_navigation_v1 nav = {.struct_size = sizeof(nav)}; uint32_t flags = 0;
                if (!scene_result(a.scene->snapshot(a.scene->context, a.session, &nav, &flags))) break;
                int32_t rc = risc_scene_resident_poll_v1(&a.resident, flags);
                if (rc == RISC_RESIDENT_RETAINED || !runtime_alive()) { retain("Daily Paper: resident cleanup failed"); return; }
                if (rc == RISC_RESIDENT_EXIT) { a.done = true; break; }
                if (a.resident.policy && !(flags & (RISC_SCENE_PRESENTING | RISC_SCENE_INPUT_BUSY))) {
                    if (!close_scene()) return;
                    rc = risc_scene_resident_dispatch_v1(&a.resident, RISC_RESIDENT_CHECKPOINT_POLICY);
                    if (rc == RISC_RESIDENT_RETAINED || !runtime_alive()) { retain("Daily Paper: resident policy failed"); return; }
                    if (rc == RISC_RESIDENT_EXIT) { a.done = true; break; }
                    if (!open_scene()) { a.done = true; break; }
                }
            }
        }
        if (!a.retry) break;
    }
    if (cleanup_work() && close_scene()) release_to(0);
}
