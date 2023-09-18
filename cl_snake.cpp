#include "cl_snake.hpp"
#include "sh_constants.hpp"
#include <cassert>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

// Calcule la rotation à appliquer aux pi�ces du serpent pour l'affichage
float computeRotationFromDirection(const sf::Vector2i& direction)
{
	if (direction.x > 0)
		return 0.f;
	else if (direction.x < 0)
		return 180.f;
	else if (direction.y > 0)
		return 90.f;
	else
		return -90.f;
}

// Calcule la rotation à appliquer aux coins du serpent pour l'affichage
float computeRotationForCorner(const sf::Vector2i& from, const sf::Vector2i& corner, const sf::Vector2i& to)
{
	if (from.x > corner.x)
	{
		if (corner.y > to.y)
			return -90.f;
		else
			return 0.f;
	}
	else if (from.x < corner.x)
	{
		if (corner.y > to.y)
			return 180.f;
		else
			return 90.f;
	}
	else if (from.y > corner.y)
	{
		if (corner.x > to.x)
			return 90.f;
		else
			return 0.f;
	}
	else
	{
		if (corner.x > to.x)
			return 180.f;
		else
			return -90.f;
	}
}

Snake::Snake()
{
	sf::Vector2i spawnPosition(0, 0);
	m_followingDir = sf::Vector2i(0, 1);
	m_color = sf::Color::Red;
	clientId = 0;
	Respawn(spawnPosition, m_followingDir);
}

Snake::Snake(const sf::Vector2i& spawnPosition, const sf::Vector2i& direction, const sf::Color& color, int id, std::string name) :
m_color(color),
m_followingDir(direction),
clientId(id),
m_snakeName(name)
{
	Respawn(spawnPosition, direction);
}

void Snake::Advance()
{
	for (std::size_t i = m_body.size() - 1; i != 0; i--)
	{
		auto& pos = m_body[i];
		pos = m_body[i - 1];
	}

	m_body[0] += m_followingDir;
}

void Snake::Advance(sf::Vector2i position)
{
	for (std::size_t i = m_body.size() - 1; i != 0; i--)
	{
		auto& pos = m_body[i];
		pos = m_body[i - 1];
	}

	m_body[0] = position;
}

void Snake::Draw(sf::RenderTarget& renderTarget, Resources& resources, sf::Font font, sf::Color color) const
{
	for (std::size_t i = 0; i < m_body.size(); ++i)
	{
		const auto& pos = m_body[i];

		float rotation;
		sf::Sprite* sprite;
		if (i == 0)
		{
			rotation = computeRotationFromDirection(GetCurrentDirection());

			sprite = &resources.snakeHead;
		}
		else if (i == m_body.size() - 1)
		{
			sf::Vector2i direction = m_body[i - 1] - m_body[i];
			rotation = computeRotationFromDirection(direction);

			sprite = &resources.snakeTail;
		}
		else
		{
			// D�tection des coins, qui n�cessitent un traitement diff�rent
			sf::Vector2i direction = m_body[i - 1] - m_body[i + 1];
			if (direction.x == 0 || direction.y == 0)
			{
				rotation = computeRotationFromDirection(direction);
				sprite = &resources.snakeBody;
			}
			else
			{
				rotation = computeRotationForCorner(m_body[i - 1], m_body[i], m_body[i + 1]);
				sprite = &resources.snakeBodyCorner;
			}
		}

		sprite->setColor(m_color);
		sprite->setPosition(pos.x * cellSize, pos.y * cellSize);
		sprite->setRotation(rotation);

		renderTarget.draw(*sprite);
	}

	sf::Text text;
	text.setString(m_snakeName);
	text.setFont(font);
	text.setCharacterSize(20);
	text.setFillColor(color);
	
	text.setPosition(GetHeadPosition().x * cellSize - text.getGlobalBounds().width/2, GetHeadPosition().y * cellSize - cellSize * 1.25);
	//text.setPosition(GetHeadPosition().x * cellSize - (text.getCharacterSize()/2) * (text.getString().getSize()/2), GetHeadPosition().y * cellSize - cellSize * 1.25);

	renderTarget.draw(text);
}

const std::vector<sf::Vector2i>& Snake::GetBody() const
{
	return m_body;
}

sf::Vector2i Snake::GetCurrentDirection() const
{
	return m_body[0] - m_body[1];
}

sf::Vector2i Snake::GetFollowingDirection() const
{
	return m_followingDir;
}

sf::Vector2i Snake::GetHeadPosition() const
{
	return m_body[0];
}

void Snake::Grow()
{
	std::size_t lastPartIndex = m_body.size() - 1;
	sf::Vector2i lastPartDirection = m_body[lastPartIndex] - m_body[lastPartIndex - 1];

	m_body.push_back(m_body.back() + lastPartDirection);
}

void Snake::Respawn(const sf::Vector2i& spawnPosition, const sf::Vector2i& direction)
{
	m_body.clear();
	m_body.push_back(spawnPosition);
	m_body.push_back(spawnPosition - direction);
	m_body.push_back(spawnPosition - direction * 2);
	SetFollowingDirection(direction);
}

void Snake::SetBody(const std::vector<sf::Vector2i>& body)
{
	assert(body.size() >= 3);
	m_body = body;
}

void Snake::SetFollowingDirection(const sf::Vector2i& direction)
{
	assert(direction.x == 0 || direction.y == 0);
	m_followingDir = direction;
}

bool Snake::TestCollision(const sf::Vector2i& position, bool testHead)
{
	for (std::size_t i = (testHead) ? 0 : 1; i < m_body.size(); ++i)
	{
		if (m_body[i] == position)
			return true;
	}

	return false;
}

void Snake::ChangeName(std::string name)
{
	m_snakeName = name;
}

int Snake::GetId()
{
	return clientId;
}

void Snake::SetId(int i)
{
	clientId = i;
}
