#include "redis.h"

#include <hiredis/hiredis.h>
#include <stdlib.h>
#include <string.h>

typedef struct redis_conn_ctx {
	char* server;
	unsigned short port;
	char* password;
	redisContext* ctx;
	int is_connected;
} redis_conn_ctx_t;

static redis_conn_ctx_t* redis_ctx = NULL;

static void redis_ctx_free(redis_conn_ctx_t* ctx)
{
	if(ctx->server) {
		free(ctx->server);
		ctx->server = NULL;
	}
	if(ctx->password) {
		free(ctx->password);
		ctx->password = NULL;
	}
	if(ctx->ctx) {
		redisFree(ctx->ctx);
		ctx->ctx = NULL;
	}
	free(ctx);
}

static redis_conn_ctx_t* redis_ctx_new(const char* server, unsigned short port, const char* password)
{
	redis_conn_ctx_t* ctx = malloc(sizeof(redis_conn_ctx_t));
	if(!ctx) {
		return NULL;
	}
	unsigned long server_len = strlen(server);
	unsigned long password_len = password ? strlen(password) : 0;
	ctx->server = malloc(server_len+1);
	ctx->password = password_len ? malloc(password_len+1) : NULL;
	if(!ctx->server || (password_len && !ctx->password)) {
		redis_ctx_free(ctx);
		return NULL;
	}
	strncpy(ctx->server, server, server_len+1);
	if(password_len) {
		strncpy(ctx->password, password, password_len+1);
	}
	ctx->port = port;
	ctx->is_connected = 0;
	return ctx;
}

static int redis_connect(redis_conn_ctx_t* ctx)
{
	struct timeval timeout = {1, 500000};

	if(ctx->is_connected) {
		return 1;
	}

	if(ctx->ctx) {
		redisFree(ctx->ctx);
	}

	ctx->ctx = redisConnectWithTimeout(ctx->server, ctx->port, timeout);

	if(!ctx->ctx || ctx->ctx->err) {
		if(ctx->ctx) {
			// Should log redis conn error
			redisFree(ctx->ctx);
			ctx->ctx = NULL;
		} else {
			// Should log redis ctx allocation failure
		}
		return 0;
	}

	if(ctx->password) {
		redisReply* pwd_reply = redisCommand(ctx->ctx, "AUTH %s", ctx->password);
		if(!pwd_reply || pwd_reply->type == REDIS_REPLY_ERROR) {
			// Should log redis error
			if(pwd_reply) {
				freeReplyObject(pwd_reply);
			}
			redisFree(ctx->ctx);
			ctx->ctx = NULL;
			return 0;
		}
		freeReplyObject(pwd_reply);
	}

	ctx->is_connected = 1;
	return 1;
}

int redis_setup(const char* server, unsigned short port, const char* password)
{
	if(redis_ctx) {
		redis_ctx_free(redis_ctx);
	}
	redis_ctx = redis_ctx_new(server, port, password);
	if(!redis_ctx) {
		return 0;
	}
	return redis_connect(redis_ctx);
}

void redis_set(const char* key, const char* value, int length, long ttl)
{
	if(!redis_is_connected()) {
		return;
	}

	redisReply* reply = NULL;

	if(ttl > 0) {
		reply = redisCommand(redis_ctx->ctx, "SETEX %s %d %b", key, ttl, value, length);
	} else {
		reply = redisCommand(redis_ctx->ctx, "SET %s %b", key, value, length);
	}

	if(reply) {
		freeReplyObject(reply);
	}
}

int redis_get(const char* key, char* buffer, int buffer_length)
{
	if(!redis_is_connected()) {
		return 0;
	}

	int result = 0;

	redisReply* reply = redisCommand(redis_ctx->ctx, "GET %s", key);

	if(reply && reply->type != REDIS_REPLY_ERROR && reply->len < buffer_length) {
		strncpy(reply->str, buffer, reply->len+1);
		result = 1;
	}

	if(reply) {
		freeReplyObject(reply);
	}

	return result;
}

int redis_exists(const char* key)
{
	if(!redis_is_connected()) {
		return 0;
	}

	int result = 0;

	redisReply* reply = redisCommand(redis_ctx->ctx, "EXISTS %s", key);

	if(reply && reply->type == REDIS_REPLY_INTEGER) {
		result = reply->integer;
	}

	if(reply) {
		freeReplyObject(reply);
	}

	return result;
}

int redis_is_connected(void)
{
	return redis_ctx && redis_ctx->is_connected && redis_ctx->ctx;
}