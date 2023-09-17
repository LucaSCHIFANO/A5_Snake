#pragma once
#include <iostream>
#include <vector>
#include <SFML/Window.hpp>
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



enum Opcode
{
	OpcodeConnection = 0,
	OpcodeSnakePosition = 1,
	OpcodeApple = 2,
	OpcodeSnakeDeath = 3,
	OpcodeEat = 4,
	OpcodeSnakeBody = 5
};


std::vector<std::uint8_t> SerializeConnection(int clientId, bool connected)
{
	uint16_t size = sizeof(std::uint8_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t);
	std::vector<std::uint8_t> sendBuffer(sizeof(std::uint16_t) + size);
	size = htons(size);

	memcpy(&sendBuffer[0], &size, sizeof(std::uint16_t));
	sendBuffer[sizeof(std::uint16_t)] = OpcodeConnection;
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t)], &clientId, sizeof(std::uint8_t));

	if (connected)
		sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t)] = 0x01; //  <---- potential d'optimiser
	else
		sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t)] = 0x0;

	return sendBuffer;
}

std::vector<std::uint8_t> SerializeSnakeBodyToServer(int toClientId, const std::vector<sf::Vector2i>& body)
{
	uint16_t size = sizeof(std::uint8_t) + sizeof(std::uint8_t) + body.size();
	std::vector<std::uint8_t> sendBuffer(sizeof(std::uint16_t) + size); 
	size = htons(size);

	memcpy(&sendBuffer[0], &size, sizeof(std::uint16_t));
	sendBuffer[sizeof(std::uint16_t)] = OpcodeSnakeBody;
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t)], &toClientId, sizeof(std::uint8_t));

	uint8_t currentSize = sizeof(std::uint16_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t);
	for (size_t i = 0; i < body.size(); i++)
	{
		memcpy(&sendBuffer[size], &body[i].x, body.size());
		memcpy(&sendBuffer[size + sizeof(std::uint8_t)], &body[i].y, body.size());
		size += sizeof(std::uint16_t);
	}

	return sendBuffer;
}

std::vector<std::uint8_t> SerializeSnakeBodyToClient(int clientId, const std::vector<uint8_t>& body)
{
	uint16_t size = sizeof(std::uint8_t) + sizeof(std::uint8_t) + body.size();
	std::vector<std::uint8_t> sendBuffer(sizeof(std::uint16_t) + size); 
	size = htons(size);

	memcpy(&sendBuffer[0], &size, sizeof(std::uint16_t));
	sendBuffer[sizeof(std::uint16_t)] = OpcodeSnakeBody;
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t)], &clientId, sizeof(std::uint8_t));
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t)], &body, sizeof(std::uint8_t));

	return sendBuffer;
}

std::vector<std::uint8_t> SerializeSnakeToServer(sf::Vector2i direction)
{
	//size
	//opcode
	//int verticale
	//horizontal

	std::uint8_t horizontal = direction.x;
	std::uint8_t vertical= direction.y;

	//Send Message
	uint16_t size = sizeof(std::uint8_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t);
	std::vector<std::uint8_t> sendBuffer(sizeof(std::uint16_t) + size);
	size = htons(size);

	memcpy(&sendBuffer[0], &size, sizeof(std::uint16_t));
	sendBuffer[sizeof(std::uint16_t)] = OpcodeSnakePosition;
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t)], &horizontal, sizeof(std::uint8_t));
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t)], &vertical, sizeof(std::uint8_t));


	return sendBuffer;
}

std::vector<std::uint8_t> SerializeSnakeToClient(sf::Vector2i direction, int id)
{
	//size
	//opcode
	//id
	//int verticale
	//horizontal

	std::uint8_t horizontal = direction.x;
	std::uint8_t vertical = direction.y;

	//Send Message
	uint16_t size = sizeof(std::uint8_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t);
	std::vector<std::uint8_t> sendBuffer(sizeof(std::uint16_t) + size);
	size = htons(size);

	memcpy(&sendBuffer[0], &size, sizeof(std::uint16_t));
	sendBuffer[sizeof(std::uint16_t)] = OpcodeSnakePosition;
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t)], &id, sizeof(std::uint8_t));
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t)], &horizontal, sizeof(std::uint8_t));
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t)], &vertical, sizeof(std::uint8_t));


	return sendBuffer;
}

std::vector<std::uint8_t> SerializeDeathToServer()
{
	//size
	//opcode

	//Send Message
	uint16_t size = sizeof(std::uint8_t);
	std::vector<std::uint8_t> sendBuffer(sizeof(std::uint16_t) + size);
	size = htons(size);

	memcpy(&sendBuffer[0], &size, sizeof(std::uint16_t));
	sendBuffer[sizeof(std::uint16_t)] = OpcodeSnakeDeath;

	return sendBuffer;
}

std::vector<std::uint8_t> SerializeDeathToClient(int id)
{
	//size
	//opcode
	//id 
	
	//Send Message
	uint16_t size = sizeof(std::uint8_t) + sizeof(std::uint8_t);
	std::vector<std::uint8_t> sendBuffer(sizeof(std::uint16_t) + size);
	size = htons(size);

	memcpy(&sendBuffer[0], &size, sizeof(std::uint16_t));
	sendBuffer[sizeof(std::uint16_t)] = OpcodeSnakeDeath;
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t)], &id, sizeof(std::uint8_t));

	return sendBuffer;
}

std::vector<std::uint8_t> SerializeAppleToServer(sf::Vector2i position)
{
	//size
	//opcode
	//int verticale
	//horizontal

	std::uint8_t horizontal = position.x;
	std::uint8_t vertical = position.y;

	//Send Message
	uint16_t size = sizeof(std::uint8_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t);
	std::vector<std::uint8_t> sendBuffer(sizeof(std::uint16_t) + size);
	size = htons(size);

	memcpy(&sendBuffer[0], &size, sizeof(std::uint16_t));
	sendBuffer[sizeof(std::uint16_t)] = OpcodeApple;
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t)], &horizontal, sizeof(std::uint8_t));
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t)], &vertical, sizeof(std::uint8_t));


	return sendBuffer;
}

std::vector<std::uint8_t> SerializeAppleToClient(sf::Vector2i position, int id)
{
	//size
	//opcode
	//id
	//int verticale
	//horizontal

	std::uint8_t horizontal = position.x;
	std::uint8_t vertical = position.y;

	//Send Message
	uint16_t size = sizeof(std::uint8_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t);
	std::vector<std::uint8_t> sendBuffer(sizeof(std::uint16_t) + size);
	size = htons(size);

	memcpy(&sendBuffer[0], &size, sizeof(std::uint16_t));
	sendBuffer[sizeof(std::uint16_t)] = OpcodeApple;
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t)], &id, sizeof(std::uint8_t));
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t)], &horizontal, sizeof(std::uint8_t));
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t)], &vertical, sizeof(std::uint8_t));


	return sendBuffer;
}

std::vector<std::uint8_t> SerializeEatToServer(sf::Vector2i position)
{
	//size
	//opcode
	//int verticale
	//horizontal

	std::uint8_t horizontal = position.x;
	std::uint8_t vertical = position.y;

	//Send Message
	uint16_t size = sizeof(std::uint8_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t);
	std::vector<std::uint8_t> sendBuffer(sizeof(std::uint16_t) + size);
	size = htons(size);

	memcpy(&sendBuffer[0], &size, sizeof(std::uint16_t));
	sendBuffer[sizeof(std::uint16_t)] = OpcodeEat;
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t)], &horizontal, sizeof(std::uint8_t));
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t)], &vertical, sizeof(std::uint8_t));


	return sendBuffer;
}

std::vector<std::uint8_t> SerializeEatToClient(sf::Vector2i position, int id)
{
	//size
	//opcode
	//id
	//int verticale
	//horizontal

	std::uint8_t horizontal = position.x;
	std::uint8_t vertical = position.y;

	//Send Message
	uint16_t size = sizeof(std::uint8_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t);
	std::vector<std::uint8_t> sendBuffer(sizeof(std::uint16_t) + size);
	size = htons(size);

	memcpy(&sendBuffer[0], &size, sizeof(std::uint16_t));
	sendBuffer[sizeof(std::uint16_t)] = OpcodeEat;
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t)], &id, sizeof(std::uint8_t));
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t)], &horizontal, sizeof(std::uint8_t));
	memcpy(&sendBuffer[sizeof(std::uint16_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t)], &vertical, sizeof(std::uint8_t));


	return sendBuffer;
}