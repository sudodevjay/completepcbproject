#include "http_server.h"
#include "socket.h"
#include "wizchip_conf.h"
#include "net_config.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define HTTP_ADMIN_USER  "Houston"
#define HTTP_ADMIN_PASS  "VK@123"

static uint8_t g_authenticated = 0;

static char reqBuf[1024];
static char bodyBuf[3072];

static void close_and_relisten(void);
static void handle_request(uint16_t avail);
static void route_request(char *req, const char *body);
static void handle_login_post(const char *body);
static void handle_settings_post(const char *body);

static void send_http(const char *status, const char *extraHeaders, const char *body);
static void send_redirect(const char *location);
static void send_404(void);
static void send_login_page(uint8_t showError);
static void send_settings_page(const wiz_NetInfo *info, const char *message, uint8_t isError);
static void send_saved_page(void);

static void url_decode(const char *src, char *dst, int dstSize);
static uint8_t get_form_value(const char *body, const char *key, char *out, int outSize);
static uint8_t parse_ipv4(const char *s, uint8_t out[4]);

void HTTPServer_Init(void)
{
	socket(HTTP_SOCK, Sn_MR_TCP, HTTP_PORT, 0);
	listen(HTTP_SOCK);
}

void HTTPServer_Poll(void)
{
	switch (getSn_SR(HTTP_SOCK))
	{
	case SOCK_ESTABLISHED:
		if (getSn_IR(HTTP_SOCK) & Sn_IR_CON)
		{
			setSn_IR(HTTP_SOCK, Sn_IR_CON);
		}
		{
			uint16_t avail = getSn_RX_RSR(HTTP_SOCK);
			if (avail > 0)
			{
				handle_request(avail);
			}
		}
		break;

	case SOCK_CLOSE_WAIT:
		close(HTTP_SOCK);
		socket(HTTP_SOCK, Sn_MR_TCP, HTTP_PORT, 0);
		listen(HTTP_SOCK);
		break;

	case SOCK_CLOSED:
		socket(HTTP_SOCK, Sn_MR_TCP, HTTP_PORT, 0);
		listen(HTTP_SOCK);
		break;

	default:
		break;
	}
}

static void close_and_relisten(void)
{
	close(HTTP_SOCK);
	socket(HTTP_SOCK, Sn_MR_TCP, HTTP_PORT, 0);
	listen(HTTP_SOCK);
}

/* Reads the request (looping briefly if a POST body hasn't fully arrived
 * yet), routes it, sends the response, then closes the connection -
 * every request is its own fresh TCP connection (HTTP/1.0 style). */
static void handle_request(uint16_t avail)
{
	if (avail > sizeof(reqBuf) - 1)
	{
		avail = sizeof(reqBuf) - 1;
	}
	int32_t n = recv(HTTP_SOCK, (uint8_t *)reqBuf, avail);
	if (n <= 0)
	{
		close_and_relisten();
		return;
	}
	int total = n;
	reqBuf[total] = '\0';

	int contentLength = 0;
	char *clPos = strstr(reqBuf, "Content-Length:");
	if (clPos)
	{
		contentLength = atoi(clPos + strlen("Content-Length:"));
	}

	char *bodyStart = strstr(reqBuf, "\r\n\r\n");
	int haveBodyLen = bodyStart ? (int)(total - (bodyStart + 4 - reqBuf)) : 0;

	int retries = 0;
	while (contentLength > 0 && haveBodyLen < contentLength &&
	       retries < 25 && total < (int)sizeof(reqBuf) - 1)
	{
		HAL_Delay(2);
		uint16_t more = getSn_RX_RSR(HTTP_SOCK);
		if (more > 0)
		{
			uint16_t toRead = more;
			if (total + toRead > (int)sizeof(reqBuf) - 1)
			{
				toRead = (uint16_t)(sizeof(reqBuf) - 1 - total);
			}
			int32_t got = recv(HTTP_SOCK, (uint8_t *)(reqBuf + total), toRead);
			if (got > 0)
			{
				total += got;
				reqBuf[total] = '\0';
				bodyStart = strstr(reqBuf, "\r\n\r\n");
				haveBodyLen = bodyStart ? (int)(total - (bodyStart + 4 - reqBuf)) : 0;
			}
		}
		retries++;
	}

	route_request(reqBuf, bodyStart ? bodyStart + 4 : "");
	close_and_relisten();
}

static void route_request(char *req, const char *body)
{
	char method[8] = {0};
	char path[64] = {0};
	sscanf(req, "%7s %63s", method, path);

	char *qmark = strchr(path, '?');
	char query[32] = {0};
	if (qmark)
	{
		strncpy(query, qmark + 1, sizeof(query) - 1);
		*qmark = '\0';
	}

	if (strcmp(method, "GET") == 0)
	{
		if (strcmp(path, "/") == 0 || strcmp(path, "/login") == 0)
		{
			send_login_page(strstr(query, "err=1") != NULL);
		}
		else if (strcmp(path, "/settings") == 0)
		{
			if (!g_authenticated)
			{
				send_redirect("/login");
				return;
			}
			wiz_NetInfo info;
			wizchip_getnetinfo(&info);
			send_settings_page(&info, NULL, 0);
		}
		else if (strcmp(path, "/logout") == 0)
		{
			g_authenticated = 0;
			send_redirect("/login");
		}
		else
		{
			send_404();
		}
	}
	else if (strcmp(method, "POST") == 0)
	{
		if (strcmp(path, "/login") == 0)
		{
			handle_login_post(body);
		}
		else if (strcmp(path, "/settings") == 0)
		{
			if (!g_authenticated)
			{
				send_redirect("/login");
				return;
			}
			handle_settings_post(body);
		}
		else
		{
			send_404();
		}
	}
	else
	{
		send_404();
	}
}

static void handle_login_post(const char *body)
{
	char user[32] = {0};
	char pass[32] = {0};
	get_form_value(body, "username", user, sizeof(user));
	get_form_value(body, "password", pass, sizeof(pass));

	if (strcmp(user, HTTP_ADMIN_USER) == 0 && strcmp(pass, HTTP_ADMIN_PASS) == 0)
	{
		g_authenticated = 1;
		send_redirect("/settings");
	}
	else
	{
		send_redirect("/login?err=1");
	}
}

static void handle_settings_post(const char *body)
{
	char ipStr[32] = {0}, snStr[32] = {0}, gwStr[32] = {0};
	get_form_value(body, "ip", ipStr, sizeof(ipStr));
	get_form_value(body, "sn", snStr, sizeof(snStr));
	get_form_value(body, "gw", gwStr, sizeof(gwStr));

	uint8_t ip[4], sn[4], gw[4];
	uint8_t ok = parse_ipv4(ipStr, ip) && parse_ipv4(snStr, sn) && parse_ipv4(gwStr, gw);
	if (ok && ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) ok = 0;
	if (ok && gw[0] == 0 && gw[1] == 0 && gw[2] == 0 && gw[3] == 0) ok = 0;

	if (!ok)
	{
		wiz_NetInfo current;
		wizchip_getnetinfo(&current);
		send_settings_page(&current,
		                    "Invalid address. Enter valid dotted-decimal IP/Subnet/Gateway values.",
		                    1);
		return;
	}

	wiz_NetInfo newInfo;
	wizchip_getnetinfo(&newInfo); /* keep existing MAC */
	memcpy(newInfo.ip, ip, 4);
	memcpy(newInfo.sn, sn, 4);
	memcpy(newInfo.gw, gw, 4);

	NetConfig_Save(&newInfo);
	send_saved_page();
	HAL_Delay(300);
	NVIC_SystemReset();
}

static void send_http(const char *status, const char *extraHeaders, const char *body)
{
	char header[192];
	int bodyLen = (int)strlen(body);
	int headerLen = snprintf(header, sizeof(header),
	                          "HTTP/1.1 %s\r\n"
	                          "Content-Type: text/html; charset=utf-8\r\n"
	                          "Content-Length: %d\r\n"
	                          "Connection: close\r\n"
	                          "%s"
	                          "\r\n",
	                          status, bodyLen, extraHeaders ? extraHeaders : "");
	send(HTTP_SOCK, (uint8_t *)header, (uint16_t)headerLen);
	if (bodyLen > 0)
	{
		send(HTTP_SOCK, (uint8_t *)body, (uint16_t)bodyLen);
	}
}

static void send_redirect(const char *location)
{
	char extra[96];
	snprintf(extra, sizeof(extra), "Location: %s\r\n", location);
	send_http("302 Found", extra, "");
}

static void send_404(void)
{
	send_http("404 Not Found", NULL, "<h1>404 Not Found</h1>");
}

static void send_login_page(uint8_t showError)
{
	snprintf(bodyBuf, sizeof(bodyBuf),
	         "<!DOCTYPE html><html><head><title>Houston QR Scanner - Login</title>"
	         "<meta name='viewport' content='width=device-width, initial-scale=1'>"
	         "<style>"
	         "body{font-family:Segoe UI,Arial,sans-serif;background:#0f172a;display:flex;"
	         "align-items:center;justify-content:center;height:100vh;margin:0;}"
	         ".card{background:#fff;padding:32px 36px;border-radius:10px;box-shadow:0 10px 30px rgba(0,0,0,.3);width:300px;}"
	         "h1{font-size:20px;margin:0 0 4px;color:#0f172a;}"
	         "p.sub{color:#64748b;font-size:13px;margin:0 0 20px;}"
	         "label{font-size:13px;color:#334155;display:block;margin-bottom:4px;}"
	         "input{width:100%%;padding:9px 10px;margin-bottom:14px;border:1px solid #cbd5e1;border-radius:6px;box-sizing:border-box;font-size:14px;}"
	         "button{width:100%%;padding:10px;background:#2563eb;color:#fff;border:none;border-radius:6px;font-size:14px;cursor:pointer;}"
	         "button:hover{background:#1d4ed8;}"
	         ".err{background:#fef2f2;color:#b91c1c;border:1px solid #fecaca;padding:8px 10px;border-radius:6px;font-size:13px;margin-bottom:14px;}"
	         "</style></head><body>"
	         "<div class='card'>"
	         "<h1>Houston QR Scanner</h1>"
	         "<p class='sub'>Network configuration login</p>"
	         "%s"
	         "<form method='POST' action='/login'>"
	         "<label>Admin username</label>"
	         "<input type='text' name='username' autocomplete='username' required>"
	         "<label>Password</label>"
	         "<input type='password' name='password' autocomplete='current-password' required>"
	         "<button type='submit'>Login</button>"
	         "</form></div></body></html>",
	         showError ? "<div class='err'>Invalid username or password.</div>" : "");
	send_http("200 OK", NULL, bodyBuf);
}

static void send_settings_page(const wiz_NetInfo *info, const char *message, uint8_t isError)
{
	char msgHtml[192];
	if (message)
	{
		snprintf(msgHtml, sizeof(msgHtml), "<div class='%s'>%s</div>", isError ? "err" : "ok", message);
	}
	else
	{
		msgHtml[0] = '\0';
	}

	snprintf(bodyBuf, sizeof(bodyBuf),
	         "<!DOCTYPE html><html><head><title>Houston QR Scanner - Settings</title>"
	         "<meta name='viewport' content='width=device-width, initial-scale=1'>"
	         "<style>"
	         "body{font-family:Segoe UI,Arial,sans-serif;background:#0f172a;display:flex;"
	         "align-items:center;justify-content:center;height:100vh;margin:0;}"
	         ".card{background:#fff;padding:32px 36px;border-radius:10px;box-shadow:0 10px 30px rgba(0,0,0,.3);width:340px;}"
	         "h1{font-size:20px;margin:0 0 4px;color:#0f172a;}"
	         "p.sub{color:#64748b;font-size:13px;margin:0 0 18px;}"
	         "label{font-size:13px;color:#334155;display:block;margin-bottom:4px;}"
	         "input{width:100%%;padding:9px 10px;margin-bottom:14px;border:1px solid #cbd5e1;border-radius:6px;box-sizing:border-box;font-size:14px;}"
	         "button{width:100%%;padding:10px;background:#2563eb;color:#fff;border:none;border-radius:6px;font-size:14px;cursor:pointer;margin-bottom:8px;}"
	         "button:hover{background:#1d4ed8;}"
	         "a.logout{display:block;text-align:center;color:#64748b;font-size:13px;text-decoration:none;}"
	         ".err{background:#fef2f2;color:#b91c1c;border:1px solid #fecaca;padding:8px 10px;border-radius:6px;font-size:13px;margin-bottom:14px;}"
	         ".ok{background:#f0fdf4;color:#15803d;border:1px solid #bbf7d0;padding:8px 10px;border-radius:6px;font-size:13px;margin-bottom:14px;}"
	         "</style></head><body>"
	         "<div class='card'>"
	         "<h1>Network Settings</h1>"
	         "<p class='sub'>MAC %02X:%02X:%02X:%02X:%02X:%02X</p>"
	         "%s"
	         "<form method='POST' action='/settings'>"
	         "<label>IP Address</label>"
	         "<input type='text' name='ip' value='%d.%d.%d.%d' required>"
	         "<label>Subnet Mask</label>"
	         "<input type='text' name='sn' value='%d.%d.%d.%d' required>"
	         "<label>Gateway</label>"
	         "<input type='text' name='gw' value='%d.%d.%d.%d' required>"
	         "<button type='submit'>Save &amp; Reboot</button>"
	         "</form>"
	         "<a class='logout' href='/logout'>Logout</a>"
	         "</div></body></html>",
	         info->mac[0], info->mac[1], info->mac[2], info->mac[3], info->mac[4], info->mac[5],
	         msgHtml,
	         info->ip[0], info->ip[1], info->ip[2], info->ip[3],
	         info->sn[0], info->sn[1], info->sn[2], info->sn[3],
	         info->gw[0], info->gw[1], info->gw[2], info->gw[3]);
	send_http("200 OK", NULL, bodyBuf);
}

static void send_saved_page(void)
{
	snprintf(bodyBuf, sizeof(bodyBuf),
	         "<!DOCTYPE html><html><head><title>Saved</title>"
	         "<meta name='viewport' content='width=device-width, initial-scale=1'>"
	         "<style>body{font-family:Segoe UI,Arial,sans-serif;background:#0f172a;display:flex;"
	         "align-items:center;justify-content:center;height:100vh;margin:0;color:#fff;text-align:center;}"
	         ".card{background:#fff;color:#0f172a;padding:32px 36px;border-radius:10px;"
	         "box-shadow:0 10px 30px rgba(0,0,0,.3);width:300px;}</style></head><body>"
	         "<div class='card'><h1>Saved</h1>"
	         "<p>New network settings saved. The device is rebooting and will "
	         "come up on the new IP address shortly.</p></div></body></html>");
	send_http("200 OK", NULL, bodyBuf);
}

static void url_decode(const char *src, char *dst, int dstSize)
{
	int di = 0;
	for (int i = 0; src[i] != '\0' && di < dstSize - 1; i++)
	{
		if (src[i] == '+')
		{
			dst[di++] = ' ';
		}
		else if (src[i] == '%' && src[i + 1] && src[i + 2])
		{
			char hex[3] = { src[i + 1], src[i + 2], '\0' };
			dst[di++] = (char)strtol(hex, NULL, 16);
			i += 2;
		}
		else
		{
			dst[di++] = src[i];
		}
	}
	dst[di] = '\0';
}

static uint8_t get_form_value(const char *body, const char *key, char *out, int outSize)
{
	out[0] = '\0';
	size_t keyLen = strlen(key);
	const char *p = body;
	while (p && *p)
	{
		if (strncmp(p, key, keyLen) == 0 && p[keyLen] == '=')
		{
			const char *valStart = p + keyLen + 1;
			const char *valEnd = strchr(valStart, '&');
			int rawLen = valEnd ? (int)(valEnd - valStart) : (int)strlen(valStart);
			char raw[64];
			if (rawLen > (int)sizeof(raw) - 1)
			{
				rawLen = sizeof(raw) - 1;
			}
			strncpy(raw, valStart, rawLen);
			raw[rawLen] = '\0';
			url_decode(raw, out, outSize);
			return 1;
		}
		p = strchr(p, '&');
		if (p)
		{
			p++;
		}
	}
	return 0;
}

static uint8_t parse_ipv4(const char *s, uint8_t out[4])
{
	int a, b, c, d;
	char extra;
	int n = sscanf(s, "%d.%d.%d.%d%c", &a, &b, &c, &d, &extra);
	if (n != 4)
	{
		return 0;
	}
	if (a < 0 || a > 255 || b < 0 || b > 255 || c < 0 || c > 255 || d < 0 || d > 255)
	{
		return 0;
	}
	out[0] = (uint8_t)a;
	out[1] = (uint8_t)b;
	out[2] = (uint8_t)c;
	out[3] = (uint8_t)d;
	return 1;
}
