#pragma once

class B;

class A
{
public:
	A(B& b)
		: m_bref{ b }
	{
	
	}

	B& m_bref;
};

class B
{
public:
	B() = default;
	int a;
};
