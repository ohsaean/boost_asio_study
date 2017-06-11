#include "util.h"

std::shared_ptr<std::vector<char>> SerializePBMessage(const gs_protocol::Message& msg)
{
  auto msg_size = msg.ByteSize();
  auto buffer = std::make_shared<std::vector<char>>(PACKET_HEADER_SIZE + msg_size); // ����ũ�� ����
  write32_be(&(*buffer)[0], msg_size);
  auto is_serialize_success = msg.SerializeToArray(&(*buffer)[PACKET_HEADER_SIZE], msg_size);

  assert(is_serialize_success == true);

  return buffer; // �˾Ƽ� move ���ְ���?
}

int32_t read32_be(const char* b)
{
  return static_cast<int32_t>(
    (b[3]) |
    (b[2] << 8) |
    (b[1] << 16) |
    (b[0] << 24));
}

void write32_be(char *b, int32_t n)
{
  b[0] = (n >> 24) & 0xFF;
  b[1] = (n >> 16) & 0xFF;
  b[2] = (n >> 8) & 0xFF;
  b[3] = n & 0xFF;
}
