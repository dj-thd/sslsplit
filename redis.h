#ifndef REDIS_H
#define REDIS_H

int redis_setup(const char* server, unsigned short port, const char* password);
void redis_set(const char* key, const char* value, int length, long ttl);
int redis_get(const char* key, char* buffer, int buffer_length);
int redis_exists(const char* key);
int redis_is_connected(void);

#endif