#include "stdafx.h"
#include "session.h"
#include "chat_server.h"
#include "util.h"
#include <algorithm>

template<typename T>
inline std::shared_ptr<T> MakeArray(int size)
{
  return std::shared_ptr<T>(new T[size], [](T *p) { delete[] p; });
}

Session::Session(int session_id, boost::asio::io_service& io_service, ChatServer* server)
  : socket_(io_service)
  , session_id_(session_id)
  , server_(server)
{
  logger_ = spdlog::get(CONSOLE);
}

Session::~Session()
{
  while (send_msg_queue_.empty() == false) {
    {
      auto will_delete = std::move(send_msg_queue_.front());
    }
    send_msg_queue_.pop_front();
  }
}

void Session::Init()
{
  packet_buffer_mark_ = 0;
}


void Session::PostRead()
{
  socket_.async_read_some(
    boost::asio::buffer(recv_buffer_),
    boost::bind(
      &Session::HandleRead, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred
    )
  );
}

// �� �޼��带 ���ؼ� Ŭ���̾�Ʈ���� �޽����� ����
void Session::PostWrite(const bool immediately, std::shared_ptr<std::vector<char>> msg)
{
  if (immediately == false) {
    // ��� �������� �ʰ�, ť�� ��
    send_msg_queue_.push_back(msg);
  }

  if (immediately == false && send_msg_queue_.size() >= 2) {
    // ��� �������� �ʾƾ� �ϰ�, ť�� ���ΰ� 2�� �̻��� ���. 
    // ������ ������ ��û�� �� ���� �Ϸ���� �ʾ��� ��� ��ٷ� ������ �ʰ� ������
    // �񵿱� �����⸦ �� �� �Ǽ��ϱ� ���� ������ �ϳ��� ������ ��û�� �� �� �����͸�
    // �ٷ� �����Ͽ� '������ ����' �� �߻��ϴ� �����
    // �ݵ�� ������ �� ������ �������� �����͸� �����ϰ�, �� ���� ���� �����ؾ��� <-- ���� �߿���
    return;
  }

  // ��� ������ ��쳪, ť ����� 1���� ���
  // HandleWrite �� ȣ�� (���� ���� �Ϸ�) �ɶ����� ���� ������ �� ���� �ž� ��
  // async_write_some �� ���� ������ �� �ȵǾ HandleWrite�� �Ҹ� �� �ִ�. (��Ʈ��ũ ȯ���� ���� ������)
  // async_write �� �ݹ� �Լ��� ȣ��Ǿ��ٸ�, ������ 100���� �Ϸ� �� ����.  
  boost::asio::async_write(
    socket_,
    boost::asio::buffer((msg.get())[0], (msg.get())->size()),
    [this](const boost::system::error_code& error, size_t bytes_transferred) // ���ٽ����� ����
  {
    logger_->debug("error : {0} write bytes : {1}", error.value(), bytes_transferred);
    // ���� �����͸� �����Ѵ�. (������ ��쿡�� �� �κп��� �� ����)
    {
      auto will_delete = std::move(send_msg_queue_.front());
    }
    send_msg_queue_.pop_front();

    if (send_msg_queue_.empty() == false)
    {
      // ť�� ���ΰ� ���� ��� (������ �������� ���Ѱ� ���� �ִ� �����)
      // ��� ����
      PostWrite(true, std::move(send_msg_queue_.front()));
    }
  }
  );
}

void Session::HandleRead(const boost::system::error_code& error, size_t byte_transferred)
{
  if (error)
  {
    if (error == boost::asio::error::eof)
    {
      logger_->error("client disconnect");
    }
    else
    {
      logger_->error("read error,  error no : {0}, error message {1}", error.value(), error.message());
    }

    server_->CloseSession(session_id_); // ������ ������ ���� ����
  }
  else
  {
    // ���� �����͸� ��Ŷ ���ۿ� ����
    memcpy(&packet_buffer_[packet_buffer_mark_], recv_buffer_.data(), byte_transferred);

    int recv_packet_data_size = packet_buffer_mark_ + byte_transferred;
    int read_data = 0;

    while (recv_packet_data_size > 0) { // ���� �����͸� ��� ó���� ������ �ݺ�
      if (recv_packet_data_size < PACKET_HEADER_SIZE)
      {
        // ���� �����Ͱ� ��Ŷ ������� ������ �ߴ�
        break;
      }
      //int body_size = *((int*)packet_buffer_[read_data]); // TODO static_cast?
      int body_size = read32_be(&packet_buffer_[read_data]);
      int total_size = (PACKET_HEADER_SIZE + body_size);
      if (total_size <= recv_packet_data_size) {
        // ó���� �� �ִ� ��ŭ �����Ͱ� �ִٸ� ��Ŷ ó��
        server_->ProcessPacket(session_id_, &packet_buffer_[read_data+ PACKET_HEADER_SIZE], body_size);
        recv_packet_data_size -= total_size;
        read_data += total_size;
      }
      else
      {
        // ��Ŷ���� ó���� �� �ִ� ��ŭ�� �ƴϸ� �ߴ�
        break;
      }
    }

    if (recv_packet_data_size > 0) { // ���� �����ʹ� ��Ŷ ���ۿ� ����
      char temp_buffer[MAX_RECEIVE_BUFFER_LEN] = { 0, };
      memcpy(&temp_buffer[0], &packet_buffer_[read_data], recv_packet_data_size);
      memcpy(&packet_buffer_[0], &temp_buffer[0], recv_packet_data_size); // ���� ���� �������� ���
    }

    // ���� ������ ���� �����ϰ� ������ �ޱ� ��û
    packet_buffer_mark_ = recv_packet_data_size;
    PostRead();
  }
}