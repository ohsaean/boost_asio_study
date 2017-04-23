#include "stdafx.h"
#include "ChatServer.h"

const int MAX_SESSION_COUNT = 100;

int main()
{
	boost::asio::io_service ios;

	ChatServer server(ios);
	server.Init(MAX_SESSION_COUNT);
	server.start();

	ios.run();

	std::cout << "��Ʈ��ũ ���� ����" << std::endl;

	return 0;
}
