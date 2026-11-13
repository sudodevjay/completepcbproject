/**
  ******************************************************************************
  * @file    http.c
  * @brief   Password-protected settings web page. See http.h.
  *
  * A request is parsed as it arrives, byte by byte:
  *   request line ("POST /login HTTP/1.1") -> kept
  *   header lines                          -> only Content-Length is used
  *   empty line, then Content-Length bytes -> the form body, kept
  * Every answer closes the connection.
  ******************************************************************************
  */
#include "http.h"
#include "w5500.h"
#include "settings.h"
#include "eeprom.h"
#include <string.h>

#define LINE_MAX_HTTP 160u      /* request line; longer ones are cut */
#define HDR_MAX       32u       /* enough to recognise "Content-Length: n" */
#define BODY_MAX      256u      /* form body; longer ones are cut */
#define PAGE_MAX      3072u
#define REBOOT_DELAY  1500u     /* ms after "saved", so the page gets out first */

#define TOKEN_LEN     16u
#define TOKEN_TTL_MS  (10u * 60u * 1000u)
#define MAX_FAILS     5u
#define LOCK_MS       60000u

typedef enum
{
    RQ_LINE = 0,
    RQ_HEADER,
    RQ_BODY,
    RQ_DONE                     /* answered, waiting for the disconnect */
} rq_state_t;

typedef struct
{
    rq_state_t st;
    char       line[LINE_MAX_HTTP];
    uint16_t   line_len;
    char       hdr[HDR_MAX];
    uint8_t    hdr_len;
    char       body[BODY_MAX];
    uint16_t   body_len;        /* bytes kept */
    uint16_t   body_rx;         /* bytes received (may exceed BODY_MAX) */
    uint16_t   content_len;
} request_t;

static request_t s_req[HTTP_SOCKET_COUNT];
static char      s_page[PAGE_MAX];
static uint16_t  s_pos;
static bool      s_reboot;
static uint32_t  s_reboot_at;

/* login session: one at a time, the newest login wins */
static char      s_token[TOKEN_LEN + 1];
static uint32_t  s_token_at;
static uint8_t   s_fails;
static uint32_t  s_lock_at;

static const char s_hex[] = "0123456789ABCDEF";

/* ---- page building -------------------------------------------------------- */

static void out(const char *s)
{
    while (*s && s_pos < PAGE_MAX - 1u)
    {
        s_page[s_pos++] = *s++;
    }
    s_page[s_pos] = '\0';
}

static void out_u(uint32_t v)
{
    char tmp[11];
    uint8_t n = 0;

    do
    {
        tmp[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
    {
        char c[2] = { tmp[--n], '\0' };
        out(c);
    }
}

static void out_hex8(uint8_t v)
{
    char c[3] = { s_hex[v >> 4], s_hex[v & 0x0F], '\0' };
    out(c);
}

static void out_ip(const uint8_t ip[4])
{
    for (uint8_t i = 0; i < 4; i++)
    {
        if (i)
        {
            out(".");
        }
        out_u(ip[i]);
    }
}

static void out_mac(const uint8_t mac[6])
{
    for (uint8_t i = 0; i < 6; i++)
    {
        if (i)
        {
            out(":");
        }
        out_hex8(mac[i]);
    }
}

static void page_head(const char *title)
{
    s_pos = 0;
    s_page[0] = '\0';
    out("<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>");
    out(title);
    out("</title><style>"
        "body{font-family:Arial,sans-serif;background:#f2f4f7;margin:0;padding:16px}"
        ".c{max-width:420px;margin:auto;background:#fff;border-radius:8px;padding:20px;"
        "box-shadow:0 1px 4px rgba(0,0,0,.15)}"
        "h2{margin-top:0}label{display:block;margin-top:12px;font-weight:bold}"
        "input,select{width:100%;box-sizing:border-box;padding:8px;margin-top:4px;font-size:15px}"
        "button{margin-top:18px;width:100%;padding:10px;font-size:16px;background:#1565c0;"
        "color:#fff;border:0;border-radius:4px}"
        ".e{color:#c62828;font-weight:bold}.ok{color:#2e7d32;font-weight:bold}"
        "table{width:100%;font-size:14px;margin-top:8px}td:first-child{color:#555}"
        "</style></head><body><div class=\"c\"><h2>Barrier Controller</h2>");
}

static void page_tail(void)
{
    out("</div></body></html>");
}

static void login_page(const char *error)
{
    page_head("Login");
    if (error)
    {
        out("<p class=\"e\">");
        out(error);
        out("</p>");
    }
    out("<form action=\"/login\" method=\"post\">"
        "<label>Password</label><input name=\"pw\" type=\"password\" autofocus>"
        "<button type=\"submit\">Login</button></form>");
    page_tail();
}

static void form_page(const settings_t *s, const char *error)
{
    page_head("Settings");
    if (error)
    {
        out("<p class=\"e\">");
        out(error);
        out("</p>");
    }
    out("<form action=\"/save\" method=\"post\">"
        "<input type=\"hidden\" name=\"token\" value=\"");
    out(s_token);
    out("\"><label>IP address</label><input name=\"ip\" value=\"");
    out_ip(s->ip);
    out("\"><label>Subnet mask</label><input name=\"sn\" value=\"");
    out_ip(s->subnet);
    out("\"><label>Gateway</label><input name=\"gw\" value=\"");
    out_ip(s->gateway);
    out("\"><label>TCP port (commands)</label><input name=\"port\" type=\"number\" min=\"1\" max=\"65535\" value=\"");
    out_u(s->port);
    out("\"><label>Gate type</label><select name=\"gate\">"
        "<option value=\"en\"");
    if (s->gate != GATE_EXIT)
    {
        out(" selected");
    }
    out(">Entry (|OPENEN%, MAC ..:01)</option><option value=\"ex\"");
    if (s->gate == GATE_EXIT)
    {
        out(" selected");
    }
    out(">Exit (|OPENEX%, MAC ..:02)</option></select>"
        "<button type=\"submit\">Save &amp; Reboot</button></form>"
        "<table><tr><td>MAC</td><td>");
    out_mac(g_mac);
    out("</td></tr><tr><td>EEPROM</td><td>");
    if (eeprom_addr())
    {
        out(eeprom_name());
        out(" @ 0x");
        out_hex8(eeprom_addr());
    }
    else
    {
        out("<span class=\"e\">not found</span>");
    }
    out("</td></tr><tr><td>Gate</td><td>");
    out(settings_gate_name(g_settings.gate));
    out("</td></tr><tr><td>Command</td><td>|");
    out(settings_open_cmd(g_settings.gate));
    out("% on TCP port ");
    out_u(g_settings.port);
    out("</td></tr></table>");
    page_tail();
}

static void saved_page(const settings_t *s)
{
    uint8_t mac[6];

    settings_mac(s->gate, mac);
    page_head("Saved");
    out("<p class=\"ok\">Settings saved. Rebooting...</p>"
        "<table><tr><td>IP</td><td>");
    out_ip(s->ip);
    out("</td></tr><tr><td>Subnet</td><td>");
    out_ip(s->subnet);
    out("</td></tr><tr><td>Gateway</td><td>");
    out_ip(s->gateway);
    out("</td></tr><tr><td>TCP port</td><td>");
    out_u(s->port);
    out("</td></tr><tr><td>Gate</td><td>");
    out(settings_gate_name(s->gate));
    out(" (|");
    out(settings_open_cmd(s->gate));
    out("%)</td></tr><tr><td>MAC</td><td>");
    out_mac(mac);
    out("</td></tr></table><p>New page: <a href=\"http://");
    out_ip(s->ip);
    out("/\">http://");
    out_ip(s->ip);
    out("/</a></p>");
    page_tail();
}

/* ---- form parsing ----------------------------------------------------------- */

static int8_t hex_val(char c)
{
    if (c >= '0' && c <= '9') return (int8_t)(c - '0');
    if (c >= 'A' && c <= 'F') return (int8_t)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (int8_t)(c - 'a' + 10);
    return -1;
}

/* value of key in "a=1&b=2", URL-decoded ("%40" -> '@', '+' -> ' ').
 * false if absent. */
static bool form_get(const char *q, const char *key, char *val, uint8_t max)
{
    size_t klen = strlen(key);

    while (q && *q)
    {
        if (strncmp(q, key, klen) == 0 && q[klen] == '=')
        {
            uint8_t n = 0;
            q += klen + 1;
            while (*q && *q != '&' && n < max - 1u)
            {
                char c = *q++;
                if (c == '+')
                {
                    c = ' ';
                }
                else if (c == '%' && hex_val(q[0]) >= 0 && hex_val(q[1]) >= 0)
                {
                    c = (char)((hex_val(q[0]) << 4) | hex_val(q[1]));
                    q += 2;
                }
                val[n++] = c;
            }
            val[n] = '\0';
            return true;
        }
        q = strchr(q, '&');
        if (q)
        {
            q++;
        }
    }
    return false;
}

static bool parse_u32(const char *s, uint32_t max, uint32_t *out_v)
{
    uint32_t v = 0;

    if (*s == '\0')
    {
        return false;
    }
    while (*s)
    {
        if (*s < '0' || *s > '9')
        {
            return false;
        }
        v = v * 10u + (uint32_t)(*s++ - '0');
        if (v > max)
        {
            return false;
        }
    }
    *out_v = v;
    return true;
}

/* "a.b.c.d", each 0..255 */
static bool parse_ip(const char *s, uint8_t ip[4])
{
    char part[4];

    for (uint8_t i = 0; i < 4; i++)
    {
        uint8_t n = 0;
        uint32_t v;

        while (*s >= '0' && *s <= '9' && n < 3u)
        {
            part[n++] = *s++;
        }
        part[n] = '\0';
        if (!parse_u32(part, 255, &v))
        {
            return false;
        }
        ip[i] = (uint8_t)v;
        if (i < 3)
        {
            if (*s++ != '.')
            {
                return false;
            }
        }
    }
    return *s == '\0';
}

/* 1..1..10..0: ones then zeros */
static bool mask_ok(const uint8_t m[4])
{
    uint32_t v = ((uint32_t)m[0] << 24) | ((uint32_t)m[1] << 16) | ((uint32_t)m[2] << 8) | m[3];
    uint32_t inv = ~v;

    return v != 0 && (inv & (inv + 1u)) == 0;
}

/* NULL if fine, otherwise the message for the page */
static const char *parse_settings(const char *body, settings_t *s)
{
    char val[20];
    uint32_t port;

    if (!form_get(body, "ip", val, sizeof(val)) || !parse_ip(val, s->ip) ||
        s->ip[0] == 0 || s->ip[0] == 127 || s->ip[0] >= 224 || s->ip[3] == 0 || s->ip[3] == 255)
    {
        return "Invalid IP address";
    }
    if (!form_get(body, "sn", val, sizeof(val)) || !parse_ip(val, s->subnet) || !mask_ok(s->subnet))
    {
        return "Invalid subnet mask";
    }
    if (!form_get(body, "gw", val, sizeof(val)) || !parse_ip(val, s->gateway))
    {
        return "Invalid gateway";
    }
    if (!form_get(body, "port", val, sizeof(val)) || !parse_u32(val, 65535, &port) || port == 0)
    {
        return "Port must be 1..65535";
    }
    if (port == CFG_HTTP_PORT)
    {
        return "Port 80 is used by this web page";
    }
    if (!form_get(body, "gate", val, sizeof(val)))
    {
        return "Select a gate type";
    }
    if (strcmp(val, "en") == 0)
    {
        s->gate = GATE_ENTRY;
    }
    else if (strcmp(val, "ex") == 0)
    {
        s->gate = GATE_EXIT;
    }
    else
    {
        return "Invalid gate type";
    }
    s->port = (uint16_t)port;
    return NULL;
}

/* ---- login ------------------------------------------------------------------ */

/* compares every byte, so the time taken does not tell how much matched */
static bool str_eq(const char *a, const char *b)
{
    size_t la = strlen(a), lb = strlen(b);
    uint8_t diff = (uint8_t)(la != lb);

    for (size_t i = 0; i < la && i < lb; i++)
    {
        diff |= (uint8_t)(a[i] ^ b[i]);
    }
    return diff == 0;
}

static bool locked(void)
{
    if (s_fails >= MAX_FAILS && (millis() - s_lock_at) < LOCK_MS)
    {
        return true;
    }
    if (s_fails >= MAX_FAILS)
    {
        s_fails = 0;    /* lock time over */
    }
    return false;
}

/* new random-enough token: the cycle counter at the moment of the login
 * request (network timing makes it unpredictable) mixed with the MAC */
static void token_new(void)
{
    uint32_t x = DWT->CYCCNT ^ (millis() * 2654435761u)
               ^ ((uint32_t)g_mac[2] << 24 | (uint32_t)g_mac[3] << 16 | (uint32_t)g_mac[4] << 8 | g_mac[5]);

    for (uint8_t i = 0; i < TOKEN_LEN; i++)
    {
        /* xorshift32 */
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        x ^= DWT->CYCCNT;
        s_token[i] = s_hex[x & 0x0F];
    }
    s_token[TOKEN_LEN] = '\0';
    s_token_at = millis();
}

static bool token_ok(const char *body)
{
    char tok[TOKEN_LEN + 2];

    return s_token[0] != '\0' && (millis() - s_token_at) < TOKEN_TTL_MS &&
           form_get(body, "token", tok, sizeof(tok)) && str_eq(tok, s_token);
}

/* ---- answering -------------------------------------------------------------- */

static void send_page(uint8_t sock, const char *status)
{
    char hdr[128];

    strcpy(hdr, "HTTP/1.1 ");
    strcat(hdr, status);
    strcat(hdr, "\r\nContent-Type: text/html\r\nCache-Control: no-store\r\n"
                "Connection: close\r\n\r\n");
    (void)w5500_send(sock, (const uint8_t *)hdr, (uint16_t)strlen(hdr));
    (void)w5500_send(sock, (const uint8_t *)s_page, s_pos);
}

static void do_login(uint8_t sock, const char *body)
{
    char pw[40];

    if (locked())
    {
        login_page("Too many wrong passwords. Try again after 1 minute.");
    }
    else if (form_get(body, "pw", pw, sizeof(pw)) && str_eq(pw, CFG_WEB_PASSWORD))
    {
        s_fails = 0;
        token_new();
        form_page(&g_settings, NULL);
    }
    else
    {
        if (++s_fails >= MAX_FAILS)
        {
            s_lock_at = millis();
        }
        login_page("Wrong password");
    }
    memset(pw, 0, sizeof(pw));
    send_page(sock, "200 OK");
}

static void do_save(uint8_t sock, const char *body)
{
    settings_t s;
    const char *err;

    if (!token_ok(body))
    {
        login_page("Session expired, please log in again");
        send_page(sock, "200 OK");
        return;
    }

    s = g_settings;
    err = parse_settings(body, &s);
    if (err == NULL && !settings_save(&s))
    {
        err = "EEPROM write failed - settings not saved";
    }
    if (err)
    {
        form_page(&s, err);
        send_page(sock, "200 OK");
        return;
    }

    s_token[0] = '\0';          /* one save per login */
    saved_page(&s);
    send_page(sock, "200 OK");
    s_reboot = true;
    s_reboot_at = millis();
}

static void handle_request(uint8_t sock, request_t *r)
{
    char *path, *end;
    bool post;

    r->body[r->body_len] = '\0';

    /* "<METHOD> <path>[?query] HTTP/1.1" */
    if (strncmp(r->line, "GET ", 4) == 0)
    {
        post = false;
        path = r->line + 4;
    }
    else if (strncmp(r->line, "POST ", 5) == 0)
    {
        post = true;
        path = r->line + 5;
    }
    else
    {
        s_pos = 0;
        out("Method not allowed");
        send_page(sock, "405 Method Not Allowed");
        return;
    }
    end = strpbrk(path, "? ");
    if (end)
    {
        *end = '\0';
    }

    if (!post && strcmp(path, "/") == 0)
    {
        login_page(NULL);
        send_page(sock, "200 OK");
    }
    else if (post && strcmp(path, "/login") == 0)
    {
        do_login(sock, r->body);
    }
    else if (post && strcmp(path, "/save") == 0)
    {
        do_save(sock, r->body);
    }
    else if (!post && (strcmp(path, "/login") == 0 || strcmp(path, "/save") == 0))
    {
        login_page(NULL);       /* bookmarked / refreshed: start at the password */
        send_page(sock, "200 OK");
    }
    else
    {
        s_pos = 0;
        out("Not found");
        send_page(sock, "404 Not Found");
    }
    memset(r->body, 0, sizeof(r->body));   /* do not keep the password around */
}

static bool starts_with_nocase(const char *s, const char *prefix)
{
    while (*prefix)
    {
        char c = *s++;
        if (c >= 'A' && c <= 'Z')
        {
            c = (char)(c - 'A' + 'a');
        }
        if (c != *prefix++)
        {
            return false;
        }
    }
    return true;
}

/* one received byte; returns true when the request is complete */
static bool feed(request_t *r, char c)
{
    switch (r->st)
    {
        case RQ_LINE:
            if (c == '\n')
            {
                r->line[r->line_len] = '\0';
                r->st = RQ_HEADER;
            }
            else if (c != '\r' && r->line_len < LINE_MAX_HTTP - 1u)
            {
                r->line[r->line_len++] = c;
            }
            break;

        case RQ_HEADER:
            if (c == '\n')
            {
                if (r->hdr_len == 0)
                {
                    /* empty line: end of the headers */
                    r->st = (r->content_len > 0) ? RQ_BODY : RQ_DONE;
                    return r->st == RQ_DONE;
                }
                r->hdr[r->hdr_len] = '\0';
                if (starts_with_nocase(r->hdr, "content-length:"))
                {
                    uint32_t v = 0;
                    const char *p = r->hdr + 15;
                    while (*p == ' ')
                    {
                        p++;
                    }
                    while (*p >= '0' && *p <= '9' && v < 65535u)
                    {
                        v = v * 10u + (uint32_t)(*p++ - '0');
                    }
                    r->content_len = (uint16_t)((v > 65535u) ? 65535u : v);
                }
                r->hdr_len = 0;
            }
            else if (c != '\r' && r->hdr_len < HDR_MAX - 1u)
            {
                r->hdr[r->hdr_len++] = c;
            }
            break;

        case RQ_BODY:
            if (r->body_len < BODY_MAX - 1u)
            {
                r->body[r->body_len++] = c;
            }
            if (++r->body_rx >= r->content_len)
            {
                r->st = RQ_DONE;
                return true;
            }
            break;

        case RQ_DONE:
        default:
            break;
    }
    return false;
}

void http_reset(void)
{
    memset(s_req, 0, sizeof(s_req));
}

void http_poll(void)
{
    uint8_t rx[64];
    uint16_t n;

    if (s_reboot && (millis() - s_reboot_at) >= REBOOT_DELAY)
    {
        NVIC_SystemReset();     /* comes back up with the new settings */
    }

    for (uint8_t i = 0; i < HTTP_SOCKET_COUNT; i++)
    {
        uint8_t sock = (uint8_t)(HTTP_FIRST_SOCKET + i);
        request_t *r = &s_req[i];

        w5500_serve(sock, CFG_HTTP_PORT);
        if (!w5500_connected(sock))
        {
            if (r->st != RQ_LINE || r->line_len != 0)
            {
                memset(r, 0, sizeof(*r));   /* next client starts clean */
            }
            continue;
        }

        while ((n = w5500_recv(sock, rx, sizeof(rx))) > 0)
        {
            for (uint16_t k = 0; k < n && r->st != RQ_DONE; k++)
            {
                if (feed(r, (char)rx[k]))
                {
                    handle_request(sock, r);
                    w5500_disconnect(sock);
                }
            }
        }
    }
}
