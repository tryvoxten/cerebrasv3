#ifndef voxten_websocket_protocol_h
#define voxten_websocket_protocol_h

bool websocket_handshake(int fd, const char* request);
bool websocket_read_text(int fd, char* output, int capacity, int* opcode);
bool websocket_send_frame(int fd, int opcode, const char* text);

#endif
