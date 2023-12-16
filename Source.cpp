#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include <WS2tcpip.h>
#include <Xinput.h>
#include <thread>
#include <vector>
#include <string>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "XInput.lib")

#define info(str) std::cout << "[*] " << str << std::endl
#define error(str) std::cout << "[-] " << str << std::endl

unsigned short port = 5678;

void XboxController(SOCKET* camera, int id)
{
	std::string left =  "\x81\x01\x06\x01\x01\x01\x01\x03\xff";
	std::string right = "\x81\x01\x06\x01\x01\x01\x02\x03\xff";
	const char* stop =  "\x81\x01\x06\x01\x0a\x0a\x03\x03\xff";

	DWORD dwResult = 0;
	bool command_sent = false;
	char recv_input[24];
	int old_speed = 0;
	while (!GetAsyncKeyState(VK_ESCAPE))
	{
		XINPUT_STATE state;
		ZeroMemory(&state, sizeof(XINPUT_STATE));

		dwResult = XInputGetState(id, &state);

		if (dwResult == ERROR_SUCCESS)
		{
			short x = state.Gamepad.sThumbRX;
			short y = state.Gamepad.sThumbRY;
			//printf("\033[A\r\033[K%d, %d\n", x, y);

			int temp = x;
			if (temp < 0)
				temp *= -1;
			if (temp < 5000)
			{
				if (!command_sent)
					continue;

				//printf("Sending stop\n");
				send(*camera, stop, strlen(stop), 0);
				recv(*camera, recv_input, sizeof(recv_input), 0);
				command_sent = false;
				continue;
			}

			unsigned short speed = unsigned short(temp / 16);
			speed /= 160;
			speed *= 0.6;
			if (speed > 16)
				speed = 16;
			// printf("%d\n", speed);
			if (old_speed != speed)
			{
				if (x > 0)
				{
					right[4] = speed;
					//printf("Sending right\n");
					send(*camera, right.c_str(), right.length(), 0);
					recv(*camera, recv_input, sizeof(recv_input), 0);
					right[4] = '\x08';
				}
				else
				{
					left[4] = speed;
					//printf("Sending left\n");
					send(*camera, left.c_str(), left.length(), 0);
					recv(*camera, recv_input, sizeof(recv_input), 0);
					left[4] = '\x08';
				}
				command_sent = true;
				old_speed = speed;
			}
		}
	}
}

void power(SOCKET camera, bool mode)
{
	const char* on =  "\x81\x01\x04\x00\x02\xff";
	const char* off = "\x81\x01\x04\x00\x03\xff";
	short command_size = 6;
	char temp[24];
	if (mode)
		send(camera, on, command_size, 0);
	else
		send(camera, off, command_size, 0);
}

void CameraConnect(std::vector<SOCKET>* cameras, std::vector<const char*> ips)
{
	for (int i = 0; i < ips.size(); i++)
	{
		printf("%s\n", ips[i]);
		SOCKET clientfd = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, 0);
		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		InetPtonA(AF_INET, ips[i], &addr.sin_addr);
		while (connect(clientfd, (sockaddr*)&addr, sizeof(addr)) != 0) {}
		cameras->at(i) = clientfd;
	}
	printf("Cameras connected!\n");
}

int main()
{
	WSAData data;
	WSAStartup(MAKEWORD(2, 2), &data);

	std::vector<const char*> ips = { <IP of camera(s)> };
	std::vector<SOCKET> cameras = {};

	//std::vector<std::thread*> xboxthreads = {};
	for (int i = 0; i < ips.size(); i++)
	{
		SOCKET clientfd = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, 0);
		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		InetPtonA(AF_INET, ips[i], &addr.sin_addr);
		while (connect(clientfd, (sockaddr*)&addr, sizeof(addr)) != 0) {}
		printf("%s connected\n", ips[i]);
		cameras.push_back(clientfd);
	}
	std::thread cameraThread(XboxController, &cameras[0], 0);

	std::string command = "";
	while (true)
	{
		printf("Manager> ");
		std::getline(std::cin, command);

		if (command == "on")
		{
			for (SOCKET sockfd : cameras)
			{
				power(sockfd, true);
				closesocket(sockfd);
			}
			CameraConnect(&cameras, ips);
		}
		else if (command == "off")
		{
			for (SOCKET sockfd : cameras)
			{
				power(sockfd, false);
				closesocket(sockfd);
			}
			CameraConnect(&cameras, ips);
		}
		else if (command == "exit")
			break;
	}
	cameraThread.detach();
	for (SOCKET sockfd : cameras)
		closesocket(sockfd);
	
	return 0;
}