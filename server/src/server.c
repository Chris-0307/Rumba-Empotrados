#define _POSIX_C_SOURCE 200809L
#include "robot_adapter.h"
#include "audio_catalog.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define REQUEST_MAX 16384
#define BODY_MAX 4096
#define RESPONSE_MAX 8192
#define TOKEN_SECONDS 86400

static sqlite3 *db;
static volatile sig_atomic_t stopping;
static void request_shutdown(int signum) { (void)signum; stopping = 1; }
static int speed = 0, volume = 100, current_song = 0;
static const char *audio_state = "stopped";
static const char *allowed_origin;
static char request_origin[256];

typedef struct { char method[8], path[128], auth[128], body[BODY_MAX + 1]; } Request;

static int format(char *out, size_t cap, const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    int n = vsnprintf(out, cap, fmt, args);
    va_end(args);
    return n >= 0 && (size_t)n < cap ? 0 : -1;
}

/* Respuestas JSON pequeñas; se cierra la conexión tras cada petición. */
static void respond(int fd, int code, const char *json) {
    const char *label = code == 200 ? "OK" : code == 201 ? "Created" :
        code == 400 ? "Bad Request" : code == 401 ? "Unauthorized" :
        code == 403 ? "Forbidden" : code == 404 ? "Not Found" :
        code == 405 ? "Method Not Allowed" : code == 409 ? "Conflict" :
        code == 413 ? "Content Too Large" : code == 503 ? "Service Unavailable" : "Internal Server Error";
    char header[640];
    const char *cors = allowed_origin && request_origin[0] && strcmp(allowed_origin,request_origin)==0 ?
        "Access-Control-Allow-Origin: %s\r\nVary: Origin\r\nAccess-Control-Allow-Methods: GET, POST, PUT, OPTIONS\r\nAccess-Control-Allow-Headers: Authorization, Content-Type\r\n" : "";
    char cors_headers[420]="";
    if (*cors && format(cors_headers,sizeof cors_headers,cors,allowed_origin)) return;
    int n = snprintf(header, sizeof header,
        "HTTP/1.1 %d %s\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: %zu\r\nCache-Control: no-store\r\n%sConnection: close\r\n\r\n",
        code, label, strlen(json),cors_headers);
    if (n < 0 || (size_t)n >= sizeof header) return;
    const char *parts[] = { header, json };
    size_t lens[] = { (size_t)n, strlen(json) };
    for (int p = 0; p < 2; p++) {
        for (size_t off = 0; off < lens[p]; ) {
            ssize_t wrote = send(fd, parts[p] + off, lens[p] - off, 0);
            if (wrote < 0 && errno == EINTR) continue;
            if (wrote <= 0) return;
            off += (size_t)wrote;
        }
    }
}
static void error_json(int fd, int code, const char *message) {
    char out[256];
    if (format(out, sizeof out, "{\"error\":\"%s\"}", message) == 0) respond(fd, code, out);
}

/* Solo objetos JSON planos: claves ASCII y valores string sin escapes o enteros. */
static int field(const char *body, const char *wanted, char *out, size_t cap) {
    const char *p = body;
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    if (*p++ != '{') return -1;
    while (*p) {
        while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r') p++;
        if (*p == '}') return -1;
        if (*p++ != '"') return -1;
        const char *start = p;
        while (*p && *p != '"' && *p != '\\') p++;
        if (*p != '"') return -1;
        size_t keylen = (size_t)(p - start); p++;
        while (*p == ' ') p++;
        if (*p++ != ':') return -1;
        while (*p == ' ') p++;
        bool quoted = *p == '"';
        if (quoted) p++;
        const char *value = p;
        if (quoted) {
            while (*p && *p != '"' && *p != '\\') p++;
            if (*p != '"') return -1;
        } else {
            while (*p >= '0' && *p <= '9') p++;
            if (p == value) return -1;
        }
        size_t len = (size_t)(p - value);
        if (quoted) p++;
        const char *tail=p;while (*tail==' ' || *tail=='\t' || *tail=='\r' || *tail=='\n') tail++;
        if (*tail!=',' && *tail!='}') return -1;
        if (strlen(wanted) == keylen && memcmp(start, wanted, keylen) == 0) {
            if (!len || len >= cap) return -1;
            memcpy(out, value, len); out[len] = 0; return 0;
        }
        while (*p == ' ') p++;
        if (*p != ',' && *p != '}') return -1;
    }
    return -1;
}
static int integer_field(const char *body, const char *name, int *value, int min, int max) {
    char buf[32];
    if (field(body, name, buf, sizeof buf)) return -1;
    char *end; errno = 0; long n = strtol(buf, &end, 10);
    if (errno || *end || n < min || n > max) return -1;
    *value = (int)n; return 0;
}

/* Recibe cabeceras completas y exactamente Content-Length bytes (sin chunked). */
static int receive_request(int fd, Request *r) {
    char raw[REQUEST_MAX + 1]; size_t used = 0, header_size = 0;
    while (!header_size && used < REQUEST_MAX) {
        ssize_t n = recv(fd, raw + used, REQUEST_MAX - used, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return 400;
        used += (size_t)n; raw[used] = 0;
        char *boundary = strstr(raw, "\r\n\r\n");
        if (boundary) header_size = (size_t)(boundary - raw) + 4;
    }
    if (!header_size) return 413;
    if (sscanf(raw, "%7s %127s", r->method, r->path) != 2) return 400;
    char *line_end = strstr(raw, "\r\n");
    if (!line_end || line_end >= raw + header_size) return 400;
    int length = -1;
    for (char *line = line_end + 2; line < raw + header_size - 2; ) {
        char *end = strstr(line, "\r\n");
        if (!end || end > raw + header_size) return 400;
        if ((size_t)(end - line) >= 16 && strncasecmp(line, "Content-Length:", 15) == 0) {
            char *num = line + 15;
            while (*num == ' ') num++;
            char *tail; errno = 0; long parsed = strtol(num, &tail, 10);
            if (errno || tail != end || parsed < 0 || parsed > BODY_MAX || length >= 0) return 413;
            length = (int)parsed;
        }
        if (strncasecmp(line, "Authorization: Bearer ", 22) == 0) {
            size_t n = (size_t)(end - line - 22);
            if (n >= sizeof r->auth) return 400;
            memcpy(r->auth, line + 22, n); r->auth[n] = 0;
        }
        if (strncasecmp(line, "Origin: ", 8) == 0) {
            size_t n=(size_t)(end-line-8);
            if (n>=sizeof request_origin) return 400;
            memcpy(request_origin,line+8,n); request_origin[n]=0;
        }
        if (strncasecmp(line, "Transfer-Encoding:", 18) == 0) return 400;
        line = end + 2;
    }
    if (length < 0) length = 0;
    if (used - header_size > (size_t)length) return 400;
    memcpy(r->body, raw + header_size, used - header_size);
    size_t got = used - header_size;
    while (got < (size_t)length) {
        ssize_t n = recv(fd, r->body + got, (size_t)length - got, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return 400;
        got += (size_t)n;
    }
    r->body[got] = 0;
    if (memchr(r->body, 0, got)) return 400;
    return 0;
}

static int sql(const char *statement) { return sqlite3_exec(db, statement, NULL, NULL, NULL) == SQLITE_OK ? 0 : -1; }
static sqlite3_stmt *prepare(const char *query) {
    sqlite3_stmt *stmt = NULL;
    return sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK ? stmt : NULL;
}
static int init_db(const char *path) {
    if (sqlite3_open(path, &db) != SQLITE_OK) return -1;
    sqlite3_busy_timeout(db, 3000);
    return sql("CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, username TEXT UNIQUE NOT NULL, salt BLOB NOT NULL, hash BLOB NOT NULL);") ||
        sql("CREATE TABLE IF NOT EXISTS sessions (token_hash BLOB PRIMARY KEY, user_id INTEGER NOT NULL, expires INTEGER NOT NULL);") ||
        sql("CREATE TABLE IF NOT EXISTS songs (id INTEGER PRIMARY KEY, title TEXT NOT NULL, file_path TEXT UNIQUE NOT NULL);") ? -1 : 0;
}
static void hex_encode(const unsigned char *input, size_t count, char *output) {
    const char *digits = "0123456789abcdef";
    for (size_t i = 0; i < count; i++) { output[2*i] = digits[input[i] >> 4]; output[2*i+1] = digits[input[i] & 15]; }
    output[2*count] = 0;
}
static int password_hash(const char *password, const unsigned char salt[16], unsigned char hash[32]) {
    return EVP_PBE_scrypt(password, strlen(password), salt, 16, 16384, 8, 1, 32*1024*1024, hash, 32) == 1 ? 0 : -1;
}
static int token_digest(const char *token, unsigned char digest[32]) {
    if (strlen(token) != 64) return -1;
    for (size_t i = 0; i < 64; i++) if (!((token[i] >= '0' && token[i] <= '9') || (token[i] >= 'a' && token[i] <= 'f'))) return -1;
    return EVP_Digest(token, 64, digest, NULL, EVP_sha256(), NULL) == 1 ? 0 : -1;
}
static int authenticated(const Request *r, int *user_id) {
    unsigned char digest[32]; if (token_digest(r->auth, digest)) return -1;
    sqlite3_stmt *s = prepare("SELECT user_id FROM sessions WHERE token_hash=? AND expires>?");
    if (!s) return -1;
    sqlite3_bind_blob(s, 1, digest, 32, SQLITE_TRANSIENT);
    sqlite3_bind_int64(s, 2, (sqlite3_int64)time(NULL));
    int ok = sqlite3_step(s) == SQLITE_ROW;
    if (ok) *user_id = sqlite3_column_int(s, 0);
    sqlite3_finalize(s); return ok ? 0 : -1;
}
static int valid_username(const char *s) {
    size_t n = strlen(s); if (n < 3 || n > 32) return 0;
    for (; *s; s++) if (!( (*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_')) return 0;
    return 1;
}
static void register_user(int fd, const Request *r) {
    char username[64], password[128];
    if (field(r->body,"username",username,sizeof username) || field(r->body,"password",password,sizeof password) ||
        !valid_username(username) || strlen(password) < 12 || strlen(password) > 120) {
        error_json(fd,400,"username: 3-32 letras/numeros/_, password: 12-120 caracteres"); return;
    }
    unsigned char salt[16], hash[32];
    if (RAND_bytes(salt,16) != 1 || password_hash(password,salt,hash)) { error_json(fd,500,"crypto_error"); return; }
    sqlite3_stmt *s = prepare("INSERT INTO users(username,salt,hash) VALUES(?,?,?)");
    if (!s) { error_json(fd,500,"database_error"); return; }
    sqlite3_bind_text(s,1,username,-1,SQLITE_TRANSIENT);
    sqlite3_bind_blob(s,2,salt,16,SQLITE_TRANSIENT);
    sqlite3_bind_blob(s,3,hash,32,SQLITE_TRANSIENT);
    int result = sqlite3_step(s); sqlite3_finalize(s);
    if (result == SQLITE_CONSTRAINT) error_json(fd,409,"username_exists");
    else if (result != SQLITE_DONE) error_json(fd,500,"database_error");
    else respond(fd,201,"{\"created\":true}");
}
static void login_user(int fd, const Request *r) {
    char username[64], password[128];
    if (field(r->body,"username",username,sizeof username) || field(r->body,"password",password,sizeof password)) { error_json(fd,400,"invalid_credentials_format"); return; }
    sqlite3_stmt *s = prepare("SELECT id,salt,hash FROM users WHERE username=?");
    if (!s) { error_json(fd,500,"database_error"); return; }
    sqlite3_bind_text(s,1,username,-1,SQLITE_TRANSIENT);
    int id = -1; unsigned char salt[16], expected[32], actual[32];
    if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_bytes(s,1) == 16 && sqlite3_column_bytes(s,2) == 32) {
        id=sqlite3_column_int(s,0); memcpy(salt,sqlite3_column_blob(s,1),16); memcpy(expected,sqlite3_column_blob(s,2),32);
    }
    sqlite3_finalize(s);
    if (id < 0 || password_hash(password,salt,actual) || CRYPTO_memcmp(expected,actual,32)) { error_json(fd,401,"invalid_credentials"); return; }
    unsigned char random_token[32], digest[32]; char token[65], out[160];
    if (RAND_bytes(random_token,32) != 1) { error_json(fd,500,"crypto_error"); return; }
    hex_encode(random_token,32,token);
    if (token_digest(token,digest)) { error_json(fd,500,"crypto_error"); return; }
    s=prepare("INSERT INTO sessions(token_hash,user_id,expires) VALUES(?,?,?)");
    if (!s) { error_json(fd,500,"database_error"); return; }
    sqlite3_bind_blob(s,1,digest,32,SQLITE_TRANSIENT);
    sqlite3_bind_int(s,2,id); sqlite3_bind_int64(s,3,(sqlite3_int64)time(NULL)+TOKEN_SECONDS);
    int result=sqlite3_step(s); sqlite3_finalize(s);
    if (result != SQLITE_DONE) { error_json(fd,500,"database_error"); return; }
    if (format(out,sizeof out,"{\"token\":\"%s\",\"expires_in\":%d}",token,TOKEN_SECONDS)==0) respond(fd,200,out);
}

static void dispatch(int fd, const Request *r) {
    if (!strcmp(r->method,"OPTIONS") && allowed_origin && request_origin[0] && !strcmp(allowed_origin,request_origin)) {
        respond(fd,200,"{}"); return;
    }
    if (!strcmp(r->method,"GET") && !strcmp(r->path,"/api/status")) { respond(fd,200,"{\"status\":\"ok\"}"); return; }
    if (!strcmp(r->method,"POST") && !strcmp(r->path,"/api/auth/register")) { register_user(fd,r); return; }
    if (!strcmp(r->method,"POST") && !strcmp(r->path,"/api/auth/login")) { login_user(fd,r); return; }
    int uid;
    if (authenticated(r,&uid)) { error_json(fd,401,"unauthorized"); return; }
    char out[RESPONSE_MAX];
    if (!strcmp(r->method,"POST") && !strcmp(r->path,"/api/auth/logout")) {
        unsigned char digest[32]; sqlite3_stmt *s=prepare("DELETE FROM sessions WHERE token_hash=?");
        if (!s || token_digest(r->auth,digest)) { sqlite3_finalize(s); error_json(fd,500,"database_error"); return; }
        sqlite3_bind_blob(s,1,digest,32,SQLITE_TRANSIENT);
        int result=sqlite3_step(s); sqlite3_finalize(s);
        if (result == SQLITE_DONE) respond(fd,200,"{\"logged_out\":true}"); else error_json(fd,500,"database_error"); return;
    }
    if (!strcmp(r->method,"GET") && !strcmp(r->path,"/api/profile")) {
        sqlite3_stmt *s=prepare("SELECT username FROM users WHERE id=?");
        if (!s) { error_json(fd,500,"database_error"); return; }
        sqlite3_bind_int(s,1,uid);
        if (sqlite3_step(s)==SQLITE_ROW) {
            const char *name=(const char *)sqlite3_column_text(s,0);
            if (format(out,sizeof out,"{\"id\":%d,\"username\":\"%s\"}",uid,name)==0) respond(fd,200,out);
        } else error_json(fd,404,"user_not_found");
        sqlite3_finalize(s); return;
    }
    if (!strcmp(r->method,"GET") && !strcmp(r->path,"/api/suction")) {
        if (robot_adapter_suction_json(out,sizeof out)) error_json(fd,503,"suction_unavailable");
        else respond(fd,200,out);
        return;
    }
    if (!strcmp(r->method,"PUT") && !strcmp(r->path,"/api/suction")) {
        char action[8];
        if (field(r->body,"action",action,sizeof action) || (strcmp(action,"on") && strcmp(action,"off"))) {
            error_json(fd,400,"invalid_suction_action");return;
        }
        int suction_result=robot_adapter_suction_set(!strcmp(action,"on"));
        if (suction_result==-5) {error_json(fd,409,"cycle_completed");return;}
        if (suction_result) {error_json(fd,503,"suction_unavailable");return;}
        if (robot_adapter_suction_json(out,sizeof out)) error_json(fd,503,"suction_unavailable");
        else respond(fd,200,out);
        return;
    }
    if (!strcmp(r->method,"GET") && !strcmp(r->path,"/api/mode")) {
        if (robot_adapter_mode_json(out,sizeof out)) { error_json(fd,503,"controller_unavailable"); return; }
        respond(fd,200,out); return;
    }
    if (!strcmp(r->method,"PUT") && !strcmp(r->path,"/api/mode")) {
        char value[16];
        if (field(r->body,"mode",value,sizeof value) || (strcmp(value,"manual") && strcmp(value,"automatic"))) { error_json(fd,400,"invalid_mode"); return; }
        int duration=0;
        if (strstr(r->body,"\"duration_seconds\"") && integer_field(r->body,"duration_seconds",&duration,0,86400)) {
            error_json(fd,400,"invalid_duration_seconds");return;
        }
        if (robot_adapter_mode_timed(value,duration)) { error_json(fd,503,"controller_unavailable"); return; }
        speed=0; if (robot_adapter_mode_json(out,sizeof out)) { error_json(fd,503,"controller_unavailable"); return; }
        respond(fd,200,out); return;
    }
    if (!strcmp(r->path,"/api/motors") && (!strcmp(r->method,"GET") || !strcmp(r->method,"PUT"))) {
        if (!strcmp(r->method,"PUT")) {
            int left,right;
            if (integer_field(r->body,"left_speed",&left,0,100) || integer_field(r->body,"right_speed",&right,0,100)) {error_json(fd,400,"invalid_motor_speeds");return;}
            if (robot_adapter_motors_set(left,right)) {error_json(fd,503,"motors_unavailable");return;}
        }
        if (robot_adapter_motors_json(out,sizeof out)) error_json(fd,503,"motors_unavailable");
        else respond(fd,200,out);
        return;
    }
    if (!strcmp(r->method,"GET") && !strcmp(r->path,"/api/sensors")) {
        if (robot_adapter_sensor_json(out,sizeof out)) { error_json(fd,503,"sensors_unavailable"); return; }
        respond(fd,200,out); return;
    }
    if (!strcmp(r->method,"GET") && !strcmp(r->path,"/api/indicators")) {
        double f,l,rr;
        if (robot_adapter_sensors(&f,&l,&rr)) { format(out,sizeof out,"{\"system\":\"functional\",\"mode\":\"%s\",\"obstacle\":null}",robot_adapter_get_mode()); respond(fd,200,out); return; }
        format(out,sizeof out,"{\"system\":\"functional\",\"mode\":\"%s\",\"obstacle\":%s}",robot_adapter_get_mode(),(f<20)?"true":"false"); respond(fd,200,out); return;
    }
    if (!strcmp(r->method,"POST") && !strcmp(r->path,"/api/move")) {
        char direction[20]; int requested_speed=0;
        if (field(r->body,"direction",direction,sizeof direction) ||
            (strcmp(direction,"forward") && strcmp(direction,"backward") && strcmp(direction,"left") && strcmp(direction,"right") && strcmp(direction,"stop"))) { error_json(fd,400,"invalid_direction"); return; }
        int legacy=strstr(r->body,"\"speed\"")!=NULL;
        if (legacy && integer_field(r->body,"speed",&requested_speed,0,100)) {error_json(fd,400,"invalid_speed");return;}
        int move_result=legacy?robot_adapter_move(direction,requested_speed):robot_adapter_move_configured(direction);
        if (move_result==-4) { error_json(fd,409,"floor_safety_blocked"); return; }
        if (move_result==-3) { error_json(fd,409,"manual_mode_required"); return; }
        if (move_result) { error_json(fd,503,"motors_unavailable"); return; }
        speed=strcmp(direction,"stop") ? requested_speed : 0;
        if (legacy) format(out,sizeof out,"{\"direction\":\"%s\",\"speed\":%d}",direction,speed);
        else if (robot_adapter_motors_json(out,sizeof out)) {error_json(fd,503,"motors_unavailable");return;}
        respond(fd,200,out);return;
    }
    if (!strcmp(r->method,"GET") && !strcmp(r->path,"/api/map")) {
        if (robot_adapter_map(out,sizeof out)) error_json(fd,503,"map_unavailable"); else respond(fd,200,out); return;
    }
    if (!strcmp(r->method,"GET") && !strcmp(r->path,"/api/audio/songs")) {
        if (audio_catalog_playlist(out,sizeof out)) error_json(fd,503,"playlist_unavailable_or_over_60MB");
        else respond(fd,200,out);
        return;
    }
    if (!strcmp(r->method,"GET") && !strcmp(r->path,"/api/audio/state")) {
        const char *actual=robot_adapter_audio_state();
        if (actual) audio_state=actual;
        format(out,sizeof out,"{\"state\":\"%s\",\"song_id\":%d,\"volume\":%d}",audio_state,current_song,volume);
        respond(fd,200,out);return;
    }
    if (!strcmp(r->method,"PUT") && !strcmp(r->path,"/api/audio/volume")) {
        int n;if(integer_field(r->body,"volume",&n,0,100)) { error_json(fd,400,"invalid_volume");return; }
        if(robot_adapter_audio("volume",current_song,n)) { error_json(fd,503,"audio_unavailable");return; }
        volume=n;format(out,sizeof out,"{\"volume\":%d}",volume);respond(fd,200,out);return;
    }
    if (!strcmp(r->method,"POST") && !strcmp(r->path,"/api/audio/play")) {
        int id;char path[1024];
        if(integer_field(r->body,"song_id",&id,1,3)) { error_json(fd,400,"invalid_song_id");return; }
        if(audio_catalog_path(id,path,sizeof path)) { error_json(fd,404,"song_unavailable_or_over_60MB");return; }
        if(robot_adapter_audio("play",id,volume)) { error_json(fd,503,"audio_unavailable");return; }
        current_song=id;audio_state="playing";
        format(out,sizeof out,"{\"state\":\"playing\",\"song_id\":%d}",id);respond(fd,200,out);return;
    }
    if (!strcmp(r->method,"POST") && !strcmp(r->path,"/api/audio/stop")) {
        if(robot_adapter_audio("stop",current_song,volume)) { error_json(fd,503,"audio_unavailable");return; }
        current_song=0;audio_state="stopped";respond(fd,200,"{\"state\":\"stopped\",\"song_id\":0}");return;
    }
    if (!strcmp(r->method,"POST") && (!strcmp(r->path,"/api/audio/pause") || !strcmp(r->path,"/api/audio/next"))) {
        int next=current_song;
        const char *action="pause";
        if (!strcmp(r->path,"/api/audio/next")) { next=audio_catalog_next(current_song);action="play"; }
        else {
            const char *actual=robot_adapter_audio_state();
            if(actual) audio_state=actual;
            if(!strcmp(audio_state,"stopped")) { error_json(fd,409,"audio_not_playing");return; }
        }
        if(next<1) { error_json(fd,404,"playlist_empty");return; }
        if(robot_adapter_audio(action,next,volume)) { error_json(fd,503,"audio_unavailable");return; }
        current_song=next;audio_state=!strcmp(action,"play")?"playing":"paused";
        format(out,sizeof out,"{\"state\":\"%s\",\"song_id\":%d}",audio_state,current_song);respond(fd,200,out);return;
    }
    error_json(fd,404,"not_found");
}

int main(int argc, char **argv) {
    const char *db_path=argc>1 ? argv[1] : "robot.db";
    int port=argc>2 ? atoi(argv[2]) : 8080;
    allowed_origin=getenv("ROBOT_ALLOWED_ORIGIN");
    if (allowed_origin && (strlen(allowed_origin)>128 || strchr(allowed_origin,'\r') || strchr(allowed_origin,'\n'))) {
        fprintf(stderr,"ROBOT_ALLOWED_ORIGIN invalido\n"); return 1;
    }
    if (port<1 || port>65535 || init_db(db_path)) { fprintf(stderr,"No se pudo iniciar base de datos o puerto\n"); return 1; }
    signal(SIGPIPE,SIG_IGN);
    int listener=socket(AF_INET,SOCK_STREAM,0);
    if (listener<0) { perror("socket"); return 1; }
    int reuse=1; setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof reuse);
    struct sockaddr_in address={0}; address.sin_family=AF_INET; address.sin_addr.s_addr=htonl(INADDR_ANY); address.sin_port=htons((unsigned short)port);
    if (bind(listener,(struct sockaddr *)&address,sizeof address)<0 || listen(listener,8)<0) { perror("bind/listen"); close(listener); return 1; }
    if (robot_adapter_init()) {
        fprintf(stderr,"No se pudo inicializar el controlador del robot\n");
        close(listener); sqlite3_close(db); return 1;
    }
    if (atexit(robot_adapter_cleanup)) {
        robot_adapter_cleanup(); close(listener); sqlite3_close(db); return 1;
    }
    struct sigaction shutdown_action={0};
    shutdown_action.sa_handler=request_shutdown;
    sigemptyset(&shutdown_action.sa_mask);
    if (sigaction(SIGINT,&shutdown_action,NULL) || sigaction(SIGTERM,&shutdown_action,NULL)) {
        perror("sigaction"); close(listener); sqlite3_close(db); return 1;
    }
    fprintf(stderr,"Robot API escuchando en :%d (database=%s)\n",port,db_path);
    while (!stopping) {
        /* Espera con timeout para que una señal previa a accept no bloquee la salida. */
        fd_set readable; FD_ZERO(&readable); FD_SET(listener,&readable);
        struct timeval poll_timeout={.tv_sec=1,.tv_usec=0};
        int ready=select(listener+1,&readable,NULL,NULL,&poll_timeout);
        if (ready<0) { if (errno==EINTR) continue; perror("select"); break; }
        if (!ready || stopping) continue;
        int client=accept(listener,NULL,NULL);
        if (client<0) { if (errno!=EINTR) perror("accept"); continue; }
        struct timeval timeout={.tv_sec=5,.tv_usec=0};
        setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof timeout);
        setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof timeout);
        Request request={0}; int result=receive_request(client,&request);
        if (result) error_json(client,result,result==413?"request_too_large":"invalid_request");
        else dispatch(client,&request);
        close(client);
        request_origin[0]=0;
    }
    close(listener);
    sqlite3_close(db);
    return 0;
}
