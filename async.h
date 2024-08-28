#ifndef ASYNC_H
#define ASYNC_H

namespace libasync {
size_t connect(size_t size_blok);
void receive(const char* buff, size_t size_buf, size_t ID,size_t tiks_start);
void disconnect(size_t ID);
}
#endif // ASYNC_H
