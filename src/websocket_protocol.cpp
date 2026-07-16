#include <websocket_protocol.h>
#include <unistd.h>
#include <cctype>
#include <cstring>

namespace
{
const int websocket_request_capacity = 8192;

static void ws_clear_buffer(char* buffer, int capacity)
{
  if ((buffer != 0) && (capacity > 0))
  {
    buffer[0] = '\0';
  }
}

static int ws_append_text(char* output, int capacity, const char* text)
{
  int length = 0;
  int index = 0;
  if ((output == 0) || (text == 0) || (capacity <= 0))
  {
    return 0;
  }
  length = static_cast<int>(std::strlen(output));
  while ((text[index] != '\0') && (length < (capacity - 1)))
  {
    output[length] = text[index];
    length += 1;
    index += 1;
  }
  output[length] = '\0';
  return length;
}

static void ws_lowercase_text(char* output, const char* input, int capacity)
{
  int index = 0;
  if ((output == 0) || (capacity <= 0))
  {
    return;
  }
  if (input == 0)
  {
    output[0] = '\0';
    return;
  }
  while ((input[index] != '\0') && (index < (capacity - 1)))
  {
    output[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(input[index])));
    index += 1;
  }
  output[index] = '\0';
}

static bool request_header_value(const char* request, const char* name, char* output, int capacity)
{
  char lowered_request[websocket_request_capacity];
  char lowered_name[128];
  const char* found = 0;
  const char* value = 0;
  int header_offset = 0;
  int out = 0;
  ws_clear_buffer(output, capacity);
  if ((request == 0) || (name == 0) || (output == 0) || (capacity <= 0))
  {
    return false;
  }
  ws_lowercase_text(lowered_request, request, websocket_request_capacity);
  ws_lowercase_text(lowered_name, name, 128);
  found = std::strstr(lowered_request, lowered_name);
  if (found == 0) { return false; }
  header_offset = static_cast<int>(found - lowered_request);
  found = std::strchr(&lowered_request[header_offset], ':');
  if (found == 0)
  {
    return false;
  }
  value = &request[header_offset + static_cast<int>(found - &lowered_request[header_offset]) + 1];
  while ((*value == ' ') || (*value == '\t')) { value += 1; }
  while ((*value != '\0') && (*value != '\r') && (*value != '\n') && (out < (capacity - 1)))
  {
    output[out] = *value;
    out += 1;
    value += 1;
  }
  output[out] = '\0';
  return output[0] != '\0';
}

static void base64_encode(const unsigned char* input, int input_length, char* output, int capacity)
{
  static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int index = 0;
  int out = 0;
  if ((input == 0) || (output == 0) || (capacity <= 0))
  {
    return;
  }
  while ((index < input_length) && (out < (capacity - 4)))
  {
    const int remaining = input_length - index;
    const unsigned int a = input[index];
    const unsigned int b = (remaining > 1) ? input[index + 1] : 0U;
    const unsigned int c = (remaining > 2) ? input[index + 2] : 0U;
    const unsigned int triple = (a << 16) | (b << 8) | c;
    output[out] = table[(triple >> 18) & 63U]; out += 1;
    output[out] = table[(triple >> 12) & 63U]; out += 1;
    output[out] = (remaining > 1) ? table[(triple >> 6) & 63U] : '='; out += 1;
    output[out] = (remaining > 2) ? table[triple & 63U] : '='; out += 1;
    index += 3;
  }
  output[out] = '\0';
}

static unsigned int rotate_left(unsigned int value, int bits)
{
  return (value << bits) | (value >> (32 - bits));
}

static void sha1_digest(const unsigned char* input, int input_length, unsigned char* digest)
{
  unsigned int h0 = 0x67452301U;
  unsigned int h1 = 0xEFCDAB89U;
  unsigned int h2 = 0x98BADCFEU;
  unsigned int h3 = 0x10325476U;
  unsigned int h4 = 0xC3D2E1F0U;
  unsigned char block[128];
  unsigned int w[80];
  int total = 0;
  int block_count = 0;
  int block_index = 0;
  int i = 0;
  const unsigned int bit_length_low = static_cast<unsigned int>(input_length) * 8U;
  const unsigned int bit_length_high = 0U;
  std::memset(block, 0, sizeof(block));
  if ((input != 0) && (input_length > 0))
  {
    std::memcpy(block, input, static_cast<unsigned long>(input_length));
  }
  block[input_length] = 0x80U;
  total = input_length + 1;
  while ((total % 64) != 56)
  {
    total += 1;
  }
  block[total] = static_cast<unsigned char>((bit_length_high >> 24) & 255U);
  block[total + 1] = static_cast<unsigned char>((bit_length_high >> 16) & 255U);
  block[total + 2] = static_cast<unsigned char>((bit_length_high >> 8) & 255U);
  block[total + 3] = static_cast<unsigned char>(bit_length_high & 255U);
  block[total + 4] = static_cast<unsigned char>((bit_length_low >> 24) & 255U);
  block[total + 5] = static_cast<unsigned char>((bit_length_low >> 16) & 255U);
  block[total + 6] = static_cast<unsigned char>((bit_length_low >> 8) & 255U);
  block[total + 7] = static_cast<unsigned char>(bit_length_low & 255U);
  total += 8;
  block_count = total / 64;
  while (block_index < block_count)
  {
    unsigned int a = h0;
    unsigned int b = h1;
    unsigned int c = h2;
    unsigned int d = h3;
    unsigned int e = h4;
    i = 0;
    while (i < 16)
    {
      const int offset = (block_index * 64) + (i * 4);
      w[i] =
        (static_cast<unsigned int>(block[offset]) << 24) |
        (static_cast<unsigned int>(block[offset + 1]) << 16) |
        (static_cast<unsigned int>(block[offset + 2]) << 8) |
        static_cast<unsigned int>(block[offset + 3]);
      i += 1;
    }
    while (i < 80)
    {
      w[i] = rotate_left(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
      i += 1;
    }
    i = 0;
    while (i < 80)
    {
      unsigned int f = 0U;
      unsigned int k = 0U;
      unsigned int temp = 0U;
      if (i < 20)
      {
        f = (b & c) | ((~b) & d);
        k = 0x5A827999U;
      }
      else if (i < 40)
      {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1U;
      }
      else if (i < 60)
      {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDCU;
      }
      else
      {
        f = b ^ c ^ d;
        k = 0xCA62C1D6U;
      }
      temp = rotate_left(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = rotate_left(b, 30);
      b = a;
      a = temp;
      i += 1;
    }
    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
    h4 += e;
    block_index += 1;
  }
  digest[0] = static_cast<unsigned char>((h0 >> 24) & 255U);
  digest[1] = static_cast<unsigned char>((h0 >> 16) & 255U);
  digest[2] = static_cast<unsigned char>((h0 >> 8) & 255U);
  digest[3] = static_cast<unsigned char>(h0 & 255U);
  digest[4] = static_cast<unsigned char>((h1 >> 24) & 255U);
  digest[5] = static_cast<unsigned char>((h1 >> 16) & 255U);
  digest[6] = static_cast<unsigned char>((h1 >> 8) & 255U);
  digest[7] = static_cast<unsigned char>(h1 & 255U);
  digest[8] = static_cast<unsigned char>((h2 >> 24) & 255U);
  digest[9] = static_cast<unsigned char>((h2 >> 16) & 255U);
  digest[10] = static_cast<unsigned char>((h2 >> 8) & 255U);
  digest[11] = static_cast<unsigned char>(h2 & 255U);
  digest[12] = static_cast<unsigned char>((h3 >> 24) & 255U);
  digest[13] = static_cast<unsigned char>((h3 >> 16) & 255U);
  digest[14] = static_cast<unsigned char>((h3 >> 8) & 255U);
  digest[15] = static_cast<unsigned char>(h3 & 255U);
  digest[16] = static_cast<unsigned char>((h4 >> 24) & 255U);
  digest[17] = static_cast<unsigned char>((h4 >> 16) & 255U);
  digest[18] = static_cast<unsigned char>((h4 >> 8) & 255U);
  digest[19] = static_cast<unsigned char>(h4 & 255U);
}

static bool websocket_accept_key(const char* key, char* output, int capacity)
{
  char source[256];
  unsigned char digest[20];
  ws_clear_buffer(source, 256);
  if ((key == 0) || (output == 0))
  {
    return false;
  }
  ws_append_text(source, 256, key);
  ws_append_text(source, 256, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
  sha1_digest(reinterpret_cast<const unsigned char*>(source), static_cast<int>(std::strlen(source)), digest);
  base64_encode(digest, 20, output, capacity);
  return output[0] != '\0';
}

static bool read_exact(int fd, unsigned char* output, int length)
{
  int offset = 0;
  while (offset < length)
  {
    const ssize_t count = read(fd, &output[offset], static_cast<unsigned long>(length - offset));
    if (count <= 0)
    {
      return false;
    }
    offset += static_cast<int>(count);
  }
  return true;
}
}

bool websocket_handshake(int fd, const char* request)
{
  char key[128];
  char accept[128];
  char response[512];
  if (!request_header_value(request, "Sec-WebSocket-Key", key, 128))
  {
    return false;
  }
  if (!websocket_accept_key(key, accept, 128))
  {
    return false;
  }
  ws_clear_buffer(response, 512);
  ws_append_text(response, 512, "HTTP/1.1 101 Switching Protocols\r\n");
  ws_append_text(response, 512, "Upgrade: websocket\r\n");
  ws_append_text(response, 512, "Connection: Upgrade\r\n");
  ws_append_text(response, 512, "Sec-WebSocket-Accept: ");
  ws_append_text(response, 512, accept);
  ws_append_text(response, 512, "\r\n\r\n");
  return write(fd, response, std::strlen(response)) > 0;
}

bool websocket_read_text(int fd, char* output, int capacity, int* opcode)
{
  unsigned char header[2];
  unsigned char mask[4];
  int length = 0;
  int index = 0;
  bool masked = false;
  ws_clear_buffer(output, capacity);
  if ((output == 0) || (capacity <= 0) || (opcode == 0))
  {
    return false;
  }
  if (!read_exact(fd, header, 2))
  {
    return false;
  }
  *opcode = header[0] & 15;
  masked = (header[1] & 128U) != 0U;
  length = header[1] & 127;
  if (length == 126)
  {
    unsigned char extended[2];
    if (!read_exact(fd, extended, 2)) { return false; }
    length = (static_cast<int>(extended[0]) << 8) | static_cast<int>(extended[1]);
  }
  else if (length == 127)
  {
    unsigned char extended[8];
    int extended_index = 0;
    if (!read_exact(fd, extended, 8)) { return false; }
    length = 0;
    while (extended_index < 8)
    {
      const int current = static_cast<int>(extended[extended_index]);
      if (length > (((capacity - 1) - current) / 256))
      {
        return false;
      }
      length = (length * 256) + current;
      extended_index += 1;
    }
  }
  if (length >= capacity)
  {
    return false;
  }
  if (masked && !read_exact(fd, mask, 4))
  {
    return false;
  }
  if ((length > 0) &&
      !read_exact(fd, reinterpret_cast<unsigned char*>(output), length))
  {
    return false;
  }
  while (index < length)
  {
    if (masked)
    {
      output[index] = static_cast<char>(
        static_cast<unsigned char>(output[index]) ^ mask[index % 4]);
    }
    index += 1;
  }
  output[index] = '\0';
  return true;
}

bool websocket_send_frame(int fd, int opcode, const char* text)
{
  unsigned char header[4];
  const int length = (text != 0) ? static_cast<int>(std::strlen(text)) : 0;
  int header_length = 0;
  if (length > 65535)
  {
    return false;
  }
  header[0] = static_cast<unsigned char>(128 | opcode);
  if (length < 126)
  {
    header[1] = static_cast<unsigned char>(length);
    header_length = 2;
  }
  else
  {
    header[1] = 126U;
    header[2] = static_cast<unsigned char>((length >> 8) & 255);
    header[3] = static_cast<unsigned char>(length & 255);
    header_length = 4;
  }
  if (write(fd, header, static_cast<unsigned long>(header_length)) <= 0)
  {
    return false;
  }
  if (length > 0)
  {
    return write(fd, text, static_cast<unsigned long>(length)) == length;
  }
  return true;
}
