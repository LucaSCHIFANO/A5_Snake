#pragma once
#include <iostream>
#include <vector>
#include <winsock2.h> //< Header principal de Winsock
#include <ws2tcpip.h> //< Header pour le modèle TCP/IP, permettant notamment la gestion d'adresses IP

// Ce fichier contient tout ce qui va être lié au protocole du jeu, à la façon dont le client et le serveur vont communiquer

std::vector<std::uint8_t> ReadData(SOCKET sock)
{
	std::vector<std::uint8_t> output(1024);
	int byteRead = recv(sock, reinterpret_cast<char*>(output.data()), static_cast<int>(output.size()), 0);
	if (byteRead == 0 || byteRead == SOCKET_ERROR)
	{
		if (byteRead == 0)
			std::cout << "server closed connection" << std::endl;
		else
			std::cerr << "failed to read data from server (" << WSAGetLastError() << ")" << std::endl;

		throw std::runtime_error("failed to read data");
	}

	output.resize(byteRead);

	return output;
}

void SendData(SOCKET sock, const void* data, std::size_t dataLength)
{
    if (send(sock, static_cast<const char*>(data), static_cast<int>(dataLength), 0) == SOCKET_ERROR)
    {
        std::cerr << "failed to send data to server (" << WSAGetLastError() << ")" << std::endl;
        throw std::runtime_error("failed to send data");
    }
}