/**
 * @file tip_protocol.h
 * @brief Typio IPC Protocol (TIP) v1 method/topic constants
 *
 * Wire vocabulary for the daemon UDS control surface (ADR-0008).
 * Resource-oriented dotted camelCase methods. The first IPC version with
 * an explicit `protocolVersion` field reported by `hello`; the older
 * unversioned vocabulary is not interoperable.
 */

#ifndef TYPIO_TIP_PROTOCOL_H
#define TYPIO_TIP_PROTOCOL_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Protocol version reported by hello ----------
 * v2: engine.use / engine.next replaced by modality-explicit keyboard.* /
 *     voice.* verbs (ADR-0026).
 * v3: language.* namespace, daemon.status activeLanguage, language.changed
 *     event (ADR-0031). */
#define TYPIO_IPC_PROTOCOL_VERSION 3

/**
 * @brief Return the canonical UDS socket path.
 *
 * Prefers $XDG_RUNTIME_DIR/typio/daemon.sock.
 * Falls back to ~/.local/share/typio/daemon.sock.
 * Caller must free() the returned string.
 */
char *typio_ipc_socket_path(void);

/* ---------- JSON-RPC 2.0 methods (TIP v1) ---------- */
#define TYPIO_IPC_METHOD_HELLO            "hello"

#define TYPIO_IPC_METHOD_CONFIG_GET       "config.get"
#define TYPIO_IPC_METHOD_CONFIG_SET       "config.set"
#define TYPIO_IPC_METHOD_CONFIG_UNSET     "config.unset"
#define TYPIO_IPC_METHOD_CONFIG_LIST      "config.list"
#define TYPIO_IPC_METHOD_CONFIG_SHOW      "config.show"
#define TYPIO_IPC_METHOD_CONFIG_RELOAD    "config.reload"

/* Cross-modality aggregate + lifecycle surface (keyed by engine name). */
#define TYPIO_IPC_METHOD_ENGINE_LIST      "engine.list"
#define TYPIO_IPC_METHOD_ENGINE_DESCRIBE  "engine.describe"
#define TYPIO_IPC_METHOD_ENGINE_INVOKE    "engine.invoke"
#define TYPIO_IPC_METHOD_ENGINE_LOAD      "engine.load"
#define TYPIO_IPC_METHOD_ENGINE_UNLOAD    "engine.unload"
#define TYPIO_IPC_METHOD_ENGINE_RELOAD    "engine.reload"

/* Modality-explicit activation/cycling (ADR-0026). The keyboard and voice
 * slots are orthogonal and simultaneously active, so each gets its own verbs
 * rather than a kind-discriminated engine.use/engine.next. */
#define TYPIO_IPC_METHOD_KEYBOARD_USE     "keyboard.use"
#define TYPIO_IPC_METHOD_KEYBOARD_NEXT    "keyboard.next"
#define TYPIO_IPC_METHOD_KEYBOARD_PREV    "keyboard.prev"
#define TYPIO_IPC_METHOD_VOICE_USE        "voice.use"
#define TYPIO_IPC_METHOD_VOICE_NEXT       "voice.next"
#define TYPIO_IPC_METHOD_VOICE_PREV       "voice.prev"

/* Language-first switching (ADR-0031). The active language retargets the
 * keyboard and voice slots together; per-language engine selection is plain
 * config (languages.<tag>.keyboard / .voice). */
#define TYPIO_IPC_METHOD_LANGUAGE_LIST    "language.list"
#define TYPIO_IPC_METHOD_LANGUAGE_USE     "language.use"
#define TYPIO_IPC_METHOD_LANGUAGE_NEXT    "language.next"
#define TYPIO_IPC_METHOD_LANGUAGE_PREV    "language.prev"

#define TYPIO_IPC_METHOD_DAEMON_STATUS    "daemon.status"
#define TYPIO_IPC_METHOD_DAEMON_STOP      "daemon.stop"
#define TYPIO_IPC_METHOD_DAEMON_VERSION   "daemon.version"

#define TYPIO_IPC_METHOD_EVENTS_SUBSCRIBE "events.subscribe"

/* ---------- Event topics (server -> client notification.method) ---------- */
#define TYPIO_IPC_TOPIC_ENGINE_CHANGED      "engine.changed"
#define TYPIO_IPC_TOPIC_LANGUAGE_CHANGED    "language.changed"
#define TYPIO_IPC_TOPIC_ENGINE_STATUS_CHANGED "engine.statusChanged"
#define TYPIO_IPC_TOPIC_CONFIG_CHANGED      "config.changed"
#define TYPIO_IPC_TOPIC_RUNTIME_CHANGED     "runtime.changed"
#define TYPIO_IPC_TOPIC_DAEMON_SHUTDOWN     "daemon.shuttingDown"

#ifdef __cplusplus
}
#endif

#endif /* TYPIO_TIP_PROTOCOL_H */
