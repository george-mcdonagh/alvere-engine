#include <algorithm>

#include "alvere/utils/shapes.hpp"

namespace alvere
{
	Rect::Rect()
		: m_x(0), m_y(0), m_width(0), m_height(0)
	{ }

	Rect::Rect(float x, float y, float width, float height)
		: m_x(x), m_y(y), m_width(width), m_height(height)
	{ }

	float Rect::getLeft() const
	{
		return m_x;
	}

	float Rect::getRight() const
	{
		return m_x + m_width;
	}

	float Rect::getTop() const
	{
		return m_y + m_height;
	}

	float Rect::getBottom() const
	{
		return m_y;
	}

	Vector2 Rect::getTopLeft() const
	{
		return alvere::Vector2{ getLeft(), getTop() };
	}

	Vector2 Rect::getBottomLeft() const
	{
		return alvere::Vector2{ getLeft(), getBottom() };
	}

	Vector2 Rect::getTopRight() const
	{
		return alvere::Vector2{ getRight(), getTop() };
	}

	Vector2 Rect::getBottomRight() const
	{
		return alvere::Vector2{ getRight(), getBottom() };
	}

	float Rect::getArea() const
	{
		return m_width * m_height;
	}

	Rect::operator RectI() const
	{
		return RectI{ (int)m_x, (int)m_y, (int)m_width, (int)m_height };
	};

	RectI::RectI()
		: m_x(0), m_y(0), m_width(0), m_height(0)
	{ }

	RectI::RectI(int x, int y, int width, int height)
		: m_x(x), m_y(y), m_width(width), m_height(height)
	{ }

	int RectI::getLeft() const
	{
		return m_x;
	}

	int RectI::getRight() const
	{
		return m_x + m_width;
	}

	int RectI::getTop() const
	{
		return m_y + m_height;
	}

	int RectI::getBottom() const
	{
		return m_y;
	}

	Vector2i RectI::getTopLeft() const
	{
		return alvere::Vector2i{ getLeft(), getTop() };
	}

	Vector2i RectI::getBottomLeft() const
	{
		return alvere::Vector2i{ getLeft(), getBottom() };
	}

	Vector2i RectI::getTopRight() const
	{
		return alvere::Vector2i{ getRight(), getTop() };
	}

	Vector2i RectI::getBottomRight() const
	{
		return alvere::Vector2i{ getRight(), getBottom() };
	}

	RectI RectI::overlap(RectI a, RectI b)
	{
		Vector2i min = Vector2i::max(a.getBottomLeft(), b.getBottomLeft());
		Vector2i max = Vector2i::min(a.getTopRight(), b.getTopRight());
		Vector2i size = Vector2i::max({ 0, 0 }, max - min);

		return { min[0], min[1], size[0], size[1] };
	}

	int RectI::getArea() const
	{
		return m_width * m_height;
	}

	RectI::operator Rect() const
	{
		return Rect{ (float)m_x, (float)m_y, (float)m_width, (float)m_height };
	};
}