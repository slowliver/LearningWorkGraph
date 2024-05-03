#pragma once

namespace LearningWorkGraph
{
class Float4
{
public:
	Float4() : m_x(0.0f), m_y(0.0f), m_z(0.0f), m_w(0.0f) {}
	Float4(float x, float y, float z, float w) : m_x(x), m_y(y), m_z(z), m_w(w) {}

public:
	union
	{
		float m_data[4];
		struct
		{
			float m_x;
			float m_y;
			float m_z;
			float m_w;
		};
	};
};
}